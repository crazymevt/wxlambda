#pragma once
enum wxl_input_event_kind {
    WXL_EVENT_KEY_DOWN=45,
    WXL_EVENT_KEY_UP=46,
    WXL_EVENT_CHAR=47,
    WXL_EVENT_CHAR_HOOK=48,
    WXL_EVENT_LEFT_DOWN=49,
    WXL_EVENT_LEFT_UP=50,
    WXL_EVENT_LEFT_DOUBLE_CLICK=51,
    WXL_EVENT_MIDDLE_DOWN=52,
    WXL_EVENT_MIDDLE_UP=53,
    WXL_EVENT_MIDDLE_DOUBLE_CLICK=54,
    WXL_EVENT_RIGHT_DOWN=55,
    WXL_EVENT_RIGHT_UP=56,
    WXL_EVENT_RIGHT_DOUBLE_CLICK=57,
    WXL_EVENT_AUX1_DOWN=58,
    WXL_EVENT_AUX1_UP=59,
    WXL_EVENT_AUX1_DOUBLE_CLICK=60,
    WXL_EVENT_AUX2_DOWN=61,
    WXL_EVENT_AUX2_UP=62,
    WXL_EVENT_AUX2_DOUBLE_CLICK=63,
    WXL_EVENT_MOUSE_MOTION=64,
    WXL_EVENT_MOUSE_ENTER=65,
    WXL_EVENT_MOUSE_LEAVE=66,
    WXL_EVENT_MOUSE_WHEEL=67,
    WXL_EVENT_FOCUS_GAINED=68,
    WXL_EVENT_FOCUS_LOST=69,
    WXL_EVENT_CONTEXT_MENU=70,
    WXL_EVENT_SIZE=71
};

/* Read-only input detail; TOKEN is a borrowed innermost event-context token. */
WXL_API int wxl_input_detail(uint64_t token, int index, int64_t* value);
WXL_API int wxl_input_allow_next(uint64_t token);
/* Boolean result, -1 on error. CHAR_HOOK only. */
WXL_API int wxl_input_next_allowed(uint64_t token);
