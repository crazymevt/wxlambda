const wxEventType stc_event_types[]={wxEVT_STC_MODIFIED,wxEVT_STC_UPDATEUI,wxEVT_STC_STYLENEEDED,
    wxEVT_STC_MARGINCLICK,wxEVT_STC_MARGIN_RIGHT_CLICK,wxEVT_STC_DWELLSTART,wxEVT_STC_DWELLEND,
    wxEVT_STC_SAVEPOINTREACHED,wxEVT_STC_SAVEPOINTLEFT,wxEVT_STC_AUTOCOMP_SELECTION,
    wxEVT_STC_AUTOCOMP_COMPLETED,wxEVT_STC_CALLTIP_CLICK,wxEVT_STC_NEEDSHOWN,wxEVT_STC_ROMODIFYATTEMPT};
bool stc_event_kind(int kind) { return (kind>=31 && kind<=44) || kind==80; }
wxEventType stc_event_type(int kind) { return kind==80 ? static_cast<wxEventType>(wxEVT_STC_CHARADDED) : (stc_event_kind(kind) ? stc_event_types[kind-31] : wxEVT_NULL); }
int stc_kind_for_type(wxEventType type) { if (type==wxEVT_STC_CHARADDED) return 80; for (int kind=31;kind<=44;++kind) if (stc_event_type(kind)==type) return kind; return 0; }
bool stc_payload_readonly(wxEventType type) { return stc_kind_for_type(type)!=0; }
void fill_stc_detail(wxEvent& event,wxl_event& snapshot) {
    if (!stc_payload_readonly(event.GetEventType())) return;
    auto* data=dynamic_cast<wxStyledTextEvent*>(&event); require(data,"Expected native styled text event");
    const int64_t values[]={data->GetPosition(),data->GetLength(),data->GetLinesAdded(),data->GetModificationType(),
        data->GetLine(),data->GetFoldLevelNow(),data->GetFoldLevelPrev(),data->GetMargin(),data->GetModifiers(),
        data->GetX(),data->GetY(),data->GetUpdated(),data->GetKey()};
    std::copy(std::begin(values),std::end(values),snapshot.detail);
}

struct StcCallbackFrame { wxEventType type; wxWindow* source; };
std::vector<StcCallbackFrame> stc_callback_stack;
struct StcCallbackScope {
    explicit StcCallbackScope(wxEvent& event) {
        wxWindow* source=nullptr;
        for (const auto& entry:windows) if (entry.second.get() && entry.second.get()==event.GetEventObject()) { source=entry.second.get(); break; }
        stc_callback_stack.push_back({event.GetEventType(),source});
    }
    ~StcCallbackScope() { stc_callback_stack.pop_back(); }
};
bool stc_synchronous_styling(wxWindow* control) {
    return callback_depth && !stc_callback_stack.empty() && stc_callback_stack.back().type==wxEVT_STC_STYLENEEDED && stc_callback_stack.back().source==control;
}
bool stc_detail_valid(wxEventType type,int field) {
    const auto kind=stc_kind_for_type(type);
    if (field==0) return kind==31 || kind==33 || kind==34 || kind==35 || kind==36 || kind==37 || kind==40 || kind==41 || kind==42 || kind==43;
    if (field==1) return kind==31 || kind==43;
    if (field==2 || field==3 || field==4 || field==5 || field==6) return kind==31;
    if (field==7 || field==8) return kind==34 || kind==35;
    if (field==9 || field==10) return kind==36 || kind==37;
    if (field==11) return kind==32;
    if (field==12) return kind==80;
    return false;
}
