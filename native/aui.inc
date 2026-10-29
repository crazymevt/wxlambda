// Include at the ABI implementation tail, outside an extern-C block.
// Flat themed art retains wxAUI sizing, page hit testing and native events.
class FlatAuiTabArt : public wxAuiGenericTabArt {
public:
    wxAuiTabArt* Clone() override { return new FlatAuiTabArt(*this); }
    int GetBorderWidth(wxWindow*) override { return 0; }
    void DrawBorder(wxDC&,wxWindow*,const wxRect&) override { }
    void DrawBackground(wxDC& dc,wxWindow*,const wxRect& rect) override {
        dc.SetPen(*wxTRANSPARENT_PEN); dc.SetBrush(wxBrush(m_baseColour)); dc.DrawRectangle(rect);
    }
    void DrawTab(wxDC& dc,wxWindow* wnd,const wxAuiNotebookPage& page,const wxRect& area,
                 int close_state,wxRect* tab_rect,wxRect* close_rect,int* extent) override {
        const auto measured=GetTabSize(dc,wnd,page.caption,page.bitmap,page.active,close_state,extent);
        const wxRect tab(area.x,area.y,measured.x,area.height);
        *tab_rect=tab; *close_rect=wxRect();
        if (tab.width<=0 || tab.height<=0) return;
        const auto background=page.active ? m_activeColour : m_baseColour;
        auto linear=[](unsigned char channel) { const double c=channel/255.0; return c<=0.04045 ? c/12.92 : std::pow((c+0.055)/1.055,2.4); };
        const double luminance=.2126*linear(background.Red())+.7152*linear(background.Green())+.0722*linear(background.Blue());
        const wxColour foreground=luminance<.179 ? *wxWHITE : *wxBLACK;
        dc.SetClippingRegion(tab.Intersect(area));
        dc.SetPen(*wxTRANSPARENT_PEN); dc.SetBrush(wxBrush(background)); dc.DrawRectangle(tab);
        const int padding=wnd->FromDIP(8), gap=wnd->FromDIP(4);
        int left=tab.x+padding, right=tab.GetRight()-padding;
        if (page.bitmap.IsOk()) {
            const auto bitmap=page.bitmap.GetBitmapFor(wnd);
            dc.DrawBitmap(bitmap,left,tab.y+(tab.height-bitmap.GetLogicalHeight())/2,true);
            left+=bitmap.GetLogicalWidth()+gap;
        }
        if (close_state!=wxAUI_BUTTON_STATE_HIDDEN) {
            const int size=wnd->FromDIP(16);
            *close_rect=wxRect(tab.GetRight()-size-gap,tab.y+(tab.height-size)/2,size,size);
            right=close_rect->x-gap;
            if (close_state==wxAUI_BUTTON_STATE_HOVER || close_state==wxAUI_BUTTON_STATE_PRESSED) {
                dc.SetBrush(wxBrush(background.ChangeLightness(luminance<.179 ? 145 : 90)));
                dc.DrawRectangle(*close_rect);
            }
            dc.SetPen(wxPen(foreground,wnd->FromDIP(1)));
            const int inset=wnd->FromDIP(5);
            dc.DrawLine(close_rect->x+inset,close_rect->y+inset,close_rect->GetRight()-inset,close_rect->GetBottom()-inset);
            dc.DrawLine(close_rect->GetRight()-inset,close_rect->y+inset,close_rect->x+inset,close_rect->GetBottom()-inset);
        }
        dc.SetFont(page.active ? m_selectedFont : m_normalFont); dc.SetTextForeground(foreground);
        wxString text=page.caption;
        const int available=std::max(0,right-left);
        if (dc.GetTextExtent(text).x>available) {
            const wxString suffix=wxS("\u2026");
            while (!text.empty() && dc.GetTextExtent(text+suffix).x>available) text.RemoveLast();
            text=dc.GetTextExtent(suffix).x<=available ? text+suffix : wxString();
        }
        dc.DrawText(text,left,tab.y+(tab.height-dc.GetTextExtent(text).y)/2);
        if (page.active) {
            dc.SetPen(wxPen(foreground,wnd->FromDIP(2)));
            dc.DrawLine(tab.x,tab.y+wnd->FromDIP(1),tab.GetRight(),tab.y+wnd->FromDIP(1));
        }
        dc.DestroyClippingRegion();
    }
};

