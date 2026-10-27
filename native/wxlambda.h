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
typedef void (*wxl_callback)(uint64_t token);
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
WXL_API int wxl_add(wxl_handle parent, wxl_handle child, int proportion);
WXL_API int wxl_show(wxl_handle window);
WXL_API int wxl_destroy(wxl_handle window);
WXL_API int wxl_set_text(wxl_handle control, const char *value);
WXL_API int wxl_on_click(wxl_handle button, uint64_t token);
/* Queue a normal button event; useful for lifecycle smoke tests. */
WXL_API int wxl_click(wxl_handle button);
#ifdef __cplusplus
}
#endif
#endif
