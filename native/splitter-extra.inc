// Included inside the native extern-C ABI tail.
void splitter_extra_ready(FixedSplitter* splitter) {
    require(callback_depth==0,"Defer splitter structure/orientation changes until callbacks return");
    require_dispatch_entry(true);
    require(!window_destruction_pending(splitter),"Splitter destruction is pending");
}
void splitter_extra_child(FixedSplitter* splitter,wxPanel* pane) {
    require(pane->GetParent()==splitter && !pane->GetContainingSizer(),"Pane must be a direct unsizered panel child");
    require(splitter->GetWindow1()!=pane && splitter->GetWindow2()!=pane,"Pane is already attached");
}
int wxl_splitter_initialize(wxl_handle id,wxl_handle pane_id) { return status([&] {
    auto* splitter=as<FixedSplitter>(id); auto* pane=as<wxPanel>(pane_id); splitter_extra_ready(splitter);
    require(!splitter->GetWindow1() && !splitter->GetWindow2(),"Splitter is already initialized"); splitter_extra_child(splitter,pane);
    SplitterOperation operation(splitter,pane); splitter->Initialize(pane); pane->Show();
}); }
int wxl_splitter_unsplit(wxl_handle id,int index) { return status([&] {
    auto* splitter=as<FixedSplitter>(id); splitter_extra_ready(splitter);
    require((index==0 || index==1) && splitter->IsSplit(),"Unsplit requires two panes and index0/1");
    auto* first=splitter->GetWindow1(); auto* second=splitter->GetWindow2(); SplitterOperation operation(splitter,first,second);
    require(splitter->Unsplit(index==0 ? first : second),"Unable to unsplit window");
}); }
int wxl_splitter_replace(wxl_handle id,int index,wxl_handle pane_id) { return status([&] {
    auto* splitter=as<FixedSplitter>(id); auto* pane=as<wxPanel>(pane_id); splitter_extra_ready(splitter);
    require(index==0 || index==1,"Invalid splitter pane index");
    auto* old=index==0 ? splitter->GetWindow1() : splitter->GetWindow2(); require(old!=nullptr,"No pane at index");
    splitter_extra_child(splitter,pane); SplitterOperation operation(splitter,old,pane);
    require(splitter->ReplaceWindow(old,pane),"Unable to replace splitter pane"); old->Hide(); pane->Show();
}); }
int wxl_splitter_resplit(wxl_handle id,wxl_handle pane_id) { return status([&] {
    auto* splitter=as<FixedSplitter>(id); auto* pane=as<wxPanel>(pane_id); splitter_extra_ready(splitter);
    auto* first=splitter->GetWindow1(); require(first && !splitter->GetWindow2(),"Resplit requires one initialized pane");
    splitter_extra_child(splitter,pane); SplitterOperation operation(splitter,first,pane);
    const int position=splitter->requested_position;
    require(splitter->GetSplitMode()==wxSPLIT_VERTICAL ? splitter->SplitVertically(first,pane,position) : splitter->SplitHorizontally(first,pane,position),"Unable to resplit window");
}); }
int wxl_set_splitter_orientation(wxl_handle id,int orientation) { return status([&] {
    auto* splitter=as<FixedSplitter>(id); splitter_extra_ready(splitter); require(orientation==0 || orientation==1,"Invalid splitter orientation");
    SplitterOperation operation(splitter,splitter->GetWindow1(),splitter->GetWindow2());
    splitter->SetSplitMode(orientation==0 ? wxSPLIT_VERTICAL : wxSPLIT_HORIZONTAL);
    splitter->SetSashPosition(splitter->requested_position); splitter->SizeWindows();
}); }
