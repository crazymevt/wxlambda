namespace {
uint64_t next_web_ticket=1;
class WebContainer : public wxPanel {
public:
    wxWeakRef<wxWebView> browser;
    wxl_handle identifier=0;
    size_t messages=0;
    bool retiring=false;
    std::unordered_map<uint64_t,bool> requests;
    ~WebContainer() override {
        retiring=true;
        web_owners.erase(this);
        if (auto* view=browser.get()) {
            wxl_web_cocoa_teardown(view->GetNativeBackend());
            view->Unbind(wxEVT_WEBVIEW_LOADED,&WebContainer::loaded,this);
            view->Unbind(wxEVT_WEBVIEW_ERROR,&WebContainer::failed,this);
            view->Stop();
        }
        requests.clear();
    }
    bool Create(wxWindow* parent) {
#if defined(__WXOSX__) && wxMAJOR_VERSION==3 && wxMINOR_VERSION==2 && wxRELEASE_NUMBER==12
        if (!wxPanel::Create(parent,wxID_ANY)) return false;
        require(wxWebView::IsBackendAvailable(wxWebViewBackendWebKit),"WKWebView backend is unavailable");
        browser=wxWebView::New(this,wxID_ANY,"about:blank",wxDefaultPosition,wxDefaultSize,wxWebViewBackendWebKit);
        require(browser.get()!=nullptr,"Native WebView creation failed");
        auto* sizer=new wxBoxSizer(wxVERTICAL); sizer->Add(browser.get(),1,wxEXPAND); SetSizer(sizer);
        browser->EnableContextMenu(false);
        return true;
#else
        (void)parent;
        throw std::runtime_error("Interactive WebView currently requires verified macOS wxWidgets3.2.12 WKWebView");
#endif
    }
    void initialise(wxl_handle id) {
        identifier=id;
        require(wxl_web_cocoa_configure(browser->GetNativeBackend(),id)==1,"Cannot configure guarded WKWebView bridge");
        browser->Bind(wxEVT_WEBVIEW_LOADED,&WebContainer::loaded,this);
        browser->Bind(wxEVT_WEBVIEW_ERROR,&WebContainer::failed,this);
        input_owners.emplace(this,false); web_owners.insert(this);
    }
    bool live() const {
        const auto found=input_owners.find(const_cast<WebContainer*>(this));
        return !retiring && browser.get() && found!=input_owners.end() && !found->second && !window_destruction_pending(const_cast<WebContainer*>(this));
    }
    void queue(int kind,const char* bytes,size_t length,uint64_t ticket=0,int success=0) {
        if (!live()) return;
        if (kind==WXL_EVENT_WEB_MESSAGE) {
            if (messages>=256 || length>1048576) return;
            ++messages;
        }
        auto event=std::make_unique<wxCommandEvent>(web_event_types[kind-WXL_EVENT_WEB_MESSAGE],GetId());
        event->SetEventObject(this); event->SetString(wxString::FromUTF8(bytes ? bytes : "",length));
        event->SetInt(success); event->SetExtraLong(static_cast<long>(ticket));
        wxQueueEvent(this,event.release());
    }
    bool ProcessEvent(wxEvent& event) override {
        if (web_event_type(event.GetEventType())) {
            if (!live()) return true;
            if (event.GetEventType()==web_event_types[0] && messages) --messages;
            if (event.GetEventType()==web_event_types[1]) {
                auto* command=dynamic_cast<wxCommandEvent*>(&event);
                if (!command || !requests.erase(static_cast<uint64_t>(command->GetExtraLong()))) return true;
            }
            InputOperation operation(this);
            return wxPanel::ProcessEvent(event);
        }
        return wxPanel::ProcessEvent(event);
    }
    void loaded(wxWebViewEvent& event) {
        const auto bytes=event.GetURL().ToUTF8();
        queue(WXL_EVENT_WEB_LOADED,bytes.data(),bytes.length()); event.Skip();
    }
    void failed(wxWebViewEvent& event) {
        const auto bytes=event.GetString().ToUTF8();
        queue(WXL_EVENT_WEB_ERROR,bytes.data(),bytes.length()); event.Skip();
    }
};
WebContainer* web_container(wxl_handle id) {
    auto* result=as<WebContainer>(id);
    require(result->live(),"WebView backend is no longer live"); return result;
}
void web_mutation_ready(WebContainer* view) {
    require(callback_depth==0,"Defer browser navigation/scripts until callbacks return");
    require(view->live(),"WebView backend is no longer live");
}
wxString web_value(const char* bytes,size_t length,size_t limit) {
    require(length<=limit,"WebView input exceeds supported byte limit"); return text_value(bytes,length);
}
}
extern "C" {
wxl_handle wxl_web_view(wxl_handle parent) { return create([&] {
    auto* owner=window(parent);
    auto result=std::make_unique<ManagedWindow<WebContainer>>();
    require(result->Create(owner),"WebView container creation failed");
    const auto id=track(result.get()); result->initialise(id); result.release(); return id;
}); }
const char* wxl_web_view_string(wxl_handle id,int field,size_t* length) { return metadata_string([&] {
    auto* view=web_container(id); require(field==0 || field==1,"Invalid WebView string field");
    return field==0 ? view->browser->GetCurrentURL() : view->browser->GetCurrentTitle();
},length); }
int wxl_web_view_property(wxl_handle id,int field) { return boolean_result([&] {
    auto* view=web_container(id); require(field>=0 && field<=2,"Invalid WebView property");
    return field==0 ? view->browser->IsBusy() : field==1 ? view->browser->CanGoBack() : view->browser->CanGoForward();
}); }
int wxl_web_view_load(wxl_handle id,const char* bytes,size_t length,const char* base,size_t base_length,int html) { return status([&] {
    auto* view=web_container(id); web_mutation_ready(view); require(html==0 || html==1,"Invalid HTML flag");
    web_value(bytes,length,html ? 2097152 : 8192);
    web_value(base,base_length,8192);
    InputOperation operation(view); wxLogNull silence;
    require(wxl_web_cocoa_load(view->browser->GetNativeBackend(),bytes,length,base,base_length,html)==1,
            "Invalid browser URL/base or native load request failure");
}); }
int wxl_web_view_action(wxl_handle id,int action) { return status([&] {
    auto* view=web_container(id); web_mutation_ready(view); require(action>=0 && action<=3,"Invalid browser action");
    InputOperation operation(view); wxLogNull silence;
    if (action==0) { require(view->browser->CanGoBack(),"No backward history"); view->browser->GoBack(); }
    else if (action==1) { require(view->browser->CanGoForward(),"No forward history"); view->browser->GoForward(); }
    else if (action==2) view->browser->Reload(); else view->browser->Stop();
}); }
int64_t wxl_web_view_script(wxl_handle id,const char* bytes,size_t length) {
    int64_t ticket=-1;
    const auto code=status([&] {
        auto* view=web_container(id); web_mutation_ready(view); web_value(bytes,length,1048576);
        InputOperation operation(view);
        require(view->requests.size()<64,"At most64 outstanding WebView script requests");
        require(next_web_ticket<=2147483647,"WebView script request IDs exhausted");
        ticket=static_cast<int64_t>(next_web_ticket++); view->requests.emplace(ticket,false);
        if (!wxl_web_cocoa_run_script(view->browser->GetNativeBackend(),id,ticket,bytes,length)) {
            view->requests.erase(ticket); throw std::runtime_error("Cannot enqueue JavaScript evaluation");
        }
    }); return code==0 ? ticket : -1;
}
void wxl_web_script_completed(uint64_t owner,uint64_t ticket,int success,const char* bytes,size_t length) {
    try {
        if (!wxIsMainThread() || finishing || !dispatch) return;
        auto* view=web_container(owner); const auto request=view->requests.find(ticket);
        if (request==view->requests.end() || request->second) return;
        request->second=true;
        if (length>1048576) { bytes="JavaScript result exceeds1MiB"; length=std::strlen(bytes); success=0; }
        view->queue(WXL_EVENT_WEB_SCRIPT_RESULT,bytes,length,ticket,success ? 1 : 0);
    } catch (...) {}
}
void wxl_web_message_received(uint64_t owner,const char* bytes,size_t length) {
    try {
        if (!wxIsMainThread() || finishing || !dispatch) return;
        web_container(owner)->queue(WXL_EVENT_WEB_MESSAGE,bytes,length);
    } catch (...) {}
}
}
