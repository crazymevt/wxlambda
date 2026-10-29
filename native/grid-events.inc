const wxEventType grid_event_types[]={wxEVT_GRID_CELL_CHANGING,wxEVT_GRID_CELL_CHANGED,wxEVT_GRID_SELECT_CELL,
    wxEVT_GRID_RANGE_SELECTING,wxEVT_GRID_RANGE_SELECTED,wxEVT_GRID_EDITOR_SHOWN,wxEVT_GRID_EDITOR_HIDDEN,
    wxEVT_GRID_CELL_LEFT_CLICK,wxEVT_GRID_CELL_RIGHT_CLICK,wxEVT_GRID_CELL_LEFT_DCLICK,wxEVT_GRID_CELL_RIGHT_DCLICK};
bool grid_event_kind(int kind) { return kind>=160 && kind<=170; }
wxEventType grid_event_type(int kind) { return grid_event_kind(kind) ? grid_event_types[kind-160] : wxEVT_NULL; }
int grid_kind_for_type(wxEventType type) { for(int kind=160;kind<=170;++kind) if(grid_event_type(kind)==type) return kind; return 0; }
bool grid_payload_readonly(wxEventType type) { return grid_kind_for_type(type)!=0; }
wxGrid* grid_event_source(wxEvent& event) {
    for(const auto& item:windows) if(auto* object=item.second.get())
        if(object==event.GetEventObject()) return dynamic_cast<wxGrid*>(object);
    return nullptr;
}
bool grid_public_event(wxEvent& event) { auto* grid=grid_event_source(event); return grid && !window_destruction_pending(grid); }
bool grid_detail_valid(int kind,int field) {
    if(!grid_event_kind(kind)) return false;
    const bool range=kind==163 || kind==164;
    if(field==4 || field==5) return true;
    if(field>=0 && field<=3) return !range;
    if(field>=6 && field<=9) return range;
    return false;
}
void fill_grid_detail(wxEvent& event,wxl_event& snapshot) {
    const auto kind=grid_kind_for_type(event.GetEventType()); if(!kind) return;
    if(kind==163 || kind==164) {
        auto* range=dynamic_cast<wxGridRangeSelectEvent*>(&event); require(range,"Expected native grid range event");
        snapshot.detail[4]=range->Selecting(); snapshot.detail[5]=range->GetModifiers();
        snapshot.detail[6]=range->GetTopRow(); snapshot.detail[7]=range->GetLeftCol();
        snapshot.detail[8]=range->GetBottomRow(); snapshot.detail[9]=range->GetRightCol();
    } else {
        auto* cell=dynamic_cast<wxGridEvent*>(&event); require(cell,"Expected native grid cell event");
        snapshot.detail[0]=cell->GetRow(); snapshot.detail[1]=cell->GetCol();
        snapshot.detail[2]=cell->GetPosition().x; snapshot.detail[3]=cell->GetPosition().y;
        snapshot.detail[4]=cell->Selecting(); snapshot.detail[5]=cell->GetModifiers();
    }
}
