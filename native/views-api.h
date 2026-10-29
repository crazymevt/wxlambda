enum wxl_views_kind { WXL_EVENT_LIST_SELECTED=140,WXL_EVENT_LIST_DESELECTED=141,
 WXL_EVENT_LIST_ACTIVATED=142,WXL_EVENT_LIST_RIGHT_CLICK=143,WXL_EVENT_LIST_CACHE_HINT=144,
 WXL_EVENT_LIST_COLUMN_CLICK=145,WXL_EVENT_TREE_ACTIVATED=150,WXL_EVENT_TREE_MENU=151,
 WXL_EVENT_TREE_EXPANDING=152,WXL_EVENT_TREE_EXPANDED=153,WXL_EVENT_TREE_COLLAPSING=154,WXL_EVENT_TREE_COLLAPSED=155 };
typedef void (*wxl_report_list_text_callback)(uint64_t token,uint64_t request,int row,int column);
WXL_API wxl_handle wxl_report_list_control(wxl_handle parent,int virtual_list,uint64_t token,wxl_report_list_text_callback callback);
WXL_API int wxl_report_list_provider_active(uint64_t token);
WXL_API int wxl_report_list_text_reply(uint64_t request,const char* text,size_t length);
WXL_API int wxl_report_list_property(wxl_handle list,int field); /* virtual0,columns1,items2,selection3(-1 absent),-2 error */
WXL_API int wxl_report_list_validate_cell(wxl_handle list,int row,int column);
WXL_API const char* wxl_report_list_text(wxl_handle list,int row,int column,size_t* length);
WXL_API int wxl_set_report_list_text(wxl_handle list,int row,int column,const char* text,size_t length);
WXL_API int wxl_report_list_insert_column(wxl_handle list,int index,const char* text,size_t length,int width);
WXL_API int wxl_report_list_delete_column(wxl_handle list,int index);
WXL_API const char* wxl_report_list_column_text(wxl_handle list,int index,size_t* length);
WXL_API int wxl_set_report_list_column_text(wxl_handle list,int index,const char* text,size_t length);
WXL_API int wxl_report_list_column_width(wxl_handle list,int index);
WXL_API int wxl_set_report_list_column_width(wxl_handle list,int index,int width);
WXL_API int wxl_report_list_append(wxl_handle list,const char* const* texts,const size_t* lengths,size_t count);
WXL_API int wxl_report_list_delete(wxl_handle list,int row);
WXL_API int wxl_report_list_clear(wxl_handle list);
WXL_API int wxl_set_report_list_count(wxl_handle list,int count);
WXL_API int wxl_set_report_list_selection(wxl_handle list,int row,int notify);
WXL_API int wxl_report_list_refresh(wxl_handle list,int first,int last);
WXL_API int wxl_views_detail(uint64_t context,int field,int64_t* result);
WXL_API int wxl_tree_decoration(wxl_handle item,int field,int64_t* result);
WXL_API int wxl_set_tree_decoration(wxl_handle item,int field,int64_t value);
