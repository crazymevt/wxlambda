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
enum wxl_event_family { WXL_FAMILY_COMMAND = 0, WXL_FAMILY_CLOSE = 1, WXL_FAMILY_NOTIFY = 2, WXL_FAMILY_IDLE = 3, WXL_FAMILY_BASE = 4 };
enum wxl_event_kind { WXL_EVENT_CLICK = 1, WXL_EVENT_TEXT_CHANGE = 2, WXL_EVENT_TOGGLE = 3, WXL_EVENT_SELECTION = 4, WXL_EVENT_CLOSE = 5, WXL_EVENT_QUERY_END_SESSION = 6, WXL_EVENT_END_SESSION = 7, WXL_EVENT_IDLE = 8, WXL_EVENT_MENU = 9, WXL_EVENT_NOTEBOOK_CHANGING = 10, WXL_EVENT_NOTEBOOK_CHANGED = 11, WXL_EVENT_LISTBOX_SELECTION = 12, WXL_EVENT_LISTBOX_DOUBLE_CLICK = 13, WXL_EVENT_TREE_CHANGING = 14, WXL_EVENT_TREE_CHANGED = 15, WXL_EVENT_SPLITTER_CHANGING = 16, WXL_EVENT_SPLITTER_CHANGED = 17, WXL_EVENT_SLIDER_CHANGE = 18, WXL_EVENT_SPIN_CHANGE = 19, WXL_EVENT_RADIO_SELECTION = 20, WXL_EVENT_TOGGLE_BUTTON_CHANGE = 21, WXL_EVENT_COMBO_SELECTION = 22, WXL_EVENT_DATE_CHANGE = 23, WXL_EVENT_TIME_CHANGE = 24, WXL_EVENT_SEARCH = 25, WXL_EVENT_SEARCH_CANCEL = 26, WXL_EVENT_WEB_MESSAGE = 27, WXL_EVENT_WEB_SCRIPT_RESULT = 28, WXL_EVENT_WEB_LOADED = 29, WXL_EVENT_WEB_ERROR = 30 };
typedef struct wxl_event {
    int kind;
    const char *text; /* UTF-8 for text changes, combo-box/toggle-button/radio-box/listbox events and registered command/notify tags. */
    int checked;      /* Raw IsChecked for checkbox/toggle-button/menu payloads; zero otherwise. */
    int selection;    /* Zero-based choice/listbox/radio-box/combo-box or native notebook event selection; -1 for none/other kinds. */
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
    wxl_handle tree_item; /* Checked bridge item token, zero for none/unmanaged/non-tree. */
    int sash_position; /* Native sash event position; meaningful only for splitter kinds. */
    int date_code; /* Copied YYYYMMDD for supported date events;0 for invalid/other. */
    int time_code; /* Copied1+seconds since midnight;0 for invalid/non-time. */
    int64_t detail[24]; /* immutable kind-specific native snapshot fields */
} wxl_event;
/* The record and its text are borrowed only until this callback returns.
   Callbacks must never unwind across this boundary. */
typedef void (*wxl_callback)(uint64_t token, const wxl_event *event);
/* Separate safe-idle callback: kind=0 releases an expired work root,
   1 invokes/removes an action, 2 invokes a periodic timer, 3 invokes/removes a one-shot.
   Never unwind across this boundary. No borrowed event context is active. */
typedef void (*wxl_ui_callback)(uint64_t token, int kind);
WXL_API int wxl_abi_version(void);
#include "drawing-canvas-api.h"
#include "activity-indicator-api.h"
#include "grid-api.h"
#include "grid-extra-api.h"
#include "webview-api.h"
#include "stc-api.h"
#include "input-events-api.h"
#include "thread-events-api.h"
#include "views-api.h"
#include "images-api.h"
#include "aui-api.h"
/* Editable single-line search text/hint: explicit UTF-8 lengths, NUL/CR/LF reject.
   Field0=text,1=hint; setter notify is valid only for text (one TEXT event).
   Search/cancel are native-only command kinds; no widget menu/button policy API. */
WXL_API wxl_handle wxl_search_control(wxl_handle parent, const char* value, size_t length, const char* hint, size_t hint_length);
WXL_API const char* wxl_search_field(wxl_handle id, int field, size_t* length);
WXL_API int wxl_set_search_field(wxl_handle id, int field, const char* value, size_t length, int notify);
/* Required local wall-clock time: hour0..23, minute/second0..59, no date/timezone.
   Getter copies1+seconds since midnight; silent unless notify=1 (wxDateEvent). */
