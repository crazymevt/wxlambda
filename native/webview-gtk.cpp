// Private WebKitGTK adapter. Historical cocoa_* names are an internal ABI only.
// Do not use wxWebView::RunScriptAsync: wx3.2.12 retains a raw wx owner there.
#include "webview-cocoa.h"
#include <wx/platform.h>
#if !defined(__WXGTK3__)
#error Experimental WebKitGTK adapter requires wxGTK3
#endif
#include <webkit2/webkit2.h>
#include <memory>
#include <cstring>
#include <string>
#include <link.h>
#include <pthread.h>
#include <cstdint>

namespace {
constexpr const char* state_key="wxlambda-web-bridge";
struct State { uint64_t owner; gulong download=0; };
struct Request { uint64_t owner, ticket; };
bool valid_url(const char* bytes,size_t length) {
    if (!bytes || !length || std::memchr(bytes,0,length)) return false;
    if (!g_utf8_validate(bytes,length,nullptr)) return false;
    for (const char* p=bytes;p<bytes+length;p=g_utf8_next_char(p)) {
        const gunichar c=g_utf8_get_char(p);
        if (g_unichar_isspace(c) || g_unichar_iscntrl(c)) return false;
    }
    for (size_t i=0;i<length;++i) {
        const auto c=static_cast<unsigned char>(bytes[i]);
        if (c<=32 || c==127) return false;
        if (c=='%') {
            if (i+2>=length || !g_ascii_isxdigit(bytes[i+1]) || !g_ascii_isxdigit(bytes[i+2])) return false;
            i+=2;
        }
    }
    const std::string text(bytes,length);
    if (text=="about:blank") return true;
    GError* error=nullptr;
    GUri* uri=g_uri_parse(text.c_str(),G_URI_FLAGS_NONE,&error);
    if (error) g_error_free(error);
    if (!uri) return false;
    const char* scheme=g_uri_get_scheme(uri);
    const char* host=g_uri_get_host(uri);
    const char* path=g_uri_get_path(uri);
    const bool good=scheme &&
        (((!g_ascii_strcasecmp(scheme,"http") || !g_ascii_strcasecmp(scheme,"https")) && host && *host) ||
         (!g_ascii_strcasecmp(scheme,"file") && path && path[0]=='/'));
    g_uri_unref(uri); return good;
}
void send_json(uint64_t owner,JSCValue* value) {
    if (jsc_value_is_undefined(value) || jsc_value_is_null(value)) {
        wxl_web_message_received(owner,"null",4); return;
    }
    char* json=jsc_value_to_json(value,0);
    if (json) { wxl_web_message_received(owner,json,std::strlen(json)); g_free(json); }
}
void message(WebKitUserContentManager*,WebKitJavascriptResult* result,gpointer data) noexcept {
    try { send_json(static_cast<State*>(data)->owner,webkit_javascript_result_get_js_value(result)); }
    catch (...) {} // No C++ exception may cross a GLib signal boundary.
}
void completed(GObject* source,GAsyncResult* result,gpointer data) noexcept {
    // Source is retained by GIO; only monotonic numeric wx owner identity is saved.
    std::unique_ptr<Request> request(static_cast<Request*>(data));
    GError* error=nullptr;
    WebKitJavascriptResult* output=webkit_web_view_run_javascript_finish(WEBKIT_WEB_VIEW(source),result,&error);
    try {
        if (error) wxl_web_script_completed(request->owner,request->ticket,0,error->message,std::strlen(error->message));
        else if (output) {
            JSCValue* value=webkit_javascript_result_get_js_value(output);
            char* json=(jsc_value_is_undefined(value) || jsc_value_is_null(value)) ? g_strdup("null") : jsc_value_to_json(value,0);
            if (json) { wxl_web_script_completed(request->owner,request->ticket,1,json,std::strlen(json)); g_free(json); }
            else {
                constexpr char failure[]="JavaScript result is not JSON serializable";
                wxl_web_script_completed(request->owner,request->ticket,0,failure,sizeof(failure)-1);
            }
        } else wxl_web_script_completed(request->owner,request->ticket,0,"No JavaScript result",20);
    } catch (...) {}
    if (error) g_error_free(error);
    if (output) webkit_javascript_result_unref(output);
}
gboolean dialog(WebKitWebView*,WebKitScriptDialog* value,gpointer) {
    switch (webkit_script_dialog_get_dialog_type(value)) {
    case WEBKIT_SCRIPT_DIALOG_CONFIRM:
    case WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM:
        webkit_script_dialog_confirm_set_confirmed(value,FALSE); break;
    default: break; // Prompt default is cancellation; alert needs no response.
    }
    return TRUE;
}
gboolean permission(WebKitWebView*,WebKitPermissionRequest* value,gpointer) {
    webkit_permission_request_deny(value); return TRUE;
}
gboolean chooser(WebKitWebView*,WebKitFileChooserRequest* value,gpointer) {
    webkit_file_chooser_request_cancel(value); return TRUE;
}
gboolean authenticate(WebKitWebView*,WebKitAuthenticationRequest* value,gpointer) {
    webkit_authentication_request_cancel(value); return TRUE;
}
gboolean suppress(WebKitWebView*,gpointer,gpointer) { return TRUE; }
gboolean fullscreen(WebKitWebView*,gpointer) { return TRUE; }
GtkWidget* popup(WebKitWebView*,WebKitNavigationAction*,gpointer) { return nullptr; }
void download(WebKitWebContext*,WebKitDownload* value,gpointer data) {
    if (webkit_download_get_web_view(value)==data) webkit_download_cancel(value);
}
void replace_signal(WebKitWebView* view,const char* name,GCallback callback) {
    const guint id=g_signal_lookup(name,WEBKIT_TYPE_WEB_VIEW);
    g_signal_handlers_disconnect_matched(view,G_SIGNAL_MATCH_ID,id,0,nullptr,nullptr,nullptr);
    g_signal_connect(view,name,callback,nullptr);
}
}

