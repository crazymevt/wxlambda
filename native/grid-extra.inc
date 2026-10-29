namespace {
wxGrid* grid_string_table(wxl_handle id) {
    auto* grid=as<wxGrid>(id);
    require(dynamic_cast<wxGridStringTable*>(grid->GetTable()),"Only the owned native string table is supported"); return grid;
}
wxColour grid_extra_colour(int64_t value) {
    require(value>=0 && value<=0xffffff,"Grid colour requires0xRRGGBB");
    return wxColour((value>>16)&255,(value>>8)&255,value&255);
}
int64_t grid_colour_value(const wxColour& colour) {
    require(colour.IsOk(),"Native grid colour is unavailable"); return (colour.Red()<<16)|(colour.Green()<<8)|colour.Blue();
}
void grid_extra_structure(wxGrid* grid) { grid_mutation_ready(grid); require_dispatch_entry(true); }
}
extern "C" {
int wxl_grid_extra_get(wxl_handle id,int property,int row,int column,int64_t* value) { return status([&] {
    auto* grid=grid_string_table(id); require(value && property>=0 && property<=6,"Invalid grid property/output");
    if(property<=2) grid_position(grid,0,row,column);
    switch(property) {
        case 0:*value=grid->IsReadOnly(row,column);break;
        case 1:*value=grid_colour_value(grid->GetCellBackgroundColour(row,column));break;
        case 2:*value=grid_colour_value(grid->GetCellTextColour(row,column));break;
        case 3:*value=grid->IsCellEditControlShown();break;
        case 4:*value=grid->CanEnableCellControl();break;
        case 5:*value=grid->GetGridCursorRow();break;
        case 6:*value=grid->GetGridCursorCol();break;
    }
}); }
int wxl_grid_extra_set(wxl_handle id,int property,int row,int column,int64_t value) { return status([&] {
    auto* grid=grid_string_table(id); grid_position(grid,0,row,column); grid_mutation_ready(grid);
    require(property>=0 && property<=2,"Invalid writable grid property");
    if(property==0) require(value==0 || value==1,"Invalid grid read-only flag"); else grid_extra_colour(value);
    InputOperation operation(grid); wxEventBlocker blocker(grid);
    if(property==0) grid->SetReadOnly(row,column,value!=0);
    else if(property==1) grid->SetCellBackgroundColour(row,column,grid_extra_colour(value));
    else grid->SetCellTextColour(row,column,grid_extra_colour(value));
}); }
int wxl_grid_editor_action(wxl_handle id,int action) { return status([&] {
    auto* grid=grid_string_table(id); require(callback_depth==0,"Defer grid editor operations until callbacks return");
    require(action>=0 && action<=4,"Invalid native grid editor operation"); InputOperation operation(grid);
    if(action==0) {
        require(grid->CanEnableCellControl(),"Current grid cell cannot enable an editor"); grid->EnableCellEditControl();
    } else if(action==1) grid->SaveEditControlValue();
    else if(action==2) grid->HideCellEditControl();
    else if(action==3) grid->DisableCellEditControl();
    else {
        require(grid->IsCellEditControlEnabled(),"Reset requires an enabled native editor");
        const auto row=grid->GetGridCursorRow(),column=grid->GetGridCursorCol(); grid_position(grid,0,row,column);
        // GetCellEditor and native GetCurrentCellEditorPtr resolve the same
        // current-cell attribute. The returned reference is owned locally.
        wxGridCellEditorPtr editor(grid->GetCellEditor(row,column));
        require(editor && editor->IsCreated() && dynamic_cast<wxGridCellTextEditor*>(editor.get()),"Reset requires the active created native text editor");
        editor->Reset();
    }
}); }
int wxl_grid_table_action(wxl_handle id,int axis,int action,int position,int count) { return status([&] {
    auto* grid=grid_string_table(id); grid_extra_structure(grid);
    require((axis==0 || axis==1) && action>=0 && action<=2 && count>=1 && count<=256,"Invalid grid table operation/count");
    const int before=axis ? grid->GetNumberCols() : grid->GetNumberRows();
    if(action==0) position=before;
    require(position>=0 && position<=before && (action!=2 || count<=before-position),"Grid table position/count outside dimension");
    const int after=before+(action==2 ? -count : count);
    const int rows=axis ? grid->GetNumberRows() : after,columns=axis ? after : grid->GetNumberCols();
    require(rows>=0 && rows<=256 && columns>=0 && columns<=256 && rows*columns<=16384,"Grid dimensions exceed supported bounds");
    InputOperation operation(grid);
    const bool result=axis ? (action==0 ? grid->AppendCols(count) : action==1 ? grid->InsertCols(position,count) : grid->DeleteCols(position,count)) :
                             (action==0 ? grid->AppendRows(count) : action==1 ? grid->InsertRows(position,count) : grid->DeleteRows(position,count));
    require(result,"Native string table dimension operation failed");
}); }
int wxl_set_grid_cursor(wxl_handle id,int row,int column) { return status([&] {
    auto* grid=grid_string_table(id); grid_position(grid,0,row,column); grid_mutation_ready(grid);
    InputOperation operation(grid); grid->SetGridCursor(row,column);
}); }
int wxl_grid_select_block(wxl_handle id,int top,int left,int bottom,int right,int add) { return status([&] {
    auto* grid=grid_string_table(id); grid_mutation_ready(grid); grid_position(grid,0,top,left); grid_position(grid,0,bottom,right);
    require(top<=bottom && left<=right && (add==0 || add==1),"Invalid grid selection rectangle");
    InputOperation operation(grid); grid->SelectBlock(top,left,bottom,right,add!=0);
}); }
int wxl_grid_clear_selection(wxl_handle id) { return status([&] {
    auto* grid=grid_string_table(id); grid_mutation_ready(grid); InputOperation operation(grid); grid->ClearSelection();
}); }
int wxl_grid_event_detail(uint64_t token,int field,int64_t* value) { return status([&] {
    auto& event=current_event(token); require(value && grid_detail_valid(grid_kind_for_type(event.GetEventType()),field),"Grid detail is unavailable for this event kind");
    wxl_event snapshot{}; fill_grid_detail(event,snapshot); *value=snapshot.detail[field];
}); }
const char* wxl_grid_event_value(uint64_t token,size_t* length,int* available) { return metadata_string([&] {
    require(available,"Missing grid current-value availability output"); *available=0;
    auto& event=current_event(token); const auto kind=grid_kind_for_type(event.GetEventType());
    require(kind==160 || kind==161,"Current cell value requires grid cell-changing/changed context");
    auto* cell=dynamic_cast<wxGridEvent*>(&event); auto* grid=grid_event_source(event);
    require(cell && grid && !window_destruction_pending(grid),"Grid event source is no longer live");
    const auto row=cell->GetRow(),column=cell->GetCol();
    if(row<0 || row>=grid->GetNumberRows() || column<0 || column>=grid->GetNumberCols()) return wxString();
    *available=1; return grid->GetCellValue(row,column);
},length); }
}
