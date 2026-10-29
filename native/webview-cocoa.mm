#include "webview-cocoa.h"
#import <WebKit/WebKit.h>
#import <objc/runtime.h>

namespace {
char bridgeAssociation;
bool hexDigit(unichar c) {
    return (c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F');
}
NSURL* checkedURL(NSString* text) {
    if (!text.length || [text rangeOfCharacterFromSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]].location!=NSNotFound ||
        [text rangeOfCharacterFromSet:[NSCharacterSet controlCharacterSet]].location!=NSNotFound) return nil;
    for (NSUInteger i=0;i<text.length;++i) if ([text characterAtIndex:i]=='%') {
        if (i+2>=text.length || !hexDigit([text characterAtIndex:i+1]) || !hexDigit([text characterAtIndex:i+2])) return nil;
        i+=2;
    }
    NSURL* url=[NSURL URLWithString:text];
    NSString* scheme=url.scheme.lowercaseString;
    if ([scheme isEqualToString:@"http"] || [scheme isEqualToString:@"https"])
        return url.host.length ? url : nil;
    if ([scheme isEqualToString:@"file"])
        return [url.path hasPrefix:@"/"] ? url : nil;
    if ([text isEqualToString:@"about:blank"]) return url;
    return nil;
}
NSData* jsonData(id value) {
    NSError* error = nil;
    return [NSJSONSerialization dataWithJSONObject:value ?: [NSNull null]
                 options:NSJSONWritingFragmentsAllowed error:&error];
}
void scriptResult(uint64_t owner, uint64_t ticket, int success, NSData* data) {
    if (!data) return;
    try { wxl_web_script_completed(owner,ticket,success,
             static_cast<const char*>(data.bytes),data.length); } catch (...) {}
}
}

// WKWebView retains its script handler; its uiDelegate property is weak.
// The associated object owns this adapter until explicit bridge teardown.
// Only numeric bridge identity is retained, never a C++ wxWindow pointer.
@interface WXLWebDelegate : NSObject<WKUIDelegate,WKScriptMessageHandler>
@property(nonatomic) uint64_t owner;
@end
@implementation WXLWebDelegate
- (WKWebView*)webView:(WKWebView*)view createWebViewWithConfiguration:(WKWebViewConfiguration*)configuration
 forNavigationAction:(WKNavigationAction*)action windowFeatures:(WKWindowFeatures*)features { return nil; }
- (void)webView:(WKWebView*)view runJavaScriptAlertPanelWithMessage:(NSString*)message
 initiatedByFrame:(WKFrameInfo*)frame completionHandler:(void (^)(void))completion { completion(); }
- (void)webView:(WKWebView*)view runJavaScriptConfirmPanelWithMessage:(NSString*)message
 initiatedByFrame:(WKFrameInfo*)frame completionHandler:(void (^)(BOOL))completion { completion(NO); }
- (void)webView:(WKWebView*)view runJavaScriptTextInputPanelWithPrompt:(NSString*)prompt
 defaultText:(NSString*)text initiatedByFrame:(WKFrameInfo*)frame
 completionHandler:(void (^)(NSString*))completion { completion(nil); }
- (void)webView:(WKWebView*)view runOpenPanelWithParameters:(WKOpenPanelParameters*)parameters
 initiatedByFrame:(WKFrameInfo*)frame completionHandler:(void (^)(NSArray<NSURL*>*))completion { completion(nil); }
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 120000
- (void)webView:(WKWebView*)view requestMediaCapturePermissionForOrigin:(WKSecurityOrigin*)origin
 initiatedByFrame:(WKFrameInfo*)frame type:(WKMediaCaptureType)type
 decisionHandler:(void (^)(WKPermissionDecision))decision { decision(WKPermissionDecisionDeny); }
#endif
#if defined(__MAC_27_0) && __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_27_0
- (void)webView:(WKWebView*)view requestGeolocationPermissionForOrigin:(WKSecurityOrigin*)origin
 initiatedByFrame:(WKFrameInfo*)frame decisionHandler:(void (^)(WKPermissionDecision))decision
 { decision(WKPermissionDecisionDeny); }
#endif
// Same optional WebKit selector used by pinned wx3.2.12's UI delegate.
// No public macOS WKUIDelegate printing method is available.
- (void)_webView:(WKWebView*)view printFrame:(WKFrameInfo*)frame {}
- (void)userContentController:(WKUserContentController*)controller didReceiveScriptMessage:(WKScriptMessage*)message {
    @try {
        const uint64_t identifier=self.owner;
        if (!identifier || ![message.name isEqualToString:@"wxlambda"]) return;
        NSData* data=jsonData(message.body);
        if (data) { try { wxl_web_message_received(identifier,static_cast<const char*>(data.bytes),data.length); } catch (...) {} }
    } @catch (NSException* exception) { /* Never unwind through WebKit. */ }
}
@end

