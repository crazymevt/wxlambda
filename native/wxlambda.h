#ifndef WXLAMBDA_H
#define WXLAMBDA_H
#include <stdint.h>
#include <stddef.h>
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
enum wxl_event_kind { WXL_EVENT_CLICK = 1, WXL_EVENT_TEXT_CHANGE = 2, WXL_EVENT_TOGGLE = 3, WXL_EVENT_SELECTION = 4 };
typedef struct wxl_event {
    int kind;
    const char *text; /* UTF-8 for text changes; NULL otherwise. */
    int checked;      /* Boolean snapshot for toggles; zero otherwise. */
    int selection;    /* Zero-based index, -1 for none; kind 4 only. */
} wxl_event;
/* The record and its text are borrowed only until this callback returns.
   Callbacks must never unwind across this boundary. */
typedef void (*wxl_callback)(uint64_t token, const wxl_event *event);
WXL_API int wxl_abi_version(void);
/* Error text is borrowed until the next bridge call on this thread. */
WXL_API const char *wxl_last_error(void);
WXL_API int wxl_start(wxl_callback callback);
WXL_API int wxl_run(void);
/* Process pending events without blocking; forbidden inside wxl_run. */
WXL_API int wxl_poll(void);
WXL_API int wxl_stop(void);
WXL_API int wxl_finish(void);
WXL_API wxl_handle wxl_frame(const char *title, int width, int height);
WXL_API wxl_handle wxl_panel(wxl_handle parent);
WXL_API wxl_handle wxl_button(wxl_handle parent, const char *label);
WXL_API wxl_handle wxl_text(wxl_handle parent, const char *value);
WXL_API wxl_handle wxl_label(wxl_handle parent, const char *label);
WXL_API int wxl_set_label(wxl_handle label, const char *value);
/* Borrowed until the next wxl_get_label on this thread; NULL indicates failure. */
WXL_API const char *wxl_get_label(wxl_handle label);
WXL_API wxl_handle wxl_checkbox(wxl_handle parent, const char *label, int checked);
/* Returns 0/1 for unchecked/checked, or -1 on failure. */
WXL_API int wxl_checked(wxl_handle checkbox);
/* notify=0 is silent; nonzero synchronously emits a toggle event. */
WXL_API int wxl_set_checked(wxl_handle checkbox, int checked, int notify);
WXL_API int wxl_on_toggle(wxl_handle checkbox, uint64_t token);
WXL_API int wxl_off_toggle(wxl_handle checkbox);
/* Items are copied. selection=-1 means no selection. */
WXL_API wxl_handle wxl_choice(wxl_handle parent, const char **items, int count, int selection);
/* Returns -1 for no selection, -2 on error. */
WXL_API int wxl_selection(wxl_handle choice);
/* selection=-1 clears; notify!=0 emits synchronously, even if unchanged. */
WXL_API int wxl_set_selection(wxl_handle choice, int selection, int notify);
WXL_API int wxl_on_selection(wxl_handle choice, uint64_t token);
WXL_API int wxl_off_selection(wxl_handle choice);
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
/* Metadata getters return UTF-8 borrowed until the next wxl_name/wxl_title
   query on this thread, with its byte count excluding any terminator in length.
   length must be non-NULL. NULL return indicates failure; empty strings succeed.
   Setter byte counts also exclude the terminator and may include embedded NULs.
   Title operations accept frames only. */
WXL_API const char *wxl_name(wxl_handle window, size_t *length);
WXL_API int wxl_set_name(wxl_handle window, const char *value, size_t length);
WXL_API const char *wxl_title(wxl_handle frame, size_t *length);
WXL_API int wxl_set_title(wxl_handle frame, const char *value, size_t length);
WXL_API int wxl_show(wxl_handle window);
/* Boolean operations return 0/1, or -1 on error. Setters return whether the
   native state changed. wxl_show is retained with its original status contract. */
WXL_API int wxl_show_state(wxl_handle window, int show);
WXL_API int wxl_shown(wxl_handle window);
WXL_API int wxl_enable(wxl_handle window, int enable);
WXL_API int wxl_enabled(wxl_handle window);
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
