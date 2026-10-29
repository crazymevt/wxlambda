#pragma once
enum wxl_grid_event_kind {
    WXL_EVENT_GRID_CELL_CHANGING=160, WXL_EVENT_GRID_CELL_CHANGED=161,
    WXL_EVENT_GRID_SELECT_CELL=162, WXL_EVENT_GRID_RANGE_SELECTING=163,
    WXL_EVENT_GRID_RANGE_SELECTED=164, WXL_EVENT_GRID_EDITOR_SHOWN=165,
    WXL_EVENT_GRID_EDITOR_HIDDEN=166, WXL_EVENT_GRID_CELL_LEFT_CLICK=167,
    WXL_EVENT_GRID_CELL_RIGHT_CLICK=168, WXL_EVENT_GRID_CELL_LEFT_DOUBLE_CLICK=169,
    WXL_EVENT_GRID_CELL_RIGHT_DOUBLE_CLICK=170
};
WXL_API int wxl_grid_extra_get(wxl_handle id,int property,int row,int column,int64_t* value);
WXL_API int wxl_grid_extra_set(wxl_handle id,int property,int row,int column,int64_t value);
WXL_API int wxl_grid_editor_action(wxl_handle id,int action);
WXL_API int wxl_grid_table_action(wxl_handle id,int axis,int operation,int position,int count);
WXL_API int wxl_set_grid_cursor(wxl_handle id,int row,int column);
WXL_API int wxl_grid_select_block(wxl_handle id,int top,int left,int bottom,int right,int add);
WXL_API int wxl_grid_clear_selection(wxl_handle id);
WXL_API int wxl_grid_event_detail(uint64_t token,int field,int64_t* value);
WXL_API const char* wxl_grid_event_value(uint64_t token,size_t* length,int* available);