WXL_API wxl_handle wxl_time_picker(wxl_handle parent, int hour, int minute, int second);
WXL_API int wxl_clock_time(wxl_handle id, int* code);
WXL_API int wxl_set_clock_time(wxl_handle id, int hour, int minute, int second, int notify);
/* Required Gregorian date1900..2100, month1..12, valid day; fixed inclusive range.
   Getter copies YYYYMMDD; setters silent unless notify=1 (native wxDateEvent). */
WXL_API wxl_handle wxl_date_picker(wxl_handle parent, int year, int month, int day);
WXL_API int wxl_calendar_date(wxl_handle id, int* code);
WXL_API int wxl_set_calendar_date(wxl_handle id, int year, int month, int day, int notify);
/* Borrowed UTF-8 until next bridge call; presence distinguishes absent/empty.
   Setter accepts0..4096 UTF-8 bytes, NUL-free; present=0 removes (length must0).
   Mutations reject callbacks. No native tooltip resource/pointer is exposed. */
WXL_API const char* wxl_tooltip_text(wxl_handle id, size_t* length, int* present);
WXL_API int wxl_set_tooltip_text(wxl_handle id, const char* value, size_t length, int present);
/* Shown/enabled windows only; defer from callbacks. Focus does not guarantee OS activation. */
WXL_API int wxl_focus_window(wxl_handle id, int keyboard);
/* 0/1 native handled result, -1 error. */
WXL_API int wxl_navigate_focus(wxl_handle id, int forward);
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
WXL_API int wxl_start(wxl_callback callback, wxl_ui_callback ui_callback);
WXL_API int wxl_defer_ui_action(uint64_t token, wxl_handle owner);
WXL_API int wxl_cancel_ui_action(uint64_t token);
WXL_API int wxl_ui_action_pending(uint64_t token);
/* Session timers: 1..86400000ms, one_shot=0/1; at most256 live tickets.
   Native ticks coalesce; functions share the safe idle action phase. */
WXL_API int wxl_start_ui_timer(uint64_t token, int milliseconds, int one_shot, wxl_handle owner);
WXL_API int wxl_cancel_ui_timer(uint64_t token);
WXL_API int wxl_ui_timer_active(uint64_t token);
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
WXL_API wxl_handle wxl_directory_dialog(wxl_handle parent,const char* title,size_t title_length,const char* path,size_t path_length);
WXL_API const char* wxl_directory_dialog_path(wxl_handle id,size_t* length);
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
/* Frame accelerator entries: bridge key ASCII A..Z/0..9 or 1001..1012 (F1..F12),
   flags control=1 shift=2 alt=4 raw-control=8, portable positive ID <=32767.
   Arrays copied; <=256 entries. Empty clears. Quiescent replacement only. */
WXL_API int wxl_set_frame_accelerators(wxl_handle frame, const int* keys, const int* flags, const int* commands, size_t count);
WXL_API int wxl_frame_accelerator_count(wxl_handle frame);
WXL_API int wxl_frame_accelerator_entry(wxl_handle frame, int index, int* key, int* flags, int* command);
/* Frame-owned horizontal toolbar and fixed 1..16 field status bar. */
WXL_API wxl_handle wxl_toolbar(wxl_handle frame);
WXL_API wxl_handle wxl_status_bar(wxl_handle frame, int fields);
WXL_API int wxl_add_tool(wxl_handle bar, int id, const char* label, size_t length, int art, int kind, const char* help, size_t help_length);
WXL_API int wxl_realize_toolbar(wxl_handle bar);
WXL_API int wxl_tool_count(wxl_handle bar);
WXL_API int wxl_tool_state(wxl_handle bar, int id, int field);
WXL_API int wxl_set_tool_state(wxl_handle bar, int id, int field, int value);
WXL_API int wxl_activate_tool(wxl_handle bar, int id);
WXL_API int wxl_status_fields(wxl_handle bar);
WXL_API const char* wxl_status_text(wxl_handle bar, int field, size_t* length);
WXL_API int wxl_set_status_text(wxl_handle bar, int field, const char* text, size_t length);
WXL_API wxl_handle wxl_menu_bar(wxl_handle frame);
WXL_API wxl_handle wxl_menu(wxl_handle bar, const char *label, size_t length);
WXL_API wxl_handle wxl_menu_item(wxl_handle menu, int id, const char *label, size_t length, const char *help, size_t help_length, int kind);
WXL_API int wxl_popup_choice(wxl_handle id,const char** labels,const int* enabled,size_t count,int x,int y,int* selected);
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
/* Fixed two-panel splitters. Integer fields: sash=0, minimum=1, orientation=2,
   is-split=3. Mutable pixels are [0,INT_MAX/4], minimum positive. Status 0/-1. */
