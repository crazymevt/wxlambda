// Included after harness helpers and token950/951 aui_probe routing.
class SplitterExtraProbe : public FixedSplitter {
public:
    using FixedSplitter::FixedSplitter;
    std::function<void()> during_size;
    void SizeWindows() override { if (during_size) during_size(); FixedSplitter::SizeWindows(); }
};
class AuiTabBudgetProbe : public DynamicAuiNotebook {
public:
    void ExhaustTabIds() { m_tabIdCounter=first_tab_id+501; }
};
void exercise_aui_notebook() {
    const auto frame=wxl_frame("AUI workspace",700,420), id=wxl_aui_notebook(frame);
    expect(id && wxl_aui_count(id)==0 && wxl_aui_selection(id)==-1,"Empty AUI construction");
    const auto a=wxl_panel(id), b=wxl_panel(id), c=wxl_panel(id);
    expect(wxl_aui_insert(id,a,0,"A λ",4,0)==0 && wxl_aui_insert(id,c,1,"C",1,0)==0 &&
           wxl_aui_insert(id,b,1,"B",1,1)==0,"AUI append/insert");
    expect(wxl_aui_page(id,1)==b && wxl_aui_page_index(id,b)==1 && wxl_aui_count(id)==3,"AUI canonical index/page lookup");
    expect(wxl_aui_insert(id,a,0,"again",5,0)==-1 && wxl_destroy(b)==-1 &&
           wxl_aui_remove(id,3,0)==-1 && wxl_aui_move(id,-1,0)==-1 && wxl_set_aui_selection(id,0,2)==-1,"AUI raw membership/index/flag checks");
    expect(wxl_aui_move(id,1,0)==0 && wxl_aui_page(id,0)==b && wxl_aui_selection(id)==0,"AUI move preserves selected page identity");
    expect(wxl_aui_remove(id,0,0)==0 && wxl_handler_alive(b)==1 && wxl_aui_page_index(id,b)==-1 && wxl_shown(b)==0,"AUI remove keeps hidden live child");
    expect(wxl_aui_insert(id,b,2,"B",1,1)==0,"AUI detached page reattach");
    auto* book=as<DynamicAuiNotebook>(id);
    expect(!(book->GetWindowStyleFlag() & (wxAUI_NB_TAB_EXTERNAL_MOVE|wxAUI_NB_TAB_SPLIT)) &&
           (book->GetWindowStyleFlag() & wxAUI_NB_TAB_MOVE),"AUI initial native movement/programmatic splitting scope");
    wxl_show(frame); book->SetSize(680,380); wxl_poll();
    expect(wxl_aui_split(id,2,1)==0,"Native AUI programmatic right split");
    wxAuiTabCtrl *left=nullptr,*right=nullptr; int left_index=-1,right_index=-1;
    expect(book->FindTab(window(a),&left,&left_index) && book->FindTab(window(b),&right,&right_index) && left!=right,
           "Native Split creates distinct private tab groups");
    // Replacing same-height tab art used to leave its drawing height at zero.
    // Exercise both private groups without a resize to catch clipped captions.
    const auto selected_before_theme=book->GetCurrentPage();
    for (int pass=0;pass<4;++pass) {
        int64_t base=pass%2 ? 0xE3E6EC : 0x181818;
        int64_t active=pass%2 ? 0xFFFFFF : 0x1F1F1F; int present=0;
        expect(wxl_aui_tab_colours(id,1,0,&base,&active,&present)==0 && present==1,
               "Apply repeated dark/light tab art without resizing");
        for (auto* tabs:{left,right}) {
            const wxSize size=tabs->GetClientSize();
            expect(size.x>0 && size.y>0,"Private tab group has drawable extent");
            wxBitmap bitmap(size.x,size.y); wxMemoryDC dc(bitmap);
            dc.SetBackground(wxBrush(*wxBLACK)); dc.Clear();
            wxRect tab,close; int extent=0;
            tabs->GetArtProvider()->DrawTab(dc,tabs,tabs->GetPage(0),wxRect(wxPoint(0,0),size),
                                           wxAUI_BUTTON_STATE_NORMAL,&tab,&close,&extent);
            expect(tab.height>0 && tab.y>=0 && tab.GetBottom()<size.y,
                   "Themed tab artwork and caption stay inside the tab strip");
            expect(close.width>0 && close.height>0 && close.y>=0 && close.y+close.width<=size.y,
                   "Themed square close glyph remains inside the tab strip");
            expect(dynamic_cast<FlatAuiTabArt*>(tabs->GetArtProvider()) && tabs->GetArtProvider()->GetBorderWidth(tabs)==0,
                   "Themed tabs use borderless flat art");
            // Solid interior samples prove the bevel/gradient is absent.
            const auto pixels=bitmap.ConvertToImage();
            const int sample_x=tab.x+1, upper=tab.y+4, lower=tab.GetBottom()-3;
            expect(pixels.GetRed(sample_x,upper)==pixels.GetRed(sample_x,lower) &&
                   pixels.GetGreen(sample_x,upper)==pixels.GetGreen(sample_x,lower) &&
                   pixels.GetBlue(sample_x,upper)==pixels.GetBlue(sample_x,lower),"Flat tab surface has no vertical gradient");
            dc.SelectObject(wxNullBitmap);
            if (pass==0 && tabs==left) {
                const auto image=publish_image(bitmap.ConvertToImage());
                const char* path="/private/tmp/wxlambda-themed-tab.png";
                expect(wxl_save_image(image,path,std::strlen(path))==0,"Save themed-tab drawing fixture");
                expect(wxl_image_destroy(image,0)==0,"Release themed-tab drawing fixture");
            }
        }
        expect(book->GetCurrentPage()==selected_before_theme && book->GetPageCount()==3,
               "Tab palette replacement preserves selection and page identities");
    }
    expect(window(a)->GetParent()==book && window(b)->GetParent()==book && window(c)->GetParent()==book &&
           wxl_aui_page_index(id,b)==2,"Native split preserves native page parent and global catalogue identity");
    expect(book->GetWindowStyleFlag() & wxAUI_NB_TAB_MOVE,"Safe native movement remains enabled in split groups");
    book->test_drag_point=true; book->test_drag_screen=wxPoint(-10000,-10000);
    const int before_drag=book->GetSelection();
    wxAuiNotebookEvent ignored_drag(wxEVT_AUINOTEBOOK_END_DRAG,right->GetId());
    ignored_drag.SetEventObject(right); ignored_drag.SetSelection(right_index); book->GetEventHandler()->ProcessEvent(ignored_drag);
    expect(book->GetSelection()==before_drag && wxWindow::GetCapture()!=right,"Split-group drag cancellation preserves global selection and leaves no mouse capture");
    expect(wxl_aui_split(id,0,4)==-1,"AUI split direction validation");
    left->CaptureMouse();
    expect(wxl_aui_split(id,0,0)==-1 && wxl_aui_move(id,0,1)==-1,"Active native tab gesture refuses structural edits");
    left->ReleaseMouse();
    unsigned private_click=0;
    aui_probe=[&](uint64_t,const wxl_event* event) {
        ++private_click;
        expect(event->origin_id==id && event->selection==2 && event->detail[1]==int64_t(b),
               "Private tab click publishes only translated global index and stable page ID");
    };
    expect(wxl_set_aui_selection(id,0,0)==0 && wxl_bind(id,100,950,wxID_ANY,wxID_ANY,1,0)==0,"Observe translated split-group selection");
    wxAuiNotebookEvent click(wxEVT_AUINOTEBOOK_PAGE_CHANGING,right->GetId());
    click.SetEventObject(right); click.SetSelection(right_index); click.SetOldSelection(right_index);
    book->GetEventHandler()->ProcessEvent(click);
    expect(private_click==1 && wxl_aui_selection(id)==2 && wxl_unbind(950)==1,"Split-group local index never escapes as public selection");
    unsigned changing=0;
    aui_probe=[&](uint64_t,const wxl_event* event) {
        ++changing;
        expect(event->kind==100 && event->detail[2]==int64_t(id),"AUI event origin detail");
        expect(wxl_set_aui_selection(id,0,0)==-1 && wxl_aui_remove(id,0,1)==-1 &&
               wxl_aui_split(id,0,0)==-1 && wxl_destroy(id)==-1 && wxl_close(frame,1)==-1,"AUI selection callback mutation/lifetime guards");
        expect(wxl_set_event_field(event->context,1,7,0)==-1 && wxl_context_set_id(event->context,5380)==-1 &&
               !wxl_clone_event(event->context,1),"AUI read-only payload/ID and no clone");
        expect(wxl_context_set_vetoed(event->context,1)==0,"AUI changing veto");
    };
    expect(wxl_bind(id,100,950,wxID_ANY,wxID_ANY,1,0)==0 && wxl_set_aui_selection(id,0,1)==0 &&
           wxl_aui_selection(id)==2 && changing==1 && wxl_unbind(950)==1,"AUI real selection veto");
    bool veto=true; unsigned closing=0,closed=0;
    const auto target=b;
    aui_probe=[&](uint64_t token,const wxl_event* event) {
        if (event->kind==102) {
            ++closing;
            expect(event->detail[1]==int64_t(target),"AUI close freezes stable target ID");
            expect(wxl_destroy(id)==-1 && wxl_close(frame,1)==-1 && wxl_aui_move(id,0,1)==-1,"AUI native close span protects book/ancestors");
            int result=-1;
            expect(wxl_poll()==-1 && wxl_clipboard_text_available()==-1,"AUI close native-entry guards");
            (void)result;
            if (veto) expect(wxl_context_set_vetoed(event->context,1)==0,"AUI native close veto");
            if (token==950) expect(wxl_context_set_skipped(event->context,1)==0,"AUI close propagation");
        } else {
            ++closed; expect(event->kind==103 && event->detail[1]==int64_t(target),"AUI closed retains removed target ID, not shifted index");
        }
    };
    expect(wxl_bind(id,102,950,wxID_ANY,wxID_ANY,1,0)==0 && wxl_bind(frame,102,951,wxID_ANY,wxID_ANY,1,0)==0,"AUI close/parent binding");
    auto close_button=[&] {
        wxAuiTabCtrl* tabs=nullptr; int index=-1;
        expect(book->FindTab(window(target),&tabs,&index),"Find native tab for close");
        wxAuiNotebookEvent button(wxEVT_AUINOTEBOOK_BUTTON,tabs->GetId()); button.SetEventObject(tabs);
        button.SetSelection(index); button.SetInt(wxAUI_BUTTON_CLOSE); book->GetEventHandler()->ProcessEvent(button);
    };
    close_button();
    expect(closing==2 && wxl_aui_count(id)==3 && wxl_handler_alive(target)==1,"Native close veto retains page");
    expect(wxl_unbind(951)==1 && wxl_bind(id,103,951,wxID_ANY,wxID_ANY,1,0)==0,"Observe actual native closed event");
    veto=false; close_button();
    expect(closing==3 && closed==1 && wxl_aui_count(id)==2 && aui_operations.empty() && input_operations.empty(),"Native accepted close deletes page and unwinds full span");
    expect(wxl_unbind(950)==1 && wxl_unbind(951)==1,"AUI close cleanup");
    expect(book->GetWindowStyleFlag() & wxAUI_NB_TAB_MOVE,"Mouse movement restored after split group closure leaves one group");
    expect(wxl_aui_split(id,0,2)==0 && wxl_aui_move(id,0,1)==0 && wxl_aui_count(id)==2 &&
           wxl_aui_remove(id,0,0)==0 && wxl_aui_count(id)==1 && wxl_aui_split(id,0,0)==-1,
           "AUI split then cross-group move/remove and single-page refusal");
    auto* budget=new ManagedWindow<AuiTabBudgetProbe>();
    expect(budget->Create(window(frame),wxID_ANY,wxDefaultPosition,wxSize(300,200),wxAUI_NB_TOP|wxAUI_NB_TAB_MOVE),"AUI ID budget fixture creation");
    const auto budget_id=track(budget); input_owners.emplace(budget,false);
    const auto budget_a=wxl_panel(budget_id),budget_b=wxl_panel(budget_id);
    expect(wxl_aui_insert(budget_id,budget_a,0,"A",1,0)==0 && wxl_aui_insert(budget_id,budget_b,1,"B",1,0)==0,"AUI ID budget fixture pages");
    budget->ExhaustTabIds();
    expect(wxl_aui_split(budget_id,0,0)==-1,"Programmatic split rejects exhausted native private ID range");
    auto* budget_tabs=budget->GetActiveTabCtrl();
    wxAuiNotebookEvent end_drag(wxEVT_AUINOTEBOOK_END_DRAG,budget_tabs->GetId());
    end_drag.SetEventObject(budget_tabs); end_drag.SetSelection(0); budget->GetEventHandler()->ProcessEvent(end_drag);
    expect(wxl_aui_count(budget_id)==2 && window(budget_a)->GetParent()==budget,"One-group drag remains safe without allocating new private IDs");
    expect(wxl_aui_remove(budget_id,0,1)==0 && wxl_aui_remove(budget_id,0,1)==0,"Existing pages remain deletable at ID exhaustion");
    const auto fresh=wxl_panel(budget_id);
    expect(wxl_aui_insert(budget_id,fresh,0,"Fresh",5,0)==-1,"Empty notebook refuses allocating unusable private tab IDs");
    for (int kind=100;kind<=104;++kind) expect(!wxl_owned_event(kind,1,nullptr,0,0,0,0,0),"AUI owned event rejection");
    std::atomic<int> foreign{99}; std::thread worker([&] { foreign=wxl_aui_count(id); }); worker.join();
    expect(foreign==-2,"AUI foreign-thread getter rejects");
    // Native sizing reentry guards protect replacement panes before publication.
    auto* probe=new ManagedWindow<SplitterExtraProbe>(window(frame),wxID_ANY);
    probe->SetMinimumPaneSize(20); const auto splitter=track(probe); splitter_owners.insert(probe);
    const auto one=wxl_panel(splitter), two=wxl_panel(splitter), three=wxl_panel(splitter);
    expect(wxl_splitter_initialize(splitter,one)==0 && wxl_splitter_resplit(splitter,two)==0,"Dynamic splitter initialize/resplit");
    unsigned sizing=0;
    probe->during_size=[&] {
        ++sizing;
        expect(wxl_destroy(two)==-1 && wxl_destroy(three)==-1 && wxl_destroy(splitter)==-1 &&
               wxl_set_splitter_orientation(splitter,1)==-1,"Replacement native sizing protects old/new/owner and rejects reentry");
    };
    expect(wxl_splitter_replace(splitter,1,three)==0 && sizing>0,"Dynamic splitter replacement guarded sizing");
    probe->during_size={};
    expect(wxl_shown(two)==0 && wxl_handler_alive(two)==1 && wxl_splitter_unsplit(splitter,1)==0 &&
           wxl_handler_alive(three)==1 && wxl_set_splitter_orientation(splitter,1)==0 && wxl_splitter_resplit(splitter,three)==0,"Retained hidden panes and mutable orientation");
    aui_probe={}; delete window(frame);
    expect(wxl_aui_count(id)==-2 && wxl_handler_alive(a)==0 && wxl_handler_alive(c)==0 && wxl_handler_alive(target)==0,"AUI ancestor teardown invalidates pages");
}

