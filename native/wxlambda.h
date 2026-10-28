#ifndef WXLAMBDA_H
#define WXLAMBDA_H
#include <stdint.h>
#ifdef _WIN32
# ifdef WXLAMBDA_BUILD
#  define WXL_API __declspec(dllexport)
# else
#  define WXL_API __declspec(dllimport)
# endif
#else
# define WXL_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t wxl_handle;
enum wxl_alignment { WXL_ALIGN_START = 0, WXL_ALIGN_CENTER = 1, WXL_ALIGN_END = 2 };
enum wxl_event_kind { WXL_EVENT_CLICK = 1, WXL_EVENT_TEXT_CHANGE = 2 };
/* text is NULL for clicks, UTF-8 for text changes, and borrowed only until
   this callback returns. Callbacks must never unwind across this boundary. */
typedef void (*wxl_callback)(uint64_t token, int kind, const char *text);
WXL_API int wxl_abi_version(void);
/* Error text is borrowed until the next bridge call on this thread. */
WXL_API const char *wxl_last_error(void);
WXL_API int wxl_start(wxl_callback callback);
WXL_API int wxl_run(void);
WXL_API int wxl_stop(void);
WXL_API int wxl_finish(void);
WXL_API wxl_handle wxl_frame(const char *title, int width, int height);
WXL_API wxl_handle wxl_panel(wxl_handle parent);
WXL_API wxl_handle wxl_button(wxl_handle parent, const char *label);
WXL_API wxl_handle wxl_text(wxl_handle parent, const char *value);
WXL_API int wxl_column(wxl_handle parent);
WXL_API int wxl_row(wxl_handle parent);
/* Alignment is across the layout's minor axis. Non-start alignment requires
   expand=0. Border applies to all four sides; proportions act on the major axis. */
WXL_API int wxl_add(wxl_handle parent, wxl_handle child, int proportion,
                    int border, int expand, int alignment);
WXL_API int wxl_spacer(wxl_handle parent, int size);
WXL_API int wxl_stretch(wxl_handle parent, int proportion);
WXL_API int wxl_layout(wxl_handle window);
/* All four output pointers must be non-NULL. Values use wxWidgets coordinates. */
WXL_API int wxl_geometry(wxl_handle window, int *x, int *y, int *width, int *height);
WXL_API int wxl_show(wxl_handle window);
WXL_API int wxl_destroy(wxl_handle window);
/* notify=0 is silent; nonzero may synchronously invoke text-change callbacks. */
WXL_API int wxl_set_text(wxl_handle control, const char *value, int notify);
/* UTF-8 text borrowed until the next wxl_get_text call on this thread.
   NULL indicates failure; an empty string indicates a successful empty value. */
WXL_API const char *wxl_get_text(wxl_handle control);
WXL_API int wxl_on_text_change(wxl_handle control, uint64_t token);
WXL_API int wxl_off_text_change(wxl_handle control);
WXL_API int wxl_off_click(wxl_handle button);
WXL_API int wxl_on_click(wxl_handle button, uint64_t token);
/* Queue a normal button event; useful for lifecycle smoke tests. */
WXL_API int wxl_click(wxl_handle button);
#ifdef __cplusplus
}
#endif
#endif
