std::function<void(uint64_t,const wxl_event*)> raw_input_probe;
void raw_input_dispatch(uint64_t token,const wxl_event* event) { if (raw_input_probe) raw_input_probe(token,event); }
class RawRetirementPanel : public wxPanel {
public:
    using wxPanel::wxPanel;
    bool refuse=false,fail=false,after_destroy_notification=false;
    void EmitFocus() {
        wxFocusEvent event(wxEVT_KILL_FOCUS,GetId()); event.SetEventObject(this);
        if (GetParent()) GetParent()->GetEventHandler()->ProcessEvent(event);
    }
    bool Destroy() override {
        EmitFocus();
        if (fail) throw std::runtime_error("Expected raw Destroy failure");
        if (refuse) return false;
        return wxPanel::Destroy();
    }
    ~RawRetirementPanel() override {
        EmitFocus(); // Runs after ManagedWindow's destructor but before wxPanel's.
        if (after_destroy_notification) {
            // Native DESTROY notification can precede the last native input.
            wxWindowDestroyEvent destroyed(this); HandleWindowEvent(destroyed);
            EmitFocus();
        }
    }
};
void exercise_input_events() {
    struct DispatchRestore { wxl_callback old=dispatch; ~DispatchRestore() { dispatch=old; raw_input_probe={}; } } restore;
    dispatch=raw_input_dispatch;
    const auto frame=wxl_frame("Raw input",400,300), panel=wxl_panel(frame);
    { wxKeyEvent key(wxEVT_KEY_DOWN); key.SetControlDown(true); key.SetRawControlDown(false);
      wxl_event snapshot{}; fill_input_detail(key,snapshot);
      expect(snapshot.detail[8]==key.CmdDown() && snapshot.detail[9]==key.RawControlDown(),"Native command/raw Control key distinction");
      wxMouseEvent mouse(wxEVT_MOTION); mouse.SetControlDown(false); mouse.SetRawControlDown(true);
      fill_input_detail(mouse,snapshot);
      expect(snapshot.detail[0]==mouse.CmdDown() && snapshot.detail[1]==mouse.RawControlDown(),"Native command/raw Control mouse distinction"); }
    auto* source=window(panel); uint64_t context=0; int calls=0;
    raw_input_probe=[&](uint64_t,const wxl_event* event) {
        ++calls; context=event->context;
        expect(event->family==WXL_FAMILY_BASE && event->detail[0]==65 && event->detail[1]==0x3bb,"Key copied native/unicode payload and base family");
        int64_t value=0;
        expect(wxl_input_detail(context,0,&value)==0 && value==65,"Current key context detail");
        expect(wxl_input_detail(context,18,&value)==-1 && wxl_input_allow_next(context)==-1,"Wrong-kind input access/hook operation rejects");
        expect(wxl_destroy(panel)==-1 && wxl_destroy(frame)==-1 && wxl_poll()==-1,"Raw input source/ancestor/native-loop guards");
        expect(wxl_context_set_skipped(context,1)==0,"Context Skip preserves normal input");
    };
    expect(wxl_bind(panel,45,970,wxID_ANY,wxID_ANY,1,0)==0,"Native key context binding");
    wxKeyEvent key(wxEVT_KEY_DOWN); key.SetEventObject(source); key.m_keyCode=65; key.m_uniChar=0x3bb;
    key.SetShiftDown(true); source->GetEventHandler()->ProcessEvent(key);
    expect(calls==1 && key.GetSkipped(),"Key dispatch preserves explicit Skip");
    int64_t value=0; expect(wxl_input_detail(context,0,&value)==-1,"Expired key context rejects");
    expect(wxl_unbind(970)==1,"Key context unbind");
    raw_input_probe=[&](uint64_t,const wxl_event* event) { ++calls; expect(event->detail[0]==65,"Observer copied key"); };
    expect(wxl_bind(panel,45,971,wxID_ANY,wxID_ANY,0,0)==0,"Observer key binding");
    key.Skip(false); source->GetEventHandler()->ProcessEvent(key);
    expect(key.GetSkipped(),"Input observers automatically Skip"); expect(wxl_unbind(971)==1,"Observer unbind");
    raw_input_probe=[&](uint64_t,const wxl_event* event) {
        expect(wxl_input_allow_next(event->context)==0 && wxl_input_next_allowed(event->context)==1,"Scoped hook allows normal next key events");
    };
    expect(wxl_bind(panel,48,972,wxID_ANY,wxID_ANY,1,0)==0,"Hook binding");
    wxKeyEvent hook(wxEVT_CHAR_HOOK); hook.SetEventObject(source); source->GetEventHandler()->ProcessEvent(hook);
    expect(hook.IsNextEventAllowed() && !hook.GetSkipped(),"Hook allow-next differs from Skip"); expect(wxl_unbind(972)==1,"Hook unbind");
    auto* private_child=new wxWindow(source,wxID_ANY);
    bool composite_called=false;
    raw_input_probe=[&](uint64_t,const wxl_event*) {
        composite_called=true;
        expect(protected_input_ancestor(source),"Unregistered native child resolves nearest managed input ancestor");
        expect(wxl_destroy(panel)==-1 && wxl_destroy(frame)==-1,"Composite raw input ancestor lifetime guards");
    };
    expect(wxl_bind(panel,48,973,wxID_ANY,wxID_ANY,0,0)==0,"Composite-origin hook binding");
    wxKeyEvent composite(wxEVT_CHAR_HOOK); composite.SetEventObject(private_child);
    source->GetEventHandler()->ProcessEvent(composite);
    expect(composite_called,"Composite raw event callback runs"); expect(wxl_unbind(973)==1,"Composite hook unbind");
    wxMouseEvent wheel(wxEVT_MOUSEWHEEL); wheel.SetEventObject(source); wheel.SetPosition(wxPoint(11,12));
    wheel.m_wheelRotation=-60; wheel.m_wheelDelta=120; wheel.m_wheelAxis=wxMOUSE_WHEEL_HORIZONTAL; wheel.SetLeftDown(true);
    wxl_event copied{}; fill_input_detail(wheel,copied);
    expect(copied.detail[6]==11 && copied.detail[7]==12 && copied.detail[9]==1 && copied.detail[10]==-60 && copied.detail[11]==120 && copied.detail[12]==1,"Wheel precision/axis/position/button payload");
    wxContextMenuEvent menu(wxEVT_CONTEXT_MENU,source->GetId(),wxDefaultPosition); copied={}; fill_input_detail(menu,copied);
    expect(copied.detail[20]==0,"Keyboard context menu sentinel copied");
    wxSizeEvent size(wxSize(123,234)); copied={}; fill_input_detail(size,copied);
    expect(copied.detail[18]==123 && copied.detail[19]==234,"Size copies whole extent");
    wxFocusEvent focus(wxEVT_SET_FOCUS); focus.SetWindow(window(frame)); copied={}; fill_input_detail(focus,copied);
    expect(copied.detail[17]==static_cast<int64_t>(frame),"Focus exposes registered related identity only");
    for (int kind=45;kind<=71;++kind) {
        bool rejected=false; try { synthetic_event(source,kind,wxID_ANY,"",0,0,0,0,0); } catch (...) { rejected=true; }
        expect(rejected,"Native-only raw input cannot be synthesized");
    }
    unsigned teardown_callbacks=0;
    raw_input_probe=[&](uint64_t,const wxl_event*) { ++teardown_callbacks; };
    expect(wxl_bind(panel,69,974,wxID_ANY,wxID_ANY,0,0)==0,"Ordinary panel early focus teardown binding");
    auto* early=new ManagedWindow<RawRetirementPanel>(source,wxID_ANY);
    const auto early_id=track(early);
    expect(!input_owners.count(early),"Ordinary panel does not require a composite input owner");
    early->refuse=true;
    expect(!early->Destroy() && !managed_window_states.at(early).retiring && !teardown_callbacks,
           "Refused ordinary Destroy suppresses early focus and restores retirement state");
    early->EmitFocus(); expect(teardown_callbacks==1,"Refused Destroy restores future ordinary input delivery");
    early->fail=true;
    bool failed=false; try { early->Destroy(); } catch (const std::exception&) { failed=true; }
    expect(failed && !managed_window_states.at(early).retiring && teardown_callbacks==1,
           "Throwing ordinary Destroy restores retirement state");
    early->EmitFocus(); expect(teardown_callbacks==2,"Throwing Destroy restores future input");
    early->refuse=false; early->fail=false;
    delete early;
    expect(teardown_callbacks==2 && wxl_handler_alive(early_id)==0,"Ordinary managed base teardown never publishes raw focus");
    auto* late=new ManagedWindow<RawRetirementPanel>(source,wxID_ANY); track(late);
    late->after_destroy_notification=true; delete late;
    expect(teardown_callbacks==2,"Retirement survives native DESTROY until weak invalidation");
    auto* disabled=new ManagedWindow<wxPanel>(source,wxID_ANY); track(disabled);
    input_owners.emplace(disabled,false); disabled->SetEvtHandlerEnabled(false); delete disabled;
    wxFocusEvent surviving(wxEVT_KILL_FOCUS,source->GetId()); surviving.SetEventObject(source);
    source->GetEventHandler()->ProcessEvent(surviving);
    expect(teardown_callbacks==3 && !input_owners.count(disabled) && !managed_window_states.count(disabled),
           "Disabled DESTROY weak pruning clears stale input owner and permits future input");
    using PlacementPanel=ManagedWindow<wxPanel>;
    std::aligned_storage_t<sizeof(PlacementPanel),alignof(PlacementPanel)> storage;
    auto* reused=::new (&storage) PlacementPanel(source,wxID_ANY); track(reused);
    input_owners.emplace(reused,false); reused->SetEvtHandlerEnabled(false); reused->~PlacementPanel();
    // No intervening input callback prunes the dead address before reuse.
    reused=::new (&storage) PlacementPanel(source,wxID_ANY); track(reused);
    expect(!input_owners.count(reused),"Tracking a reused native address clears its expired composite input owner");
    wxFocusEvent reused_event(wxEVT_KILL_FOCUS,reused->GetId()); reused_event.SetEventObject(reused);
    source->GetEventHandler()->ProcessEvent(reused_event);
    expect(teardown_callbacks==4,"Reused native address cannot leave a permanent input blackhole");
    reused->~PlacementPanel();
    expect(wxl_unbind(974)==1,"Raw teardown binding cleanup");
    delete window(frame);
}