WXL_API wxl_handle wxl_splitter(wxl_handle parent, int orientation, int position, int minimum, double gravity);
WXL_API int wxl_splitter_attach(wxl_handle splitter, wxl_handle first, wxl_handle second);
WXL_API int wxl_splitter_pane(wxl_handle splitter, int index, wxl_handle *pane);
WXL_API int wxl_splitter_int(wxl_handle splitter, int field, int *value);
WXL_API int wxl_set_splitter_int(wxl_handle splitter, int field, int value);
WXL_API int wxl_splitter_gravity(wxl_handle splitter, double *gravity);
WXL_API int wxl_set_splitter_gravity(wxl_handle splitter, double gravity);
WXL_API int wxl_event_sash(uint64_t context, int *position);
WXL_API int wxl_set_event_sash(uint64_t context, int position);
/* Managed tree item tokens are not raw wxTreeItemId pointers. Status: 0/-1.
   wxQt tree creation is unavailable pending generation-safe async event translation. */
WXL_API wxl_handle wxl_tree(wxl_handle parent);
WXL_API wxl_handle wxl_tree_with_selection(wxl_handle parent,int multiple);
WXL_API int wxl_tree_selections(wxl_handle owner,wxl_handle* items,size_t capacity,size_t* count);
WXL_API int wxl_set_tree_selections(wxl_handle owner,const wxl_handle* items,size_t count);
WXL_API int wxl_tree_add(wxl_handle tree, wxl_handle parent, const char *text, size_t length, wxl_handle *item);
WXL_API int wxl_tree_query(wxl_handle tree, int field, wxl_handle *item); /* 0=root, 1=selection */
WXL_API int wxl_tree_relative(wxl_handle item, int field, wxl_handle *relative); /* 0=parent, 1=first child, 2=next sibling */
WXL_API const char* wxl_tree_text(wxl_handle item, size_t *length);
WXL_API int wxl_set_tree_text(wxl_handle item, const char *text, size_t length);
WXL_API int wxl_tree_expanded(wxl_handle item); /* boolean, -1 error */
WXL_API int wxl_set_tree_expanded(wxl_handle item, int expanded);
WXL_API int wxl_tree_delete(wxl_handle item);
WXL_API int wxl_tree_select(wxl_handle tree, wxl_handle item, int notify);
WXL_API int wxl_tree_item_alive(wxl_handle item); /* boolean, -1 error */
WXL_API int wxl_tree_revision(uint64_t *revision);
WXL_API int wxl_event_tree_item(uint64_t source, int contextual, wxl_handle *item);
/* Unsorted single-selection text lists. Item operations are silent.
   Count/append: -1 error. Selection: -1 none, -2 error. Text: NULL error.
   Other item/selection status operations: 0 success, -1 error. */
WXL_API wxl_handle wxl_list_box(wxl_handle parent, const char **items, const size_t *lengths, size_t count, int selection);
WXL_API int wxl_list_count(wxl_handle list);
WXL_API const char *wxl_list_item(wxl_handle list, int index, size_t *length);
WXL_API int wxl_set_list_item(wxl_handle list, int index, const char *value, size_t length);
WXL_API int wxl_list_append(wxl_handle list, const char *value, size_t length);
WXL_API int wxl_list_delete(wxl_handle list, int index);
WXL_API int wxl_list_clear(wxl_handle list);
WXL_API int wxl_list_selection(wxl_handle list);
WXL_API int wxl_set_list_selection(wxl_handle list, int selection, int notify);
WXL_API int wxl_on_list_selection(wxl_handle list, uint64_t token);
WXL_API int wxl_off_list_selection(wxl_handle list);
/* Fixed setup-time panel pages; no page removal/reparenting or raw ownership. */
WXL_API wxl_handle wxl_notebook(wxl_handle parent);
WXL_API int wxl_notebook_add(wxl_handle book, wxl_handle page, const char *label, size_t length, int select);
WXL_API int wxl_notebook_count(wxl_handle book);
WXL_API wxl_handle wxl_notebook_page(wxl_handle book, int index);
WXL_API const char* wxl_notebook_label(wxl_handle book, int index, size_t *length);
WXL_API int wxl_set_notebook_label(wxl_handle book, int index, const char *label, size_t length);
/* Selection: -1 empty, -2 error; count: -1 error; page handle: 0 error. */
WXL_API int wxl_notebook_selection(wxl_handle book);
WXL_API int wxl_set_notebook_selection(wxl_handle book, int index, int notify);
WXL_API wxl_handle wxl_text(wxl_handle parent, const char *value, size_t length, int multiline, int editable);
WXL_API wxl_handle wxl_label(wxl_handle parent, const char *label);
WXL_API int wxl_set_label(wxl_handle label, const char *value);
/* Borrowed until the next wxl_get_label on this thread; NULL indicates failure. */
WXL_API const char *wxl_get_label(wxl_handle label);
/* Gauge getters return -1 on failure; values/ranges are nonnegative. */
/* Numeric kind0 slider, kind1 integer spin; bounded min<max, endpoints +/-1000000.
   Fields0 value,1 minimum,2 maximum,3 slider vertical; range writes clamp silently. */