extern "C" int wxl_web_cocoa_configure(void* backend,uint64_t owner) {
    @try {
        id candidate=(id)backend;
        if (!owner || ![NSThread isMainThread] || ![candidate isKindOfClass:[WKWebView class]]) return 0;
        WKWebView* view=(WKWebView*)candidate;
        if (objc_getAssociatedObject(view,&bridgeAssociation)) return 0;
        WXLWebDelegate* delegate=[[[WXLWebDelegate alloc] init] autorelease]; delegate.owner=owner;
        // Remove wx's raw-window fullscreen callback and injected fullscreen shim.
        [view.configuration.userContentController removeAllUserScripts];
        [view.configuration.userContentController removeScriptMessageHandlerForName:@"__wxfullscreen"];
        [view.configuration.userContentController removeScriptMessageHandlerForName:@"wxlambda"];
        objc_setAssociatedObject(view,&bridgeAssociation,delegate,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        view.UIDelegate=delegate;
        [view.configuration.userContentController addScriptMessageHandler:delegate name:@"wxlambda"];
        return 1;
    } @catch (NSException* exception) { wxl_web_cocoa_teardown(backend); return 0; }
}
extern "C" void wxl_web_cocoa_teardown(void* backend) {
    @try {
        id candidate=(id)backend;
        if (![NSThread isMainThread] || ![candidate isKindOfClass:[WKWebView class]]) return;
        WKWebView* view=(WKWebView*)candidate;
        WXLWebDelegate* delegate=objc_getAssociatedObject(view,&bridgeAssociation);
        delegate.owner=0;
        [view.configuration.userContentController removeScriptMessageHandlerForName:@"wxlambda"];
        if (view.UIDelegate==delegate) view.UIDelegate=nil;
        objc_setAssociatedObject(view,&bridgeAssociation,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    } @catch (NSException* exception) {}
}
extern "C" int wxl_web_cocoa_run_script(void* backend,uint64_t owner,uint64_t ticket,const char* bytes,size_t length) {
    @try {
        id candidate=(id)backend;
        if (!owner || !ticket || ![NSThread isMainThread] || (!bytes && length) || ![candidate isKindOfClass:[WKWebView class]]) return 0;
        WKWebView* view=(WKWebView*)candidate;
        WXLWebDelegate* delegate=objc_getAssociatedObject(view,&bridgeAssociation);
        if (!delegate || delegate.owner!=owner) return 0;
        NSString* script=[[[NSString alloc] initWithBytes:bytes ?: "" length:length encoding:NSUTF8StringEncoding] autorelease];
        if (!script) return 0;
        [view evaluateJavaScript:script completionHandler:^(id value,NSError* error) {
            @try {
                if (error) scriptResult(owner,ticket,0,[error.localizedDescription dataUsingEncoding:NSUTF8StringEncoding]);
                else {
                    NSData* data=jsonData(value);
                    if (data) scriptResult(owner,ticket,1,data);
                    else scriptResult(owner,ticket,0,[@"JavaScript result is not JSON serializable" dataUsingEncoding:NSUTF8StringEncoding]);
                }
            } @catch (NSException* exception) {
                scriptResult(owner,ticket,0,[@"Native JavaScript result conversion failed" dataUsingEncoding:NSUTF8StringEncoding]);
            }
        }];
        return 1;
    } @catch (NSException* exception) { return 0; }
}
extern "C" int wxl_web_cocoa_load(void* backend,const char* bytes,size_t length,
                                  const char* base,size_t base_length,int html) {
    @try {
        id candidate=(id)backend;
        if (![NSThread isMainThread] || (html!=0 && html!=1) || (!bytes && length) || (!base && base_length) ||
            ![candidate isKindOfClass:[WKWebView class]]) return 0;
        WKWebView* view=(WKWebView*)candidate;
        WXLWebDelegate* delegate=objc_getAssociatedObject(view,&bridgeAssociation);
        if (!delegate.owner) return 0;
        NSString* text=[[[NSString alloc] initWithBytes:bytes ?: "" length:length encoding:NSUTF8StringEncoding] autorelease];
        if (!text) return 0;
        if (html) {
            NSString* baseText=[[[NSString alloc] initWithBytes:base ?: "" length:base_length encoding:NSUTF8StringEncoding] autorelease];
            NSURL* url=checkedURL(baseText);
            if (!url) return 0;
            [view loadHTMLString:text baseURL:url];
        } else {
            NSURL* url=checkedURL(text);
            if (!url) return 0;
            [view loadRequest:[NSURLRequest requestWithURL:url]];
        }
        return 1; // Accepted request; asynchronous completion is independent.
    } @catch (NSException* exception) { return 0; }
}
