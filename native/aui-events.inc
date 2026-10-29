// Prelude: include after InputOperation and before native_event_type.
bool aui_event_kind(int kind) { return kind>=100 && kind<=104; }
wxEventType aui_event_type(int kind) {
    switch (kind) {
        case 100:return wxEVT_AUINOTEBOOK_PAGE_CHANGING;
        case 101:return wxEVT_AUINOTEBOOK_PAGE_CHANGED;
        case 102:return wxEVT_AUINOTEBOOK_PAGE_CLOSE;
        case 103:return wxEVT_AUINOTEBOOK_PAGE_CLOSED;
        case 104:return wxEVT_AUINOTEBOOK_DRAG_DONE;
        default:return wxEVT_NULL;
    }
}
int aui_kind_for_type(wxEventType type) {
    for (int kind=100;kind<=104;++kind) if (aui_event_type(kind)==type) return kind;
    return 0;
}
bool aui_payload_readonly(wxEventType type) { return aui_kind_for_type(type)!=0; }
bool aui_public_event(wxEvent& event) {
    // Private tab controls reuse PAGE_CHANGING with group-local indices before
    // wxAuiNotebook translates it. Compare the borrowed origin only; only a
    // registered live book can publish the public global-index notification.
    for (const auto& entry:windows) if (auto* object=entry.second.get())
        if (object==event.GetEventObject()) return dynamic_cast<wxAuiNotebook*>(object)!=nullptr;
    return false;
}
wxl_handle aui_window_id(wxWindow* object) {
    for (const auto& entry:windows) if (entry.second.get()==object) return entry.first;
    return 0;
}
struct AuiActive { wxWeakRef<wxAuiNotebook> book; std::vector<wxWeakRef<wxWindow>> pages; };
std::vector<AuiActive> aui_operations;
std::unordered_map<wxAuiNotebook*,wxl_handle> aui_closing_pages;
bool protected_aui_ancestor(wxWindow* object) {
    for (const auto& active:aui_operations) {
        for (const auto& page:active.pages) if (page.get()==object) return true;
        // Native sizing/focus can still hold descendant control pointers when
        // a page callback runs, including freshly attached page contents.
        for (auto* node=object;node;node=node->GetParent()) if (node==active.book.get()) return true;
        for (auto* node=static_cast<wxWindow*>(active.book.get());node;node=node->GetParent()) if (node==object) return true;
    }
    return false;
}
bool attached_aui_page(wxWindow* object) {
    auto* book=dynamic_cast<wxAuiNotebook*>(object->GetParent());
    return book && book->GetPageIndex(object)!=wxNOT_FOUND;
}
struct AuiOperation {
    std::unique_ptr<InputOperation> input;
    bool added=false;
    explicit AuiOperation(wxAuiNotebook* book,wxWindow* extra=nullptr,bool native=false) {
        for (const auto& active:aui_operations) if (active.book.get()==book) {
            require(native,"Defer same-notebook mutation until its operation returns"); return;
        }
        AuiActive active; active.book=book;
        for (size_t index=0;index<book->GetPageCount();++index) active.pages.emplace_back(book->GetPage(index));
        if (extra) active.pages.emplace_back(extra);
        bool existing=false; for (const auto& item:input_operations) if (item.get()==book) existing=true;
        if (!existing) input=std::make_unique<InputOperation>(book);
        aui_operations.push_back(std::move(active)); added=true;
    }
    ~AuiOperation() { if (added) aui_operations.pop_back(); }
};
void fill_aui_detail(wxEvent& event,wxl_event& snapshot) {
    auto* detail=dynamic_cast<wxAuiNotebookEvent*>(&event);
    if (!detail || !aui_kind_for_type(event.GetEventType())) return;
    snapshot.detail[0]=detail->GetOldSelection();
    auto* book=dynamic_cast<wxAuiNotebook*>(event.GetEventObject());
    if (!book) return;
    snapshot.detail[2]=aui_window_id(book);
    if (event.GetEventType()==wxEVT_AUINOTEBOOK_PAGE_CLOSED) {
        const auto found=aui_closing_pages.find(book);
        if (found!=aui_closing_pages.end()) snapshot.detail[1]=found->second;
    } else if (detail->GetSelection()>=0 && size_t(detail->GetSelection())<book->GetPageCount())
        snapshot.detail[1]=aui_window_id(book->GetPage(size_t(detail->GetSelection())));
}
