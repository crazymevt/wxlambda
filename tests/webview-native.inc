void exercise_web_view() {
    const auto frame=wxl_frame("WebView native",400,300), id=wxl_web_view(frame);
    expect(id && !wxl_web_view(0),"WebView native construction/parent validation");
    auto* view=web_container(id); size_t length=0;
    expect(wxl_web_view_string(id,0,&length) && wxl_web_view_property(id,0)>=0,"WebView getters");
    expect(!wxl_web_view_string(id,3,&length) && wxl_web_view_property(id,3)==-1,"WebView raw property bounds");
    expect(wxl_web_view_load(id,"http://",7,"",0,0)==-1 &&
           wxl_web_view_load(id,"x",1,"relative",8,1)==-1,"WebView raw malformed URL/base reject");
    int foreign=0; std::thread worker([&] { foreign=wxl_web_view_property(id,0); }); worker.join();
    expect(foreign==-1,"WebView foreign thread rejects before native backend access");
    const auto request=uint64_t(1000000); view->requests.emplace(request,false);
    wxl_web_script_completed(id,request,1,"123",3);
    expect(view->requests.at(request),"Web completion marks exactly one queued result");
    wxl_web_script_completed(id,request,1,"456",3);
    for (unsigned i=0;i<300;++i) wxl_web_message_received(id,"{}",2);
    expect(view->messages==256,"Web messages bound pending native queue");
    wxl_web_message_received(id,"x",1048577);
    expect(view->messages==256,"Web messages bound bytes before copying");
    {
        InputOperation operation(view);
        expect(wxl_destroy(id)==-1 && wxl_destroy(frame)==-1 && wxl_close(frame,1)==-1,"Web callback/operation protects receiver and ancestors");
        const auto pending=view->requests.size();
        expect(wxl_poll()==-1 && wxl_web_view_script(id,"1",1)==-1 && view->requests.size()==pending,"Rejected nested script does not leak request slots");
    }
    wxCommandEvent event(web_event_types[1]); event.SetEventObject(view);
    bool rejected=false; try { write_event_field(event,7,0); } catch (...) { rejected=true; }
    expect(rejected,"Web result metadata read-only");
    delete window(frame);
    wxl_web_script_completed(id,request,1,"late",4);
    wxl_web_message_received(id,"late",4);
    expect(wxl_web_view_property(id,0)==-1,"Web owner teardown rejects stale handle and drops late completions");
}
