enum wxl_aui_event_kind { WXL_EVENT_AUI_PAGE_CHANGING=100, WXL_EVENT_AUI_PAGE_CHANGED=101,
    WXL_EVENT_AUI_PAGE_CLOSE=102, WXL_EVENT_AUI_PAGE_CLOSED=103, WXL_EVENT_AUI_DRAG_DONE=104 };
/* Fixed top tabs, one-group drag movement, active-tab close; programmatic Split.
   Mouse split/external transfer disabled; split books use
   checked programmatic movement. Page limit256, copied caption<=4096UTF8 bytes.
   detail[0]=native old selection, [1]=stable target page handle or0,
   [2]=book handle or0. CLOSED selection is former index, never a live lookup.
   Mutation status0/-1; integer queries-2 error,-1 absent; handles0 error. */
WXL_API wxl_handle wxl_aui_notebook(wxl_handle parent);
WXL_API int wxl_aui_count(wxl_handle book);
WXL_API wxl_handle wxl_aui_page(wxl_handle book,int index);
WXL_API int wxl_aui_page_index(wxl_handle book,wxl_handle page);
WXL_API int wxl_aui_insert(wxl_handle book,wxl_handle page,int index,const char* label,size_t length,int select);
WXL_API int wxl_aui_remove(wxl_handle book,int index,int destroy);
WXL_API int wxl_aui_move(wxl_handle book,int from,int to);
/* direction0 left,1 right,2 top,3 bottom; at least2 pages. */
WXL_API int wxl_aui_split(wxl_handle book,int index,int direction);
WXL_API const char* wxl_aui_label(wxl_handle book,int index,size_t* length);
WXL_API int wxl_set_aui_label(wxl_handle book,int index,const char* label,size_t length);
WXL_API int wxl_aui_selection(wxl_handle book);
WXL_API int wxl_set_aui_selection(wxl_handle book,int index,int notify);
WXL_API int wxl_aui_detail(uint64_t context,int field,int64_t* result);
WXL_API int wxl_splitter_initialize(wxl_handle splitter,wxl_handle pane);
WXL_API int wxl_splitter_unsplit(wxl_handle splitter,int index);
WXL_API int wxl_splitter_replace(wxl_handle splitter,int index,wxl_handle pane);
WXL_API int wxl_splitter_resplit(wxl_handle splitter,wxl_handle pane);
WXL_API int wxl_set_splitter_orientation(wxl_handle splitter,int orientation);