class DynamicAuiNotebook : public wxAuiNotebook {
public:
    int first_tab_id=0;
    bool themed_colours=false; wxColour theme_base,theme_active;
    bool Create(wxWindow* parent,wxWindowID id,const wxPoint& position=wxDefaultPosition,
                const wxSize& size=wxDefaultSize,long style=wxAUI_NB_DEFAULT_STYLE) {
        const bool result=wxAuiNotebook::Create(parent,id,position,size,style);
        if (result) first_tab_id=m_tabIdCounter;
        return result;
    }
    bool TabIdAvailable() const { return first_tab_id && m_tabIdCounter<=first_tab_id+500; }
    bool GestureActive() const {
        for (auto* child:GetChildren()) if (auto* tabs=dynamic_cast<wxAuiTabCtrl*>(child))
            if (tabs->IsDragging() || wxWindow::GetCapture()==tabs) return true;
        return false;
    }
    size_t TabGroupCount() {
        size_t count=0; auto& panes=m_mgr.GetAllPanes();
        for (size_t index=0;index<panes.GetCount();++index) if (panes.Item(index).name!=wxT("dummy")) ++count;
        return count;
    }
    void SyncMovementStyle() {
        const long previous=GetWindowStyleFlag();
        const long style=TabGroupCount()<=1 ? previous|wxAUI_NB_TAB_MOVE : previous&~wxAUI_NB_TAB_MOVE;
        if (style!=previous) SetWindowStyleFlag(style);
    }
    bool ProcessEvent(wxEvent& event) override {
        // Internal tab-button/drag events are not public bindings, but their
        // complete native handler span must protect pages after callbacks return.
        if (!dynamic_cast<wxAuiNotebookEvent*>(&event)) return wxAuiNotebook::ProcessEvent(event);
        if (window_destruction_pending(this)) return true;
        bool outer=true; for (const auto& active:aui_operations) if (active.book.get()==this) outer=false;
        AuiOperation operation(this,nullptr,true);
        struct ClosingScope {
            wxAuiNotebook* book; bool clear;
            ~ClosingScope() { if (clear) aui_closing_pages.erase(book); }
        } closing{this,outer};
        if (event.GetEventType()==wxEVT_AUINOTEBOOK_END_DRAG && TabGroupCount()>1) {
            // Pinned3.2.12's native drag handlers mix tab-group-local indices
            // with global catalogue indices. Movement is disabled in split
            // books. The tab control already released capture and cleared its
            // dragging flag before END_DRAG; native cancellation clears hints
            // and cursor without touching page ordering or global selection.
            OnTabCancelDrag(static_cast<wxAuiNotebookEvent&>(event)); return true;
        }
        if (event.GetEventType()==wxEVT_AUINOTEBOOK_PAGE_CLOSE) {
            const int index=static_cast<wxAuiNotebookEvent&>(event).GetSelection();
            aui_closing_pages[this]=(index>=0 && size_t(index)<GetPageCount()) ? aui_window_id(GetPage(size_t(index))) : 0;
        }
        const bool result=wxAuiNotebook::ProcessEvent(event); SyncMovementStyle(); return result;
    }
    ~DynamicAuiNotebook() override { aui_closing_pages.erase(this); }
};
void aui_mutation_ready(DynamicAuiNotebook* book,bool structural=false) {
    require(callback_depth==0,"Defer AUI notebook mutation until callbacks return");
    require(!book->GestureActive(),"Finish the native tab gesture before changing notebook properties");
    if (structural) require_dispatch_entry(true);
    require(!window_destruction_pending(book),"AUI notebook destruction is pending");
}
void aui_index(wxAuiNotebook* book,int index,bool insertion=false) {
    require(index>=0 && size_t(index)<book->GetPageCount()+(insertion ? 1 : 0),"AUI notebook index out of bounds");
}
extern "C" {
int wxl_aui_detail(uint64_t token,int field,int64_t* result) { return status([&] {
    require(result && field>=0 && field<=2,"Invalid AUI event detail output/field");
    auto& event=current_event(token); require(aui_payload_readonly(event.GetEventType()),"AUI detail requires a current native AUI context");
    wxl_event snapshot{}; fill_aui_detail(event,snapshot); *result=snapshot.detail[field];
}); }
wxl_handle wxl_aui_notebook(wxl_handle parent) { return create([&] {
    auto* owner=window(parent);
    auto book=std::make_unique<ManagedWindow<DynamicAuiNotebook>>();
    const long style=wxAUI_NB_TOP|wxAUI_NB_TAB_MOVE|wxAUI_NB_SCROLL_BUTTONS|wxAUI_NB_CLOSE_ON_ACTIVE_TAB;
    // Create installs wxAuiManager's private pushed handler. A temporary
    // wxEventBlocker spanning Create would pop the wrong handler on destruction.
    struct CreationScope { CreationScope() { ++input_creation_depth; } ~CreationScope() { --input_creation_depth; } } creation;
    require(book->Create(owner,wxID_ANY,wxDefaultPosition,wxDefaultSize,style),"Native AUI notebook creation failed");
    const auto id=track(book.get()); input_owners.emplace(book.get(),false); book.release(); return id;
}); }
int wxl_aui_count(wxl_handle id) {
    int value=-2; status([&] { value=int(as<DynamicAuiNotebook>(id)->GetPageCount()); }); return value;
}
wxl_handle wxl_aui_page(wxl_handle id,int index) {
    wxl_handle result=0; status([&] {
        auto* book=as<DynamicAuiNotebook>(id); aui_index(book,index);
        auto* page=book->GetPage(size_t(index)); require(!window_destruction_pending(page),"AUI page destruction is pending");
        result=aui_window_id(page); require(result!=0,"Unmanaged AUI notebook page");
    }); return result;
}
int wxl_aui_page_index(wxl_handle id,wxl_handle page_id) {
    int result=-2; status([&] { auto* book=as<DynamicAuiNotebook>(id); result=book->GetPageIndex(window(page_id)); }); return result;
}
int wxl_aui_insert(wxl_handle id,wxl_handle page_id,int index,const char* label,size_t length,int select) { return status([&] {
    auto* book=as<DynamicAuiNotebook>(id); auto* page=as<wxPanel>(page_id); aui_mutation_ready(book,true); aui_index(book,index,true);
    require(book->GetPageCount()<256,"AUI notebook supports at most256 pages");
    require(select==0 || select==1,"Invalid AUI select flag"); require(length<=4096,"AUI caption exceeds4096 UTF8 bytes");
    const auto caption=menu_string(label,length,true);
    require(page->GetParent()==book && !page->GetContainingSizer() && book->GetPageIndex(page)==wxNOT_FOUND,"AUI page requires an unattached direct unsizered panel child");
    require(book->GetPageCount()!=0 || book->TabIdAvailable(),"AUI private tab-group ID capacity exhausted; create a new notebook");
    AuiOperation operation(book,page);
    require(book->InsertPage(size_t(index),page,caption,select!=0),"Unable to insert AUI notebook page");
    book->SyncMovementStyle();
}); }
int wxl_aui_remove(wxl_handle id,int index,int destroy) { return status([&] {
    auto* book=as<DynamicAuiNotebook>(id); aui_mutation_ready(book,true); aui_index(book,index);
    require(destroy==0 || destroy==1,"Invalid AUI delete flag"); AuiOperation operation(book);
    require(destroy ? book->DeletePage(size_t(index)) : book->RemovePage(size_t(index)),"Unable to remove AUI notebook page");
    book->SyncMovementStyle();
}); }
int wxl_aui_move(wxl_handle id,int from,int to) { return status([&] {
    auto* book=as<DynamicAuiNotebook>(id); aui_mutation_ready(book,true); aui_index(book,from); aui_index(book,to);
    if (from==to) return;
    auto* page=book->GetPage(size_t(from)); const auto caption=book->GetPageText(size_t(from));
    auto* selected=book->GetSelection()>=0 ? book->GetPage(size_t(book->GetSelection())) : nullptr;
    AuiOperation operation(book); wxEventBlocker blocker(book);
    require(book->RemovePage(size_t(from)),"Unable to detach AUI page for move");
    require(book->InsertPage(size_t(to),page,caption,false),"Unable to insert moved AUI page");
    if (selected) book->ChangeSelection(size_t(book->GetPageIndex(selected)));
    book->SyncMovementStyle();
}); }
int wxl_aui_split(wxl_handle id,int index,int direction) { return status([&] {
    auto* book=as<DynamicAuiNotebook>(id); aui_mutation_ready(book,true); aui_index(book,index);
    require(direction>=0 && direction<=3,"Invalid AUI split direction");
    require(book->GetPageCount()>=2,"AUI split requires at least two pages");
    require(book->TabIdAvailable(),"AUI private tab-group ID capacity exhausted; create a new notebook");
    const int directions[]={wxLEFT,wxRIGHT,wxTOP,wxBOTTOM};
    AuiOperation operation(book); book->Split(size_t(index),directions[direction]); book->SyncMovementStyle();
}); }
const char* wxl_aui_label(wxl_handle id,int index,size_t* length) { return metadata_string([&] {
    auto* book=as<DynamicAuiNotebook>(id); aui_index(book,index); return book->GetPageText(size_t(index));
},length); }
int wxl_set_aui_label(wxl_handle id,int index,const char* label,size_t length) { return status([&] {
    auto* book=as<DynamicAuiNotebook>(id); aui_mutation_ready(book); aui_index(book,index);
    require(length<=4096,"AUI caption exceeds4096 UTF8 bytes"); const auto caption=menu_string(label,length,true);
    AuiOperation operation(book); require(book->SetPageText(size_t(index),caption),"Unable to update AUI caption");
}); }
int wxl_aui_selection(wxl_handle id) { int value=-2; status([&] { value=as<DynamicAuiNotebook>(id)->GetSelection(); }); return value; }
int wxl_set_aui_selection(wxl_handle id,int index,int notify) { return status([&] {
    auto* book=as<DynamicAuiNotebook>(id); aui_mutation_ready(book); aui_index(book,index);
    require(notify==0 || notify==1,"Invalid AUI notification flag"); AuiOperation operation(book);
    if (notify) book->SetSelection(size_t(index)); else book->ChangeSelection(size_t(index));
}); }
}

