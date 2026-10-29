/* Fixed-shape string grid. Fields: 0 cell(row,column), 1 row label(row),
   2 column label(column). Properties: 0 rows,1 columns,2 editable,3 editing.
   Read-only dimensions >=1; boolean0/1; integer getter -1 means error.
   Text uses explicit UTF-8 lengths; string getters return NULL on error.
   Status setters return0/-1 and require no active native cell editor. */
WXL_API wxl_handle wxl_grid(wxl_handle parent, int rows, int columns, int editable);
WXL_API int wxl_grid_property(wxl_handle id, int property);
WXL_API const char* wxl_grid_text(wxl_handle id, int field, int row, int column, size_t* length);
WXL_API int wxl_set_grid_text(wxl_handle id, int field, int row, int column, const char* text, size_t length);
WXL_API int wxl_set_grid_editable(wxl_handle id, int editable);
