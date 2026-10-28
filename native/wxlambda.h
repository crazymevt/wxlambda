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
enum wxl_event_family { WXL_FAMILY_COMMAND = 0, WXL_FAMILY_CLOSE = 1, WXL_FAMILY_NOTIFY = 2, WXL_FAMILY_IDLE = 3 };
enum wxl_event_kind { WXL_EVENT_CLICK = 1, WXL_EVENT_TEXT_CHANGE = 2, WXL_EVENT_TOGGLE = 3, WXL_EVENT_SELECTION = 4, WXL_EVENT_CLOSE = 5, WXL_EVENT_QUERY_END_SESSION = 6, WXL_EVENT_END_SESSION = 7, WXL_EVENT_IDLE = 8, WXL_EVENT_MENU = 9 };
typedef struct wxl_event {
    int kind;
    const char *text; /* UTF-8 for text changes and registered command/notify tags. */
    int checked;      /* Raw IsChecked for toggle/menu payloads; zero otherwise. */
    int selection;    /* Zero-based index, -1 for none; kind 4 only. */
    int id;
    size_t text_length; /* Explicit UTF-8 byte count when text is supplied. */
    uint64_t context; /* Scoped opaque token, zero for observers. */
    int native_type;
    int can_veto; /* Close or notify capability. */
    int vetoed;
    int family;
    int integer;
    int logging_off;
    int64_t timestamp; /* Native signed long, widened without interpreting its epoch. */
    int command;
    wxl_handle origin_id; /* Known bridge identity only; zero for null/untracked. */
    int category;
    int64_t extra_long; /* Native signed long, copied for command/notify events. */
    int selection_action; /* Native IsSelection, not a predicate on the kind tag. */
    uint64_t user_data; /* Binding root token, zero for none; never a native pointer. */
    int more_requested; /* Native idle RequestMore state; false for other families. */
} wxl_event;
/* The record and its text are borrowed only until this callback returns.
   Callbacks must never unwind across this boundary. */
typedef void (*wxl_callback)(uint64_t token, const wxl_event *event);
WXL_API int wxl_abi_version(void);
/* Error text is borrowed until the next bridge call on this thread. */
WXL_API const char *wxl_last_error(void);
/* Standard clipboard only. Each operation opens/closes; no exposed native ownership.
   Queries/flush: 0/1, -1 error. Read returns borrowed UTF-8 plus length/availability;
   null pointer means error, available=0 means no text, available=1 includes empty.
   Setter replaces all formats, rejects NUL. Flush false means not persisted. */