void exercise_aui_group_drag() {
    const auto frame=wxl_frame("Group movement",900,500), id=wxl_aui_notebook(frame);
    const auto a=wxl_panel(id),b=wxl_panel(id),c=wxl_panel(id),d=wxl_panel(id);
    for (auto page:{a,b,c,d}) expect(wxl_aui_insert(id,page,wxl_aui_count(id),"Tab",3,0)==0,"Group fixture append");
    auto* book=as<DynamicAuiNotebook>(id); wxl_show(frame); book->SetSize(880,450); wxl_poll();
    expect(wxl_aui_split(id,wxl_aui_page_index(id,d),1)==0,"Group fixture split"); wxl_poll();
    auto group=[&](wxl_handle page) {
        int indices[256]; size_t count=0; std::vector<wxl_handle> pages;
        expect(wxl_aui_group_indices(id,wxl_aui_page_index(id,page),indices,256,&count)==0,"Query visual group");
        for (size_t i=0;i<count;++i) pages.push_back(wxl_aui_page(id,indices[i]));
        return pages;
    };
    expect(group(a)==std::vector<wxl_handle>({a,b,c}) && group(d)==std::vector<wxl_handle>({d}),"Initial group order");
    auto drag=[&](wxl_handle page,wxl_handle destination,bool body,bool after) {
        auto* source=book->GroupForPage(window(page)); auto* target=book->GroupForPage(window(destination));
        int local=source->GetIdxFromWindow(window(page)); const int target_index=target->GetIdxFromWindow(window(destination));
        if (body) book->test_drag_screen=window(destination)->ClientToScreen(wxPoint(30,50));
        else {
            const auto rect=target->GetPage(target_index).rect;
            expect(rect.width>4 && rect.height>4,"Native tab rectangle available for deterministic drop");
            book->test_drag_screen=target->ClientToScreen(wxPoint(rect.x+(after ? rect.width-2 : 2),rect.y+rect.height/2));
        }
        book->test_drag_point=true;
        wxAuiNotebookEvent motion(wxEVT_AUINOTEBOOK_DRAG_MOTION,source->GetId());
        motion.SetEventObject(source); motion.SetSelection(local); book->GetEventHandler()->ProcessEvent(motion);
        wxAuiNotebookEvent end(wxEVT_AUINOTEBOOK_END_DRAG,source->GetId());
        end.SetEventObject(source); end.SetSelection(local); book->GetEventHandler()->ProcessEvent(end);
        wxl_poll();
        expect(book->GetCurrentPage()==window(page),"Native drop selects source page by global identity");
        expect(window(page)->GetParent()==book && wxl_aui_count(id)==4,"Native drop preserves membership/parent");
    };
    drag(c,a,false,false);
    expect(group(a)==std::vector<wxl_handle>({c,a,b}),"Same-group drop reorders visual tabs");
    drag(b,d,false,true);
    expect(group(d)==std::vector<wxl_handle>({d,b}) && group(a)==std::vector<wxl_handle>({c,a}),"Cross-group tab-strip drop");
    drag(a,b,true,false);
    expect(group(d)==std::vector<wxl_handle>({d,b,a}),"Editor-body drop appends to existing destination group");
    drag(c,a,true,false);
    expect(group(d)==std::vector<wxl_handle>({d,b,a,c}) && book->TabGroupCount()==1,"Last source tab drop removes empty group");
    expect(wxl_aui_move_to_group(id,wxl_aui_page_index(id,c),wxl_aui_page_index(id,d),0)==0,
           "Public group move shares checked drag movement implementation");
    expect(group(d)==std::vector<wxl_handle>({c,d,b,a}),"Group move preserves requested visual insertion slot");
    expect(wxl_aui_move_to_group(id,0,1,257)==-1,"Invalid group insertion rejected");
    int indices[256];size_t count=0;
    expect(wxl_aui_group_indices(id,-1,indices,256,&count)==-1 && wxl_aui_group_indices(id,0,indices,2,&count)==-1,"Group query bounds");
    wxl_destroy(frame);wxl_poll();
}

