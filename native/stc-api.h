enum wxl_stc_event_kind {
    WXL_EVENT_STC_MODIFIED=31,
    WXL_EVENT_STC_UPDATE_UI=32,
    WXL_EVENT_STC_STYLE_NEEDED=33,
    WXL_EVENT_STC_MARGIN_CLICK=34,
    WXL_EVENT_STC_MARGIN_RIGHT_CLICK=35,
    WXL_EVENT_STC_DWELL_START=36,
    WXL_EVENT_STC_DWELL_END=37,
    WXL_EVENT_STC_SAVE_POINT_REACHED=38,
    WXL_EVENT_STC_SAVE_POINT_LEFT=39,
    WXL_EVENT_STC_AUTOCOMPLETE_SELECTION=40,
    WXL_EVENT_STC_AUTOCOMPLETE_COMPLETED=41,
    WXL_EVENT_STC_CALL_TIP_CLICK=42,
    WXL_EVENT_STC_NEED_SHOWN=43,
    WXL_EVENT_STC_READ_ONLY_ATTEMPT=44,
    WXL_EVENT_STC_CHAR_ADDED=80
};
WXL_API wxl_handle wxl_stc(wxl_handle parent,const char* text,size_t length);
WXL_API int wxl_stc_get(wxl_handle id,int field,int index,int64_t* result);
WXL_API int wxl_stc_set(wxl_handle id,int field,int index,int64_t value);
WXL_API int wxl_stc_action(wxl_handle id,int operation,int a,int b,int64_t* result);
WXL_API const char* wxl_stc_text(wxl_handle id,int start,int end,size_t* length);
WXL_API int wxl_stc_replace(wxl_handle id,int start,int end,const char* text,size_t length);
WXL_API const char* wxl_stc_font(wxl_handle id,int style,size_t* length);
WXL_API int wxl_stc_string(wxl_handle id,int operation,int argument,const char* text,size_t length);
WXL_API int wxl_stc_point(wxl_handle id,int position,int* x,int* y);
WXL_API int wxl_stc_position(wxl_handle id,int x,int y,int* position);
WXL_API int wxl_stc_detail(uint64_t token,int field,int64_t* result);

WXL_API int wxl_stc_style_bytes(wxl_handle id,const unsigned char* bytes,size_t length);

WXL_API int wxl_stc_caret_colour(wxl_handle id,int write,int64_t* colour);
WXL_API int wxl_stc_selection_colours(wxl_handle id,int write,int reset,int64_t* foreground,int64_t* background,int* present);
