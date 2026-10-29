// Included by the native invariant harness after helpers and grid_probe routing.
class GridTableProbe : public wxGridStringTable {
public:
    GridTableProbe() : wxGridStringTable(2,2) {}
    std::function<void()> during_change;
    void SetValue(int row, int column, const wxString& value) override {
        if (during_change) during_change();
        wxGridStringTable::SetValue(row,column,value);
    }
};
class EarlyGridDestroyProbe : public wxGrid {
public:
    using wxGrid::wxGrid;
    std::function<void()> during_destroy;
    bool refuse=false;
    bool Destroy() override {
        if (during_destroy) during_destroy();
        return refuse ? false : wxGrid::Destroy();
    }
};
void exercise_grid() {
    const auto frame=wxl_frame("Grid",600,400), panel=wxl_panel(frame);
    unsigned calls=0;
    grid_probe=[&](uint64_t,const wxl_event*) { ++calls; };
    expect(wxl_bind(frame,WXL_EVENT_TEXT_CHANGE,940,wxID_ANY,wxID_ANY,1,0)==0,"Observe grid construction");
    const auto id=wxl_grid(panel,3,4,1);
    expect(id && !calls && wxl_grid_property(id,0)==3 && wxl_grid_property(id,1)==4 &&
           wxl_grid_property(id,2)==1 && wxl_grid_property(id,3)==0,"Grid construction dimensions/editing state and silence");
    expect(wxl_unbind(940)==1,"Grid construction unbind");
    expect(!wxl_grid(panel,0,1,1) && !wxl_grid(panel,1,257,1) && !wxl_grid(panel,256,256,1) &&
           !wxl_grid(panel,1,1,2) && wxl_grid_property(frame,0)==-1 && wxl_grid_property(id,4)==-1 &&
           wxl_set_grid_editable(id,-1)==-1,"Grid raw dimensions/flags/type checks");
    size_t length=0;
    expect(wxl_set_grid_text(id,0,2,3,"cell λ",7)==0,"Grid copied UTF8 value");
    auto* text=wxl_grid_text(id,0,2,3,&length);
    expect(text && std::string(text,length)=="cell λ","Grid text length readback");
    expect(wxl_set_grid_text(id,1,2,0,"row λ",6)==0 && wxl_set_grid_text(id,2,0,3,"column λ",9)==0,"Grid label writes");
    text=wxl_grid_text(id,1,2,0,&length); expect(text && std::string(text,length)=="row λ","Grid row label");
    text=wxl_grid_text(id,2,0,3,&length); expect(text && std::string(text,length)=="column λ","Grid column label");
    expect(!wxl_grid_text(id,0,3,0,&length) && !wxl_grid_text(id,3,0,0,&length) && !wxl_grid_text(id,0,0,0,nullptr) &&
           wxl_set_grid_text(id,0,0,4,"",0)==-1 && wxl_set_grid_text(id,0,0,0,"a\0b",3)==-1 &&
           wxl_set_grid_text(id,0,0,0,"\xff",1)==-1 && wxl_set_grid_text(id,0,0,0,nullptr,1)==-1,"Grid raw text/index checks");
    const std::string oversized(65537,'x');
    expect(wxl_set_grid_text(id,0,0,0,oversized.data(),oversized.size())==-1,"Grid native byte limit");
    expect(wxl_set_grid_editable(id,0)==0 && wxl_grid_property(id,2)==0 &&
           wxl_set_grid_text(id,0,0,0,"programmatic",12)==0 && wxl_set_grid_editable(id,1)==0,"Read-only grid allows programmatic text");
    auto* grid=as<wxGrid>(id);
    window(panel)->SetSize(600,400); grid->SetSize(580,340); wxl_show(frame); wxl_poll();
    grid->SetGridCursor(0,0); grid->EnableCellEditControl();
    expect(grid->IsCellEditControlEnabled() && wxl_grid_property(id,3)==1,"Native editor activation readback");
    expect(wxl_set_grid_text(id,0,0,0,"replace",7)==-1 && wxl_set_grid_text(id,1,0,0,"row",3)==-1 &&
           wxl_set_grid_editable(id,0)==-1,"Active native editor rejects scalar writes without implicit commit");
    text=wxl_grid_text(id,0,0,0,&length); expect(text && std::string(text,length)=="programmatic","Active editor getter reads committed table");
    auto* editor=grid->GetCellEditor(0,0); auto* control=editor->GetControl();
    expect(control!=nullptr,"Native string editor created");
    auto* entry=dynamic_cast<wxTextCtrl*>(control);
    expect(entry!=nullptr,"Default grid string editor uses a native text control");
    entry->ChangeValue("edited λ");
    text=wxl_grid_text(id,0,0,0,&length);
    expect(text && std::string(text,length)=="programmatic","Pending editor buffer is distinct from committed table");
    const auto dialog=wxl_message_dialog(frame,"Grid",4,"Probe",5,0,0);
    grid_probe=[&](uint64_t,const wxl_event*) {
        ++calls;
        expect(wxl_destroy(id)==-1 && wxl_destroy(panel)==-1 && wxl_close(frame,1)==-1,"Grid native child callback lifetime guard");
        expect(wxl_set_grid_text(id,0,0,0,"nested",6)==-1 && wxl_set_grid_editable(id,0)==-1,"Grid child callback mutation guard");
        int result=-1;
        expect(wxl_poll()==-1 && wxl_clipboard_text_available()==-1 && wxl_dialog_show(dialog,&result)==-1,"Grid child native-entry guards");
    };
    expect(wxl_bind(frame,WXL_EVENT_TEXT_CHANGE,940,wxID_ANY,wxID_ANY,1,0)==0,"Grid private editor TEXT bind");
    wxCommandEvent input(wxEVT_TEXT,control->GetId()); input.SetEventObject(control); input.SetString("pending");
    control->HandleWindowEvent(input);
    expect(calls==1 && wxl_unbind(940)==1 && input_operations.empty(),"Grid private editor propagated TEXT guard unwinds");
    editor->DecRef(); grid->DisableCellEditControl();
    expect(wxl_grid_property(id,3)==0,"Native editor completion readback");
    text=wxl_grid_text(id,0,0,0,&length);
    expect(text && std::string(text,length)=="edited λ","Native editor commit reaches copied table readback");
    // An arbitrary private button child exercises the same composite origin rule.
    const auto child=wxl_button(id,"child");
    expect(wxl_bind(frame,WXL_EVENT_CLICK,940,wxID_ANY,wxID_ANY,1,0)==0,"Grid child BUTTON bind");
    wxCommandEvent button(wxEVT_BUTTON,window(child)->GetId()); button.SetEventObject(window(child)); window(child)->HandleWindowEvent(button);
    expect(calls==2 && wxl_unbind(940)==1,"Grid child BUTTON lifetime guards");
    // Private fixture table invokes checked APIs synchronously inside SetValue.
    auto* table=new GridTableProbe(); grid->SetTable(table,true);
    unsigned operations=0;
    table->during_change=[&] {
        ++operations;
        expect(wxl_destroy(id)==-1 && wxl_destroy(panel)==-1 && wxl_close(frame,1)==-1 &&
               wxl_set_grid_text(id,0,0,0,"recursive",9)==-1 && wxl_set_grid_editable(id,0)==-1,"Grid native operation lifetime/reentry guards");
        expect(wxl_poll()==-1 && wxl_run()==-1 && wxl_finish()==-1,"Grid operation native entry guards");
    };
    expect(wxl_set_grid_text(id,0,0,0,"outer",5)==0 && operations==1 && input_operations.empty(),"Grid synchronous SetValue guard unwinds");
    table->during_change={};
    auto* teardown=new ManagedWindow<EarlyGridDestroyProbe>(window(panel),wxID_ANY);
    expect(teardown->CreateGrid(1,1),"Early grid fixture creation");
    const auto teardown_id=track(teardown); input_owners.emplace(teardown,false);
    grid_probe=[&](uint64_t,const wxl_event*) { ++calls; };
    expect(wxl_bind(frame,WXL_EVENT_TEXT_CHANGE,940,wxID_ANY,wxID_ANY,1,0)==0,"Early grid teardown bind");
    teardown->during_destroy=[&] { wxCommandEvent event(wxEVT_TEXT,teardown->GetId()); event.SetEventObject(teardown); teardown->HandleWindowEvent(event); };
    teardown->refuse=true;
    expect(!teardown->Destroy() && !input_owners.at(teardown) && calls==2,"Refused grid Destroy restores early input suppression");
    teardown->refuse=false;
    expect(wxl_destroy(teardown_id)==0 && calls==2 && wxl_unbind(940)==1,"Early grid Destroy suppresses child input callbacks");
    std::atomic<int> foreign{99}; std::thread worker([&] { foreign=wxl_grid_property(id,0); }); worker.join();
    expect(foreign==-1,"Grid foreign-thread getter rejects");
    grid_probe={}; delete window(frame);
    expect(wxl_grid_property(id,0)==-1 && !wxl_grid_text(id,0,0,0,&length) && wxl_set_grid_editable(id,0)==-1,"Grid owner teardown invalidates resources");
}
