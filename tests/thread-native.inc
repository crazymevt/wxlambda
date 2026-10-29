// Included by the native harness after thread-events.inc implementation.
class ThreadRetirementProbe : public wxPanel {
public:
    using wxPanel::wxPanel;
    bool refuse=true,fail=false;
    bool Destroy() override {
        if (fail) throw std::runtime_error("Expected thread receiver Destroy failure");
        return refuse ? false : wxPanel::Destroy();
    }
};
std::function<void(uint64_t,const wxl_event*)> native_thread_probe;
void native_thread_dispatch(uint64_t token,const wxl_event* event) { if (native_thread_probe) native_thread_probe(token,event); }
void exercise_thread_events() {
    const auto frame=wxl_frame("Thread delivery",400,200), handler=wxl_event_handler();
    auto* target=event_handler(handler);
    thread_endpoint_register(handler,target);
    const bool previous_retirement=thread_endpoint_retire(target);
    expect(wxl_queue_thread_event(handler,"retiring",8)==-1,"Retiring receiver rejects worker posts");
    thread_endpoint_restore(target,previous_retirement);
    auto* retirement=new ManagedWindow<ThreadRetirementProbe>(window(frame),wxID_ANY);
    const auto retirement_id=track(retirement); thread_endpoint_register(retirement_id,retirement);
    expect(!retirement->Destroy() && wxl_queue_thread_event(retirement_id,"restored",8)==0,
           "Refused native Destroy restores worker ingress");
    expect(wxl_delete_pending_events(retirement_id)==0,"Discard refused-Destroy probe packet");
    retirement->fail=true; bool failed=false;
    try { retirement->Destroy(); } catch(const std::exception&) { failed=true; }
    expect(failed && wxl_queue_thread_event(retirement_id,"restored",8)==0,
           "Throwing native Destroy restores worker ingress");
    delete retirement;
    expect(wxl_queue_thread_event(retirement_id,"stale",5)==-1,"Actual native destructor invalidates worker endpoint");
    {
        struct Restore { wxl_callback old=dispatch; ~Restore() { dispatch=old; native_thread_probe={}; } } restore;
        unsigned callbacks=0;
        const std::string callback_value("callback λ\0😀",16);
        native_thread_probe=[&](uint64_t token,const wxl_event* event) {
            if (token!=980) { restore.old(token,event); return; }
            ++callbacks;
            expect(wxIsMainThread() && event->kind==120 && event->family==WXL_FAMILY_BASE &&
                   std::string(event->text,event->text_length)==callback_value,"Native worker snapshot delivered on GUI thread");
            auto* native=dynamic_cast<wxThreadEvent*>(&current_event(event->context));
            expect(native!=nullptr,"Native delivery keeps wxThreadEvent class");
            const wxString native_value=native->GetString(); const auto native_bytes=native_value.ToUTF8();
            expect(std::string(native_bytes.data(),native_bytes.length())==callback_value,"Native wxThreadEvent GetString preserves Unicode/NUL payload");
            const auto reserved=thread_pending.load();
            std::unique_ptr<wxEvent> clone(native->Clone()); auto* native_clone=dynamic_cast<wxThreadEvent*>(clone.get());
            native->SetString("changed by native fixture");
            expect(native_clone && native_clone->GetString()==native_value && thread_pending.load()==reserved,
                   "Inherited native clone has independent string and no duplicated reservation");
            native->SetString(native_value);
            expect(wxl_destroy(frame)==-1 && wxl_poll()==-1,"Thread callback source/native-entry guards");
            expect(wxl_set_event_text(event->context,1,"changed",7)==-1 && !wxl_clone_event(event->context,1),"Thread payload readonly/no clone");
            expect(wxl_context_set_skipped(event->context,1)==0,"Thread context Skip");
        };
        dispatch=native_thread_dispatch;
        expect(wxl_bind(frame,120,980,wxID_ANY,wxID_ANY,1,0)==0,"GUI thread binding establishes endpoint");
        std::thread sender([&] { expect(wxl_queue_thread_event(frame,callback_value.data(),callback_value.size())==0,"Worker packet enqueue"); }); sender.join();
        expect(wxl_poll()==0 && callbacks==1,"Queued worker callback dispatch");
        expect(wxl_unbind(980)==1,"Thread callback cleanup");
    }
    const std::string unicode("λ\0😀",7);
    int worker_result=-1;
    std::thread worker([&] { worker_result=wxl_queue_thread_event(handler,unicode.data(),unicode.size()); }); worker.join();
    expect(worker_result==0 && thread_pending.load()==1,"Worker posts copied Unicode/NUL without GUI-ready access");
    expect(wxl_delete_pending_events(handler)==0 && !thread_pending.load(),"Discard retires thread packet reservation");
    std::atomic<unsigned> accepted{0},full{0};
    std::thread fill([&] {
        for (unsigned i=0;i<257;++i) {
            const auto text=std::to_string(i);
            if (wxl_queue_thread_event(handler,text.data(),text.size())==0) ++accepted; else ++full;
        }
    }); fill.join();
    expect(accepted==256 && full==1,"Per-receiver thread queue bound");
    expect(wxl_queue_thread_event(handler,"x",1048577)==-1 && wxl_queue_thread_event(handler,"\xc0\x80",2)==-1,"Thread payload byte/UTF8 bounds");
    expect(wxl_delete_pending_events(handler)==0 && !thread_pending.load(),"Discard all bounded packets");
    std::vector<wxl_handle> receivers;
    for (unsigned i=0;i<17;++i) { const auto id=wxl_event_handler(); receivers.push_back(id); thread_endpoint_register(id,event_handler(id)); }
    unsigned global_accepted=0;
    std::thread global_fill([&] {
        for (auto id:receivers) for (unsigned i=0;i<256;++i)
            if (wxl_queue_thread_event(id,"g",1)==0) ++global_accepted;
    }); global_fill.join();
    expect(global_accepted==4096 && thread_pending.load()==4096,"Session thread queue bound");
    for (auto id:receivers) expect(wxl_handler_destroy(id)==0,"Destroy globally bounded receiver");
    expect(!thread_pending.load(),"Global queued reservations retire on destruction");
    // Repeated discard concurrently with posting exercises both the endpoint
    // mutex and native pending-registration order; no packet destructor locks.
    std::atomic<bool> entered{false};
    std::thread producer([&] { entered=true; for (unsigned i=0;i<2000;++i) wxl_queue_thread_event(handler,"race",4); });
    while (!entered.load()) std::this_thread::yield();
    for (unsigned i=0;i<50;++i) expect(wxl_delete_pending_events(handler)==0,"Concurrent pending deletion");
    producer.join(); expect(wxl_delete_pending_events(handler)==0,"Final pending deletion");
    expect(!thread_pending.load(),"Concurrent delete/post reservations retire");
    std::atomic<bool> post_started{false};
    std::thread deletion_race([&] { post_started=true; for (unsigned i=0;i<2000;++i) wxl_queue_thread_event(handler,"race",4); });
    while (!post_started.load()) std::this_thread::yield();
    expect(wxl_handler_destroy(handler)==0,"Destroy registered receiver during worker posting");
    deletion_race.join();
    expect(wxl_queue_thread_event(handler,"stale",5)==-1 && !thread_pending.load(),"Stale worker receiver drops without GUI registry access");
    const auto shutdown_handler=wxl_event_handler();
    thread_endpoint_register(shutdown_handler,event_handler(shutdown_handler));
    std::atomic<bool> shutdown_started{false};
    std::thread shutdown_race([&] { shutdown_started=true; for (unsigned i=0;i<2000;++i) wxl_queue_thread_event(shutdown_handler,"race",4); });
    while (!shutdown_started.load()) std::this_thread::yield();
    thread_endpoint_shutdown(); shutdown_race.join();
    expect(wxl_queue_thread_event(shutdown_handler,"closed",6)==-1,"Shutdown rejects worker ingress");
    expect(wxl_handler_destroy(shutdown_handler)==0 && !thread_pending.load(),"Shutdown queued packets retire at receiver destruction");
    // Native fixture resumes ingress only to let the rest of the same harness
    // proceed. Public application sessions remain single-use.
    thread_endpoint_start();
    delete window(frame);
}
