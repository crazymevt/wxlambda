/* Activity indicator: window-owned animation, initially stopped unless requested.
   running accepts0/1. Getters return0/1 or-1 error; status setters return0/-1.
   Mutations require callback deferral and protect synchronous native work. */
WXL_API wxl_handle wxl_activity_indicator(wxl_handle parent, int running);
WXL_API int wxl_activity_running(wxl_handle id);
WXL_API int wxl_set_activity_running(wxl_handle id, int running);