WXL_API int wxl_clipboard_text_available(void);
WXL_API const char* wxl_clipboard_text(size_t* length, int* available);
WXL_API int wxl_set_clipboard_text(const char* value, size_t length);
WXL_API int wxl_flush_clipboard(void);
WXL_API int wxl_start(wxl_callback callback);
WXL_API int wxl_run(void);
/* GUI dispatch is forbidden inside run, a pump or any Lisp callback. */
WXL_API int wxl_poll(void);
/* Boolean queries: 0/1 on success, -1 on failure. Active session required. */
WXL_API int wxl_main_loop_running(void);
WXL_API int wxl_pending_events(void);
/* Dispatch queued wx events only, without OS events or idle processing. */
WXL_API int wxl_process_pending_events(void);
WXL_API int wxl_stop(void);
WXL_API int wxl_finish(void);
WXL_API wxl_handle wxl_message_dialog(wxl_handle parent, const char *message, size_t message_length, const char *title, size_t title_length, int buttons, int icon);
WXL_API wxl_handle wxl_file_dialog(wxl_handle parent, const char *title, size_t title_length, const char *directory, size_t directory_length, const char *filename, size_t filename_length, const char *wildcard, size_t wildcard_length, int mode, int must_exist, int overwrite_prompt);
WXL_API const char *wxl_dialog_message(wxl_handle dialog, size_t *length);
WXL_API int wxl_message_dialog_buttons(wxl_handle dialog);
WXL_API int wxl_file_dialog_mode(wxl_handle dialog);
WXL_API const char *wxl_file_dialog_path(wxl_handle dialog, size_t *length);
WXL_API int wxl_set_file_dialog_path(wxl_handle dialog, const char *path, size_t length);
WXL_API int wxl_file_dialog_filter_index(wxl_handle dialog);
WXL_API int wxl_set_file_dialog_filter_index(wxl_handle dialog, int index);
WXL_API int wxl_dialog_depth(void);
/* Private Lisp error containment latch: prevents new internal modal entry until rethrow. */
WXL_API int wxl_callback_error_pending(int pending);
WXL_API wxl_handle wxl_text_entry_dialog(wxl_handle parent, const char *message, size_t message_length, const char *title, size_t title_length, const char *value, size_t value_length);
WXL_API wxl_handle wxl_single_choice_dialog(wxl_handle parent, const char *message, size_t message_length, const char *title, size_t title_length, const char **items, size_t count, int selection);
/* Show/End codes: 0=OK,1=CANCEL,2=YES,3=NO. Message success requires an offered button; cancellation always allowed. End does not perform validation/transfer. */
WXL_API int wxl_dialog_show(wxl_handle dialog, int *code);
WXL_API int wxl_dialog_end(wxl_handle dialog, int code);
WXL_API int wxl_dialog_abort(void);
WXL_API int wxl_dialog_modal(wxl_handle dialog);
WXL_API const char *wxl_dialog_title(wxl_handle dialog, size_t *length);
WXL_API int wxl_set_dialog_title(wxl_handle dialog, const char *title, size_t length);
WXL_API const char *wxl_dialog_value(wxl_handle dialog, size_t *length);
WXL_API int wxl_set_dialog_value(wxl_handle dialog, const char *value, size_t length);
WXL_API int wxl_dialog_selection(wxl_handle dialog);
WXL_API int wxl_set_dialog_selection(wxl_handle dialog, int selection);
WXL_API const char *wxl_dialog_selection_text(wxl_handle dialog, size_t *length);
WXL_API wxl_handle wxl_menu_bar(wxl_handle frame);
WXL_API wxl_handle wxl_menu(wxl_handle bar, const char *label, size_t length);
WXL_API wxl_handle wxl_menu_item(wxl_handle menu, int id, const char *label, size_t length, const char *help, size_t help_length, int kind);
WXL_API int wxl_menu_alive(wxl_handle resource);
WXL_API int wxl_menu_destroy(wxl_handle resource);
WXL_API int wxl_menu_count(wxl_handle resource);
WXL_API int wxl_menu_item_id(wxl_handle item, int *value);
WXL_API int wxl_menu_item_kind(wxl_handle item);
WXL_API const char *wxl_menu_label(wxl_handle resource, size_t *length);
WXL_API int wxl_set_menu_label(wxl_handle resource, const char *text, size_t length);
WXL_API int wxl_menu_enabled(wxl_handle item);
WXL_API int wxl_set_menu_enabled(wxl_handle item, int value);
WXL_API int wxl_menu_checked(wxl_handle item);
WXL_API int wxl_set_menu_checked(wxl_handle item, int value);
WXL_API int wxl_activate_menu(wxl_handle item);
WXL_API int wxl_idle_mode(void);
WXL_API int wxl_set_idle_mode(int mode);
WXL_API int wxl_window_process_idle(wxl_handle window);
WXL_API int wxl_set_window_process_idle(wxl_handle window, int enabled);
WXL_API int wxl_wake_up_idle(void);
/* Standalone handlers are session-owned; window handlers borrow window identity.
   Queries return 0/1, or -1 on failure. ALIVE returns 0 for stale/unknown IDs. */
WXL_API wxl_handle wxl_application_handler(void);
WXL_API wxl_handle wxl_event_handler(void);
/* Reciprocal managed standalone chains; no borrowed app/window endpoints. */
WXL_API int wxl_handler_chain_peer(wxl_handle handler, int previous, wxl_handle *peer);
WXL_API int wxl_handler_unlinked(wxl_handle handler);
WXL_API int wxl_link_handlers(wxl_handle handler, wxl_handle peer, int previous);
WXL_API int wxl_unlink_handler(wxl_handle handler);
WXL_API int wxl_handler_alive(wxl_handle handler);
WXL_API int wxl_handler_enabled(wxl_handle handler);
WXL_API int wxl_set_handler_enabled(wxl_handle handler, int enabled);
/* Client objects hold opaque integer tokens only, never Lisp/native pointers.
   Get/set return 0/-1; token zero denotes clear/no object. Active returns
   0/1/-1 and scans still-associated bridge objects, including deferred deletion.
   Foreign typed objects and all untyped storage modes reject get/set. */
WXL_API int wxl_handler_client_data(wxl_handle handler, uint64_t *token);
WXL_API int wxl_set_handler_client_data(wxl_handle handler, uint64_t token);
WXL_API int wxl_client_data_active(uint64_t token);
/* Only standalone handlers may be destroyed here. */
WXL_API int wxl_handler_destroy(wxl_handle handler);
/* Independent bindings for supported command/close event kinds. Bind/queue
   return 0 or -1. Unbind/active return 0/1 or -1. Filters use -1 for wxID_ANY.
   Observer callbacks Skip; contextual callbacks control Skip. Native callback
   order is not guaranteed here. */
WXL_API int wxl_bind(wxl_handle handler, int kind, uint64_t token, int first_id, int last_id, int with_context, int user_data);
/* Contexts refer only to their currently executing innermost native callback; no raw event pointer
   escapes. Queries use 0/1/-1, stop returns the old level or -1, writers 0/-1. */
