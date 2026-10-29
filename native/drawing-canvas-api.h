/* Private retained-scene transport; no DC, bitmap, font or native pointer ownership. */
typedef struct wxl_canvas_item {
    int kind, x, y, width, height;
    uint32_t rgb;
    int option;
    const char* text;
    size_t text_length;
    const unsigned char* pixels;
    size_t pixel_length;
} wxl_canvas_item;
WXL_API wxl_handle wxl_drawing_canvas(wxl_handle parent);
WXL_API int wxl_set_canvas_content(wxl_handle id, uint32_t background, const wxl_canvas_item* items, size_t count);
WXL_API int wxl_canvas_client_size(wxl_handle id, int* width, int* height);
WXL_API int wxl_save_canvas_image(wxl_handle id, const char* path, size_t length, int width, int height);
