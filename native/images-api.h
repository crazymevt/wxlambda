WXL_API wxl_handle wxl_image(int width,int height,const unsigned char* rgb,size_t rgb_length,const unsigned char* alpha,size_t alpha_length);
WXL_API wxl_handle wxl_load_image(const char* path,size_t length);
WXL_API wxl_handle wxl_scale_image(wxl_handle id,int width,int height,int quality);
WXL_API int wxl_image_size(wxl_handle id,int bitmap,int* width,int* height);
WXL_API int wxl_image_copy_bytes(wxl_handle id,int alpha,unsigned char* output,size_t length);
WXL_API int wxl_image_has_alpha(wxl_handle id);
WXL_API int wxl_image_set_bytes(wxl_handle id,int alpha,const unsigned char* input,size_t length);
WXL_API int wxl_image_pixel(wxl_handle id,int x,int y,unsigned char* rgba,int write);
WXL_API int wxl_save_image(wxl_handle id,const char* path,size_t length);
WXL_API wxl_handle wxl_bitmap(wxl_handle image);
WXL_API wxl_handle wxl_copy_bitmap(wxl_handle bitmap);
WXL_API wxl_handle wxl_bitmap_image(wxl_handle bitmap);
WXL_API int wxl_bitmap_geometry(wxl_handle id,int* logical,int* dip,double* scale);
WXL_API int wxl_image_alive(wxl_handle id,int bitmap);
WXL_API int wxl_image_destroy(wxl_handle id,int bitmap);
WXL_API wxl_handle wxl_static_bitmap(wxl_handle parent,wxl_handle bitmap);
WXL_API wxl_handle wxl_static_bitmap_get(wxl_handle id);
WXL_API int wxl_static_bitmap_set(wxl_handle id,wxl_handle bitmap);
WXL_API int wxl_display_count();
WXL_API int wxl_display_info(int index,int client,int* rectangle,int* ppi,double* scale);
WXL_API int wxl_window_display(wxl_handle id);
WXL_API int wxl_window_point(wxl_handle id,int operation,int x,int y,int* result);
WXL_API int wxl_window_content_scale(wxl_handle id,double* scale);

WXL_API int wxl_bitmap_toggle_supported();
WXL_API wxl_handle wxl_bitmap_toggle(wxl_handle parent,wxl_handle bitmap,int checked,int border);
WXL_API wxl_handle wxl_bitmap_toggle_get(wxl_handle id);
WXL_API int wxl_bitmap_toggle_set(wxl_handle id,wxl_handle bitmap);

WXL_API int wxl_window_colours(wxl_handle id,unsigned char* foreground,unsigned char* background);

WXL_API int wxl_system_colours(unsigned char* foreground,unsigned char* background);
WXL_API int wxl_set_window_colours(wxl_handle id,const unsigned char* foreground,const unsigned char* background,int recursive);

WXL_API const char* wxl_font_names(int fixed,size_t* length);
WXL_API const char* wxl_default_font(int fixed,int* points,int* mono,size_t* length);
WXL_API const char* wxl_window_font(wxl_handle id,int* points,size_t* length);
WXL_API int wxl_set_window_font(wxl_handle id,const char* face,size_t length,int points,int recursive);

WXL_API int wxl_tree_icon_count(wxl_handle id);
WXL_API wxl_handle wxl_tree_icon_get(wxl_handle id,int index);
WXL_API int wxl_tree_icons(wxl_handle id,const wxl_handle* bitmaps,size_t count);
WXL_API int wxl_tree_item_icon(wxl_handle id,int write,int* index);
