// Included by the native invariant harness after its helpers.
class ActivityOperationProbe : public wxActivityIndicator {
public:
    using wxActivityIndicator::wxActivityIndicator;
    std::function<void()> during_change;
    void Start() override { if (during_change) during_change(); wxActivityIndicator::Start(); }
    void Stop() override { if (during_change) during_change(); wxActivityIndicator::Stop(); }
};
void exercise_activity_indicator() {
    const auto frame=wxl_frame("Activity",320,180), panel=wxl_panel(frame);
    const auto stopped=wxl_activity_indicator(panel,0), running=wxl_activity_indicator(panel,1);
    expect(stopped && running && wxl_activity_running(stopped)==0 && wxl_activity_running(running)==1,"Activity constructor state/readback");
    expect(!wxl_activity_indicator(panel,2) && wxl_set_activity_running(stopped,-1)==-1 &&
           wxl_activity_running(frame)==-1,"Activity raw flag/type checks");
    expect(wxl_set_activity_running(stopped,1)==0 && wxl_set_activity_running(stopped,1)==0 &&
           wxl_activity_running(stopped)==1 && wxl_set_activity_running(stopped,0)==0 &&
           wxl_set_activity_running(stopped,0)==0 && wxl_activity_running(stopped)==0,"Activity start/stop idempotence");
    auto* probe=new ManagedWindow<ActivityOperationProbe>(window(panel),wxID_ANY);
    const auto target=track(probe);
    const auto dialog=wxl_message_dialog(frame,"Activity",8,"Probe",5,0,0);
    unsigned calls=0;
    probe->during_change=[&] {
        ++calls;
        expect(wxl_destroy(target)==-1 && wxl_destroy(panel)==-1 && wxl_close(frame,1)==-1,"Activity operation receiver/ancestor protection");
        expect(wxl_set_activity_running(target,0)==-1,"Activity operation recursive state write rejects");
        int result=-1;
        expect(wxl_poll()==-1 && wxl_run()==-1 && wxl_finish()==-1 && wxl_clipboard_text_available()==-1 &&
               wxl_dialog_show(dialog,&result)==-1,"Activity native-entry guards");
    };
    expect(wxl_set_activity_running(target,1)==0 && wxl_set_activity_running(target,0)==0 &&
           calls==2 && input_operations.empty(),"Activity synchronous native-operation guards unwind");
    probe->during_change={};
    std::atomic<int> foreign_result{99};
    std::thread worker([&] { foreign_result=wxl_activity_running(target); }); worker.join();
    expect(foreign_result==-1,"Activity foreign-thread read rejects before native access");
    expect(wxl_set_activity_running(target,1)==0,"Activity restart before owner teardown");
    delete window(frame);
    expect(wxl_activity_running(stopped)==-1 && wxl_activity_running(running)==-1 &&
           wxl_activity_running(target)==-1 && wxl_set_activity_running(target,0)==-1,"Activity running-owner teardown invalidates handles");
}