void exercise_aui_selection_focus() {
    const auto frame=wxl_frame("Selection focus",800,500), id=wxl_aui_notebook(frame);
    const auto a=wxl_panel(id), b=wxl_panel(id), c=wxl_panel(id);
    const auto input=wxl_text(a,"editor",6,1,1);
    for (auto page:{a,b,c}) expect(wxl_aui_insert(id,page,wxl_aui_count(id),"Tab",3,0)==0,"Selection fixture append");
    auto* book=as<DynamicAuiNotebook>(id); wxl_show(frame);book->SetSize(780,450);wxl_poll();
    expect(wxl_set_aui_selection(id,0,0)==0,"Select outgoing editor");
    window(input)->SetFocus();
    expect(wxl_set_aui_selection(id,2,1)==0 && wxl_aui_selection(id)==2 && window(c)->IsShown(),
           "Child focus while hiding outgoing editor cannot select intermediate tab");
    // Inject the exact native child-focus reentry deterministically as well.
    book->ChangeSelection(0);
    { DynamicAuiNotebook::SelectionScope scope(*book);
      wxChildFocusEvent focus(window(b));book->GetEventHandler()->ProcessEvent(focus); }
    expect(book->GetSelection()==0,"Selection scope suppresses nested native CHILD_FOCUS routing");
    wxl_destroy(frame);wxl_poll();
}