extern "C" int wxl_web_backend_compatible(void) {
    // Both libraries can be loaded when wx and this bridge select different
    // pkg-config ABIs. Reject before native browser construction/symbol use.
    unsigned seen=0;
    dl_iterate_phdr([](dl_phdr_info* info,size_t,void* data) {
        auto& mask=*static_cast<unsigned*>(data);
        if (std::strstr(info->dlpi_name,"libwebkit2gtk-4.0.so")) mask|=1;
        if (std::strstr(info->dlpi_name,"libwebkit2gtk-4.1.so")) mask|=2;
        return 0;
    },&seen);
    return seen==WXLAMBDA_EXPECTED_WEBKITGTK_MASK;
}
extern "C" int wxl_web_backend_stack_compatible(void) {
    // JSC uses pthread stack bounds for conservative scanning and validates its
    // saved stack top against them. Lisp-hosted alternate stacks must not enter
    // that VM: rejecting here is safer than a later release assertion/abort.
    volatile char marker=0;
    pthread_attr_t attributes;
    if (pthread_getattr_np(pthread_self(), &attributes)!=0) return 0;
    void* base=nullptr;
    size_t size=0;
    const int result=pthread_attr_getstack(&attributes, &base, &size);
    pthread_attr_destroy(&attributes);
    const auto current=reinterpret_cast<uintptr_t>(&marker);
    const auto low=reinterpret_cast<uintptr_t>(base);
    return result==0 && base && current>=low && current-low<size;
}
extern "C" int wxl_web_cocoa_configure(void* backend,uint64_t owner) {
    auto* view=WEBKIT_WEB_VIEW(backend);
    if (!view || !owner || g_object_get_data(G_OBJECT(view),state_key)) return 0;
    auto state=std::make_unique<State>(); state->owner=owner;
    auto* manager=webkit_web_view_get_user_content_manager(view);
    g_signal_connect(manager,"script-message-received::wxlambda",G_CALLBACK(message),state.get());
    if (!webkit_user_content_manager_register_script_message_handler(manager,"wxlambda")) {
        g_signal_handlers_disconnect_by_data(manager,state.get()); return 0;
    }
    replace_signal(view,"create",G_CALLBACK(popup));
    replace_signal(view,"enter-fullscreen",G_CALLBACK(fullscreen));
    replace_signal(view,"leave-fullscreen",G_CALLBACK(fullscreen));
    g_signal_connect(view,"script-dialog",G_CALLBACK(dialog),nullptr);
    g_signal_connect(view,"permission-request",G_CALLBACK(permission),nullptr);
    g_signal_connect(view,"run-file-chooser",G_CALLBACK(chooser),nullptr);
    g_signal_connect(view,"authenticate",G_CALLBACK(authenticate),nullptr);
    g_signal_connect(view,"print",G_CALLBACK(suppress),nullptr);
    auto* context=webkit_web_view_get_context(view);
    state->download=g_signal_connect(context,"download-started",G_CALLBACK(download),view);
    g_object_set_data_full(G_OBJECT(view),state_key,state.release(),[](gpointer data) { delete static_cast<State*>(data); });
    return 1;
}
extern "C" void wxl_web_cocoa_teardown(void* backend) {
    if (!backend) return;
    auto* view=WEBKIT_WEB_VIEW(backend);
    auto* state=static_cast<State*>(g_object_get_data(G_OBJECT(view),state_key));
    if (!state) return;
    state->owner=0;
    auto* manager=webkit_web_view_get_user_content_manager(view);
    g_signal_handlers_disconnect_by_data(manager,state);
    webkit_user_content_manager_unregister_script_message_handler(manager,"wxlambda");
    if (state->download) g_signal_handler_disconnect(webkit_web_view_get_context(view),state->download);
    g_object_set_data(G_OBJECT(view),state_key,nullptr);
}
extern "C" int wxl_web_cocoa_run_script(void* backend,uint64_t owner,uint64_t ticket,const char* bytes,size_t length) try {
    if (!backend || !bytes || std::memchr(bytes,0,length)) return 0;
    auto* view=WEBKIT_WEB_VIEW(backend);
    auto* state=static_cast<State*>(g_object_get_data(G_OBJECT(view),state_key));
    if (!state || state->owner!=owner) return 0;
    auto request=std::make_unique<Request>(Request{owner,ticket});
    const std::string script(bytes,length);
    webkit_web_view_run_javascript(view,script.c_str(),nullptr,completed,request.release());
    return 1;
} catch (...) { return 0; } // Let the caller retire its reserved request slot.
extern "C" int wxl_web_cocoa_load(void* backend,const char* bytes,size_t length,const char* base,size_t base_length,int html) {
    if (!backend || !bytes || std::memchr(bytes,0,length) || (html!=0 && html!=1)) return 0;
    auto* view=WEBKIT_WEB_VIEW(backend);
    auto* state=static_cast<State*>(g_object_get_data(G_OBJECT(view),state_key));
    if (!state || !state->owner || !valid_url(html ? base : bytes,html ? base_length : length)) return 0;
    const std::string text(bytes,length);
    if (html) { const std::string origin(base,base_length); webkit_web_view_load_html(view,text.c_str(),origin.c_str()); }
    else webkit_web_view_load_uri(view,text.c_str());
    return 1;
}
