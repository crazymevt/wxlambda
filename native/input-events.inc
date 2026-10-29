// Include once with helpers before input callback/native type dispatch, and again
// with WXL_INPUT_EVENT_EXPORTS defined after status()/boolean_result().
#ifndef WXL_INPUT_EVENT_EXPORTS
#ifndef WXL_INPUT_EVENT_HELPERS
#define WXL_INPUT_EVENT_HELPERS
const wxEventType input_event_types[] = {
    wxEVT_KEY_DOWN,wxEVT_KEY_UP,wxEVT_CHAR,wxEVT_CHAR_HOOK,
    wxEVT_LEFT_DOWN,wxEVT_LEFT_UP,wxEVT_LEFT_DCLICK,
    wxEVT_MIDDLE_DOWN,wxEVT_MIDDLE_UP,wxEVT_MIDDLE_DCLICK,
    wxEVT_RIGHT_DOWN,wxEVT_RIGHT_UP,wxEVT_RIGHT_DCLICK,
    wxEVT_AUX1_DOWN,wxEVT_AUX1_UP,wxEVT_AUX1_DCLICK,
    wxEVT_AUX2_DOWN,wxEVT_AUX2_UP,wxEVT_AUX2_DCLICK,
    wxEVT_MOTION,wxEVT_ENTER_WINDOW,wxEVT_LEAVE_WINDOW,wxEVT_MOUSEWHEEL,
    wxEVT_SET_FOCUS,wxEVT_KILL_FOCUS,wxEVT_CONTEXT_MENU,wxEVT_SIZE
};
bool input_event_kind(int kind) { return kind>=45 && kind<=71; }
wxEventType input_event_type(int kind) {
    require(input_event_kind(kind),"Unsupported input event kind"); return input_event_types[kind-45];
}
int input_kind_for_type(wxEventType type) {
    for (size_t i=0;i<sizeof(input_event_types)/sizeof(*input_event_types);++i)
        if (input_event_types[i]==type) return static_cast<int>(i)+45;
    return 0;
}
bool input_payload_readonly(wxEventType type) { return input_kind_for_type(type)!=0; }
bool input_detail_valid(int kind,int index) {
    const bool key=kind>=45 && kind<=48, mouse=kind>=49 && kind<=67;
    if (index==0 || index==1) return key || mouse;
    if (index==2 || index==3) return key;
    if (index==4) return key || mouse;
    if (index==5) return key;
    if (index==23) return mouse;
    if (index==6 || index==7 || index==20) return key || mouse || kind==70;
    if (index==8 || index==9) return key || mouse;
    if (index==21) return mouse;
    if (index>=10 && index<=16) return kind==67;
    if (index==17) return kind==68 || kind==69;
    if (index==18 || index==19) return kind==71;
    if (index==22) return kind==48;
    return false;
}
void fill_input_detail(wxEvent& event,wxl_event& snapshot) {
    const auto kind=input_kind_for_type(event.GetEventType()); if (!kind) return;
    auto* out=snapshot.detail;
    if (kind>=45 && kind<=48) {
        auto* key=dynamic_cast<wxKeyEvent*>(&event); require(key,"Input event requires native wxKeyEvent");
        out[0]=key->GetKeyCode(); out[1]=key->GetUnicodeKey()==WXK_NONE ? -1 : static_cast<int64_t>(key->GetUnicodeKey());
#ifdef wxHAS_RAW_KEY_CODES
        out[2]=key->GetRawKeyCode(); out[3]=key->GetRawKeyFlags();
#else
        out[2]=out[3]=-1;
#endif
        out[4]=key->GetModifiers(); out[5]=key->IsAutoRepeat();
        out[8]=key->CmdDown(); out[9]=key->RawControlDown();
        out[6]=key->GetX(); out[7]=key->GetY(); out[20]=1;
        if (kind==48) out[22]=key->IsNextEventAllowed();
    } else if (kind>=49 && kind<=67) {
        auto* mouse=dynamic_cast<wxMouseEvent*>(&event); require(mouse,"Input event requires native wxMouseEvent");
        out[0]=mouse->CmdDown(); out[1]=mouse->RawControlDown();
        out[4]=mouse->GetModifiers(); out[6]=mouse->GetX(); out[7]=mouse->GetY(); out[20]=1;
        out[8]=mouse->GetButton();
        out[9]=(mouse->LeftIsDown()?1:0)|(mouse->MiddleIsDown()?2:0)|(mouse->RightIsDown()?4:0)|
               (mouse->Aux1IsDown()?8:0)|(mouse->Aux2IsDown()?16:0);
        out[21]=mouse->Dragging();
        out[23]=mouse->GetClickCount();
        if (kind==67) {
            out[10]=mouse->GetWheelRotation(); out[11]=mouse->GetWheelDelta();
            out[12]=mouse->GetWheelAxis()==wxMOUSE_WHEEL_HORIZONTAL ? 1 : 0;
            out[13]=mouse->GetLinesPerAction(); out[14]=mouse->GetColumnsPerAction();
            out[15]=mouse->IsPageScroll(); out[16]=mouse->IsWheelInverted();
        }
    } else if (kind==68 || kind==69) {
        auto* focus=dynamic_cast<wxFocusEvent*>(&event); require(focus,"Input event requires native wxFocusEvent");
        // Compare registered weak identities only; never retain or adopt pointer.
        if (focus->GetWindow()) for (const auto& entry:windows)
            if (entry.second.get()==focus->GetWindow()) { out[17]=static_cast<int64_t>(entry.first); break; }
    } else if (kind==70) {
        auto* context=dynamic_cast<wxContextMenuEvent*>(&event); require(context,"Input event requires native wxContextMenuEvent");
        const auto position=context->GetPosition(); out[6]=position.x; out[7]=position.y;
        out[20]=position!=wxDefaultPosition;
    } else {
        auto* size=dynamic_cast<wxSizeEvent*>(&event); require(size,"Input event requires native wxSizeEvent");
        const auto extent=size->GetSize(); out[18]=extent.x; out[19]=extent.y;
    }
}
int64_t input_event_detail(wxEvent& event,int index) {
    require(input_detail_valid(input_kind_for_type(event.GetEventType()),index),"Input detail is unavailable for this event kind");
    wxl_event snapshot{}; fill_input_detail(event,snapshot); return snapshot.detail[index];
}
#endif
#else
extern "C" {
int wxl_input_detail(uint64_t token,int index,int64_t* value) { return status([&] {
    ready(); require(value,"Missing input event detail output"); *value=input_event_detail(current_event(token),index);
}); }
int wxl_input_allow_next(uint64_t token) { return status([&] {
    ready(); auto& event=current_event(token);
    require(event.GetEventType()==wxEVT_CHAR_HOOK,"Allow-next-key-event requires current CHAR_HOOK context");
    auto* key=dynamic_cast<wxKeyEvent*>(&event); require(key,"Expected native key event"); key->DoAllowNextEvent();
}); }
int wxl_input_next_allowed(uint64_t token) { return boolean_result([&] {
    ready(); return input_event_detail(current_event(token),22)!=0;
}); }
}
#endif