WXL_API int wxl_event_type(int kind);
/* Scoped reads capture the binding token before native self-unbinding can delete its adapter. */
WXL_API int wxl_context_user_data(uint64_t context, uint64_t *value);
WXL_API int wxl_event_user_data(wxl_handle event, uint64_t *value);
WXL_API int wxl_context_active(uint64_t context);
WXL_API int wxl_context_skipped(uint64_t context);
WXL_API int wxl_context_set_skipped(uint64_t context, int skip);
WXL_API int wxl_context_propagates(uint64_t context);
WXL_API int wxl_context_stop_propagation(uint64_t context);
WXL_API int wxl_context_set_can_veto(uint64_t context, int value);
WXL_API int wxl_event_category_mask(int category);
WXL_API int wxl_context_category(uint64_t context, int *value);
WXL_API int wxl_context_set_native_type(uint64_t context, int type);
WXL_API int wxl_context_native_type(uint64_t context, int *value);
WXL_API int wxl_context_origin_id(uint64_t context, wxl_handle *value);
WXL_API int wxl_context_id(uint64_t context, int *value);
WXL_API int wxl_context_set_id(uint64_t context, int value);
WXL_API int wxl_context_timestamp(uint64_t context, int64_t *value);
WXL_API int wxl_context_set_timestamp(uint64_t context, int64_t value);
WXL_API int wxl_context_command(uint64_t context);
WXL_API int wxl_context_can_veto(uint64_t context);
WXL_API int wxl_context_logging_off(uint64_t context);
WXL_API int wxl_context_set_logging_off(uint64_t context, int value);
WXL_API int wxl_context_vetoed(uint64_t context);
WXL_API int wxl_context_set_vetoed(uint64_t context, int vetoed);
WXL_API int wxl_close(wxl_handle frame, int force);
WXL_API int wxl_context_resume_propagation(uint64_t context, int level);
WXL_API int wxl_unbind(uint64_t token);
WXL_API int wxl_binding_active(uint64_t token);
/* Register returns private kind code or -1; process returns 0/1/-1 and copies
   final veto separately. A false handled result does not mean no observer ran. */
WXL_API int wxl_register_event_type(int family);
WXL_API int wxl_process_event(wxl_handle handler, int kind, int id, const char *text, size_t length, int value, int can_veto, int vetoed, int logging_off, int *final_veto);
WXL_API int wxl_queue_event(wxl_handle handler, int kind, int id, const char *text, size_t length, int value, int can_veto, int vetoed, int logging_off);
/* Owned event handles are independent resources. Default dispatch clones; explicit transfer consumes. */
WXL_API wxl_handle wxl_owned_event(int kind, int id, const char *text, size_t length, int value, int can_veto, int vetoed, int logging_off);
WXL_API wxl_handle wxl_clone_event(uint64_t source, int contextual);
WXL_API int wxl_event_alive(wxl_handle event);
WXL_API int wxl_event_destroy(wxl_handle event);
WXL_API int wxl_event_field(uint64_t source, int contextual, int field, int64_t *value);
WXL_API int wxl_set_event_field(uint64_t source, int contextual, int field, int64_t value);
WXL_API const char *wxl_event_text(uint64_t source, int contextual, size_t *length);
WXL_API int wxl_set_event_text(uint64_t source, int contextual, const char *text, size_t length);
WXL_API int wxl_owned_stop(wxl_handle event);
/* Modes: 0 process-copy, 1 queue-copy, 2 post-copy, 3 queue-transfer, 4 local-copy, 5 window-copy, 6 window-local-copy. */
WXL_API int wxl_delete_pending_events(wxl_handle receiver);
WXL_API int wxl_dispatch_owned(wxl_handle receiver, wxl_handle event, int mode, int *final_veto);
/* Window stacks borrow session-owned handlers. Window destruction detaches them. */
WXL_API int wxl_window_handler(wxl_handle window, wxl_handle *handler);
WXL_API int wxl_push_handler(wxl_handle window, wxl_handle handler);
WXL_API int wxl_pop_handler(wxl_handle window, int destroy, wxl_handle *handler);
WXL_API int wxl_remove_handler(wxl_handle window, wxl_handle handler);
WXL_API wxl_handle wxl_frame(const char *title, int width, int height);
WXL_API wxl_handle wxl_panel(wxl_handle parent);
WXL_API wxl_handle wxl_button(wxl_handle parent, const char *label);
WXL_API wxl_handle wxl_text(wxl_handle parent, const char *value);
WXL_API wxl_handle wxl_label(wxl_handle parent, const char *label);
WXL_API int wxl_set_label(wxl_handle label, const char *value);
/* Borrowed until the next wxl_get_label on this thread; NULL indicates failure. */
WXL_API const char *wxl_get_label(wxl_handle label);
/* Gauge getters return -1 on failure; values/ranges are nonnegative. */
WXL_API wxl_handle wxl_gauge(wxl_handle parent, int range, int value, int vertical);
WXL_API int wxl_gauge_value(wxl_handle gauge);
WXL_API int wxl_gauge_range(wxl_handle gauge);
WXL_API int wxl_set_gauge_value(wxl_handle gauge, int value);
WXL_API int wxl_set_gauge_range(wxl_handle gauge, int range);
WXL_API int wxl_gauge_vertical(wxl_handle gauge);
WXL_API int wxl_gauge_pulse(wxl_handle gauge);
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