WXL_API wxl_handle wxl_numeric(wxl_handle parent, int kind, int minimum, int maximum, int value, int vertical);
WXL_API int wxl_numeric_field(wxl_handle control, int field, int* value);
WXL_API int wxl_set_numeric_value(wxl_handle control, int value, int notify);
WXL_API int wxl_set_numeric_range(wxl_handle control, int minimum, int maximum);
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
/* notify=0 is silent; notify=1 may synchronously invoke text-change callbacks. */
WXL_API int wxl_set_text(wxl_handle control, const char *value, size_t length, int notify);
/* UTF-8 text borrowed until the next metadata/string-result call on this thread.
   NULL indicates failure; an empty string indicates a successful empty value. */
WXL_API const char *wxl_get_text(wxl_handle control, size_t *length);
/* Text positions are wxWidgets native offsets, not portable Lisp string indices. */
WXL_API int wxl_text_multiline(wxl_handle control);
WXL_API int wxl_text_editable(wxl_handle control);
WXL_API int wxl_set_text_editable(wxl_handle control, int editable);
WXL_API int wxl_text_selection(wxl_handle control, int64_t *from, int64_t *to);
WXL_API int wxl_set_text_selection(wxl_handle control, int64_t from, int64_t to);
/* field=0 insertion point, field=1 native end. No -1 position sentinels. */
WXL_API int wxl_text_position(wxl_handle control, int field, int64_t *position);
WXL_API int wxl_set_text_insertion(wxl_handle control, int64_t position);
WXL_API int wxl_replace_text(wxl_handle control, int64_t from, int64_t to, const char *value, size_t length);
WXL_API int wxl_on_text_change(wxl_handle control, uint64_t token);
WXL_API int wxl_off_text_change(wxl_handle control);
WXL_API int wxl_off_click(wxl_handle button);
WXL_API int wxl_on_click(wxl_handle button, uint64_t token);
/* Queue a normal button event; useful for lifecycle smoke tests. */
WXL_API int wxl_click(wxl_handle button);

/* Scrolled-panel pairs: field0 pixels/unit, field1 view-start units, field2 extent pixels.
   Setters allow fields0/1 only; native scrolling can clamp requested positions. */
WXL_API wxl_handle wxl_scrolled_panel(wxl_handle parent, int xrate, int yrate);
WXL_API int wxl_scroll_pair(wxl_handle control, int field, int* x, int* y);
WXL_API int wxl_set_scroll_pair(wxl_handle control, int field, int x, int y);
WXL_API int wxl_fit_scroll_content(wxl_handle control);
WXL_API wxl_handle wxl_combo_box(wxl_handle parent, const char** items, const size_t* lengths, size_t count, const char* value, size_t value_length, int selection);
/* field0 selection (-1 none), field1 count; -2 error. */
WXL_API int wxl_combo_field(wxl_handle control, int field);
WXL_API const char* wxl_combo_text(wxl_handle control, size_t* length);
WXL_API const char* wxl_combo_item(wxl_handle control, int index, size_t* length);
WXL_API int wxl_set_combo_text(wxl_handle control, const char* value, size_t length, int notify);
WXL_API int wxl_set_combo_selection(wxl_handle control, int index, int notify);
WXL_API wxl_handle wxl_toggle_button(wxl_handle parent, const char* label, int checked);
WXL_API int wxl_toggle_button_checked(wxl_handle control);
WXL_API int wxl_set_toggle_button_checked(wxl_handle control, int checked, int notify);
WXL_API const char* wxl_toggle_button_label(wxl_handle control);
WXL_API int wxl_set_toggle_button_label(wxl_handle control, const char* label);
WXL_API wxl_handle wxl_radio_box(wxl_handle parent, const char* label, const char** items, int count, int selection, int horizontal);
WXL_API int wxl_radio_field(wxl_handle control, int field);
WXL_API const char* wxl_radio_item(wxl_handle control, int index);
WXL_API int wxl_set_radio_selection(wxl_handle control, int index, int notify);

#ifdef __cplusplus
}
#endif
#endif
