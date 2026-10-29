std::function<void(const wxl_event*)> grid_extra_probe;
void grid_extra_dispatch(uint64_t,const wxl_event* event) { if(grid_extra_probe) grid_extra_probe(event); }
void exercise_grid_extra() {
    const auto old_dispatch=dispatch; dispatch=grid_extra_dispatch;
    const auto frame=wxl_frame("Grid native edit",400,300), id=wxl_grid(frame,3,3,1);
    auto* grid=as<wxGrid>(id); wxl_show(frame); grid->SetSize(400,300);
    for(int kind: {160,161,165,166}) expect(wxl_bind(id,kind,990+kind-160,wxID_ANY,wxID_ANY,1,0)==0,"Bind grid native event");
    bool veto=false; int changing=0,changed=0,hidden=0; std::string proposed,previous;
    grid_extra_probe=[&](const wxl_event* event) {
        expect(wxl_grid_editor_action(id,1)==-1,"Nested editor save must reject");
        expect(wxl_grid_table_action(id,0,0,0,1)==-1,"Callback structural edit must reject");
        if(event->kind==160 || event->kind==161) {
            size_t length=0; int available=0;
            const auto* value=wxl_grid_event_value(event->context,&length,&available);
            expect(value && available,"Current cell value context");
            if(event->kind==160) { ++changing; proposed.assign(event->text,event->text_length); expect(std::string(value,length)==previous,"CHANGING current table is old value"); }
            else { ++changed; expect(std::string(event->text,event->text_length)==previous,"CHANGED GetString is old value"); }
            if(veto && event->kind==160) expect(wxl_context_set_vetoed(event->context,1)==0,"Native changing veto");
        }
        if(event->kind==166) ++hidden;
    };
    auto set_table=[&](const char* text) { expect(wxl_set_grid_text(id,0,0,0,text,std::strlen(text))==0,"Set table outside active edit"); previous=text; };
    auto edit=[&](const char* text) {
        expect(wxl_grid_editor_action(id,0)==0,"Enable created editor");
        wxGridCellEditorPtr editor(grid->GetCellEditor(0,0));
        auto* control=dynamic_cast<wxTextCtrl*>(editor->GetWindow()); expect(control,"Default native text editor");
        control->ChangeValue(wxString::FromUTF8(text));
    };
    set_table("old λ"); edit("new λ");
    expect(wxl_grid_extra_set(id,0,0,0,1)==-1,"Active-editor attribute edit rejects");
    expect(wxl_grid_editor_action(id,1)==0,"Explicit SaveEditControlValue");
    expect(changing==1 && changed==1 && proposed=="new λ" && grid->GetCellValue(0,0)==wxString::FromUTF8("new λ"),"Save native validation and table update");
    expect(grid->IsCellEditControlEnabled(),"Save leaves editor enabled");
    // Native repeated Save may report again; test only the supported distinction.
    grid_extra_probe={}; expect(wxl_grid_editor_action(id,3)==0,"Disable native commit attempt");
    set_table("cancel baseline"); edit("discard this");
    expect(wxl_grid_editor_action(id,4)==0 && wxl_grid_editor_action(id,3)==0,"Reset before Save then Disable");
    expect(grid->GetCellValue(0,0)=="cancel baseline","Unchanged original editor baseline reset");
    set_table("veto baseline"); edit("rejected"); veto=true;
    grid_extra_probe=[&](const wxl_event* event) { if(event->kind==160) expect(wxl_context_set_vetoed(event->context,1)==0,"Veto native edit"); };
    expect(wxl_grid_editor_action(id,3)==0 && grid->GetCellValue(0,0)=="veto baseline","Changing veto preserves table");
    grid_extra_probe={};
    set_table("hide baseline"); edit("hide retained");
    expect(wxl_grid_editor_action(id,2)==0 && grid->IsCellEditControlEnabled() && !grid->IsCellEditControlShown(),"Hide alone retains enabled editor");
    expect(wxl_grid_editor_action(id,3)==0 && grid->GetCellValue(0,0)=="hide retained","Disable after Hide saves retained editor");
    expect(wxl_grid_table_action(id,0,2,0,3)==0 && wxl_grid_table_action(id,1,2,0,3)==0,"Delete all native table axes");
    expect(wxl_grid_table_action(id,0,0,0,2)==0 && wxl_grid_table_action(id,1,0,0,2)==0,"Restore empty native table");
    expect(wxl_destroy(frame)==0,"Destroy grid native fixture");
    grid_extra_probe={}; dispatch=old_dispatch;
}
