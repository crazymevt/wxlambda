/* Private child browser; callbacks use copied JSON and numeric request IDs. */
WXL_API wxl_handle wxl_web_view(wxl_handle parent);
WXL_API const char* wxl_web_view_string(wxl_handle id, int field, size_t* length);
WXL_API int wxl_web_view_property(wxl_handle id, int field);
WXL_API int wxl_web_view_load(wxl_handle id, const char* text, size_t length, const char* base, size_t base_length, int html);
WXL_API int wxl_web_view_action(wxl_handle id, int action);
WXL_API int64_t wxl_web_view_script(wxl_handle id, const char* script, size_t length);
