// Included in the native ABI block after checked window/operation helpers.
void grid_position(wxGrid* grid, int field, int row, int column) {
    require(field>=0 && field<=2,"Invalid grid text field");
    if (field!=2) require(row>=0 && row<grid->GetNumberRows(),"Grid row out of bounds");
    if (field!=1) require(column>=0 && column<grid->GetNumberCols(),"Grid column out of bounds");
}
void grid_mutation_ready(wxGrid* grid) {
    require(callback_depth==0,"Defer grid mutations until callbacks return");
    require(!grid->IsCellEditControlEnabled(),"Finish or cancel the native cell edit before changing grid properties");
}
wxl_handle wxl_grid(wxl_handle parent_id, int rows, int columns, int editable) {
    return create([&] {
        auto* parent=window(parent_id);
        require(rows>=1 && rows<=256 && columns>=1 && columns<=256 && rows*columns<=16384,"Grid dimensions require1..256 and at most16384 cells");
        require(editable==0 || editable==1,"Invalid grid editable flag");
        struct CreationScope { CreationScope() { ++input_creation_depth; } ~CreationScope() { --input_creation_depth; } } creation;
        auto object=std::make_unique<ManagedWindow<wxGrid>>();
        { wxEventBlocker blocker(object.get());
          require(object->Create(parent,wxID_ANY),"Native grid creation failed");
          require(object->CreateGrid(rows,columns),"Native grid table creation failed");
          object->EnableEditing(editable!=0); }
        const auto id=track(object.get()); input_owners.emplace(object.get(),false); object.release(); return id;
    });
}
int wxl_grid_property(wxl_handle id, int property) {
    int value=-1;
    const auto result=status([&] {
        auto* grid=as<wxGrid>(id);
        require(property>=0 && property<=3,"Invalid grid property");
        value=property==0 ? grid->GetNumberRows() : property==1 ? grid->GetNumberCols() :
              property==2 ? int(grid->IsEditable()) : int(grid->IsCellEditControlEnabled());
    });
    return result==0 ? value : -1;
}
const char* wxl_grid_text(wxl_handle id, int field, int row, int column, size_t* length) {
    return metadata_string([&] {
        auto* grid=as<wxGrid>(id); grid_position(grid,field,row,column);
        return field==0 ? grid->GetCellValue(row,column) : field==1 ? grid->GetRowLabelValue(row) : grid->GetColLabelValue(column);
    },length);
}
int wxl_set_grid_text(wxl_handle id, int field, int row, int column, const char* value, size_t length) { return status([&] {
    auto* grid=as<wxGrid>(id); grid_position(grid,field,row,column);
    require(length<=65536,"Grid text exceeds65536 UTF-8 bytes"); const auto text=text_value(value,length);
    grid_mutation_ready(grid); InputOperation operation(grid); wxEventBlocker blocker(grid);
    if (field==0) grid->SetCellValue(row,column,text);
    else if (field==1) grid->SetRowLabelValue(row,text);
    else grid->SetColLabelValue(column,text);
}); }
int wxl_set_grid_editable(wxl_handle id, int editable) { return status([&] {
    auto* grid=as<wxGrid>(id); require(editable==0 || editable==1,"Invalid grid editable flag");
    grid_mutation_ready(grid); InputOperation operation(grid); wxEventBlocker blocker(grid);
    grid->EnableEditing(editable!=0);
}); }