extern "C" {
int wxl_aui_tab_colours(wxl_handle id,int write,int reset,int64_t* base,int64_t* active,int* present) { return status([&] {
    auto* book=as<DynamicAuiNotebook>(id); require(base && active && present && (write==0 || write==1) && (reset==0 || reset==1),"Invalid tab colour arguments");
    if (write) {
        aui_mutation_ready(book,true);
        if (!reset) { stc_unpack_colour(*base); stc_unpack_colour(*active); }
        AuiOperation operation(book);
        std::unique_ptr<wxAuiTabArt> art;
        if (reset) art=std::make_unique<wxAuiDefaultTabArt>();
        else {
            auto custom=std::make_unique<FlatAuiTabArt>();
            custom->SetColour(stc_unpack_colour(*base)); custom->SetActiveColour(stc_unpack_colour(*active)); art=std::move(custom);
        }
        book->SetArtProvider(art.release());
        // SetArtProvider clones the art into each private group. If the height
        // is unchanged wxAUI does not resize the controls, leaving fresh generic
        // art with m_tabCtrlHeight=0. Initialize every clone's current geometry
        // explicitly, without waiting for a later native resize event.
        for (auto* child:book->GetChildren())
            if (auto* tabs=dynamic_cast<wxAuiTabCtrl*>(child)) {
                tabs->SetRect(tabs->GetClientRect());
                tabs->Refresh();
            }
        book->themed_colours=!reset;
        if (!reset) { book->theme_base=stc_unpack_colour(*base); book->theme_active=stc_unpack_colour(*active); }
        book->Refresh();
    }
    *present=book->themed_colours ? 1 : 0;
    if (book->themed_colours) { *base=stc_pack_colour(book->theme_base); *active=stc_pack_colour(book->theme_active); }
}); }
}
