// Session-owned wxImage/wxBitmap values; never expose adopted native pointers.
namespace {
std::unordered_map<wxl_handle,wxImage> image_resources;
std::unordered_map<wxl_handle,wxBitmap> bitmap_resources;
size_t image_pixels(int width,int height) {
    require(width>=1 && width<=4096 && height>=1 && height<=4096,"Image dimensions require1..4096 pixels");
    return size_t(width)*height;
}
wxImage& image_resource(wxl_handle id) {
    ready(); auto found=image_resources.find(id); require(found!=image_resources.end(),"Invalid or disposed image"); return found->second;
}
wxBitmap& bitmap_resource(wxl_handle id) {
    ready(); auto found=bitmap_resources.find(id); require(found!=bitmap_resources.end(),"Invalid or disposed bitmap"); return found->second;
}
void image_budget(size_t pixels) {
    size_t used=0; for (const auto& entry:image_resources) used+=size_t(entry.second.GetWidth())*entry.second.GetHeight();
    for (const auto& entry:bitmap_resources) used+=size_t(entry.second.GetWidth())*entry.second.GetHeight();
    require(image_resources.size()+bitmap_resources.size()<256 && pixels<=67108864-used,"Image resource session budget exceeded");
}
wxl_handle publish_image(wxImage value) {
    require(value.IsOk(),"Native image operation failed"); image_pixels(value.GetWidth(),value.GetHeight());
    image_budget(size_t(value.GetWidth())*value.GetHeight());
    if (value.HasMask()) { if (!value.HasAlpha()) value.InitAlpha(); require(value.GetAlpha(),"Mask alpha allocation failed"); value.SetMask(false); }
    const auto id=next_handle++; image_resources.emplace(id,std::move(value)); return id;
}
wxl_handle publish_bitmap(wxBitmap value) {
    require(value.IsOk(),"Native bitmap operation failed"); image_pixels(value.GetWidth(),value.GetHeight());
    image_budget(size_t(value.GetWidth())*value.GetHeight());
    const auto id=next_handle++; bitmap_resources.emplace(id,std::move(value)); return id;
}
void clear_image_resources() { bitmap_resources.clear(); image_resources.clear(); }
void image_mutation() { ready(); require(callback_depth==0,"Defer image/bitmap writes until callbacks return"); }
}
extern "C" {
wxl_handle wxl_image(int width,int height,const unsigned char* rgb,size_t rgb_length,const unsigned char* alpha,size_t alpha_length) { return create([&] {
    image_mutation(); const auto pixels=image_pixels(width,height);
    require(rgb && rgb_length==pixels*3 && ((!alpha && !alpha_length) || (alpha && alpha_length==pixels)),"Image requires exact RGB and optional alpha bytes");
    wxImage image(width,height,false); require(image.IsOk(),"Image allocation failed"); std::memcpy(image.GetData(),rgb,rgb_length);
    if (alpha) { image.InitAlpha(); require(image.GetAlpha(),"Alpha allocation failed"); std::memcpy(image.GetAlpha(),alpha,pixels); }
    return publish_image(std::move(image));
}); }
wxl_handle wxl_load_image(const char* path,size_t length) { return create([&] {
    image_mutation(); const auto filename=text_value(path,length); require(!filename.empty(),"Image path is empty");
    wxLogNull silence; wxFFileInputStream file(filename); require(file.IsOk(),"Cannot open PNG image");
    require(file.GetLength()>=24 && file.GetLength()<=67108864,"PNG input requires24 bytes..64MiB");
    unsigned char header[24]; file.Read(header,sizeof(header)); require(file.LastRead()==sizeof(header),"Truncated PNG header");
    const unsigned char signature[]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82};
    require(std::memcmp(header,signature,sizeof(signature))==0,"Only PNG images are supported");
    auto dimension=[&](int offset) { uint32_t value=0; for (int i=0;i<4;++i) value=(value<<8)|header[offset+i]; require(value<=4096,"PNG dimension exceeds4096"); return int(value); };
    image_pixels(dimension(16),dimension(20)); require(file.SeekI(0)!=wxInvalidOffset,"Cannot rewind PNG input");
    wxImage image; wxPNGHandler handler; require(handler.LoadFile(&image,file,false),"PNG decoding failed"); return publish_image(std::move(image));
}); }
wxl_handle wxl_scale_image(wxl_handle id,int width,int height,int quality) { return create([&] {
    image_mutation(); auto& image=image_resource(id); image_pixels(width,height); require(quality==0 || quality==1,"Image quality must be nearest or high");
    return publish_image(image.Scale(width,height,quality ? wxIMAGE_QUALITY_HIGH : wxIMAGE_QUALITY_NEAREST));
}); }
int wxl_image_size(wxl_handle id,int bitmap,int* width,int* height) { return status([&] {
    require(width && height && (bitmap==0 || bitmap==1),"Invalid image size outputs/type");
    if (bitmap) { auto& value=bitmap_resource(id); *width=value.GetWidth(); *height=value.GetHeight(); }
    else { auto& value=image_resource(id); *width=value.GetWidth(); *height=value.GetHeight(); }
}); }
int wxl_image_copy_bytes(wxl_handle id,int alpha,unsigned char* output,size_t length) { return status([&] {
    auto& image=image_resource(id); require(alpha==0 || alpha==1,"Invalid image channel"); const auto pixels=image_pixels(image.GetWidth(),image.GetHeight());
    require(output && length==pixels*(alpha ? 1:3),"Image output has wrong byte length");
    if (alpha && !image.HasAlpha()) std::memset(output,255,length);
    else std::memcpy(output,alpha ? image.GetAlpha():image.GetData(),length);
}); }
int wxl_image_has_alpha(wxl_handle id) { return boolean_result([&] { return image_resource(id).HasAlpha(); }); }
int wxl_image_set_bytes(wxl_handle id,int alpha,const unsigned char* input,size_t length) { return status([&] {
    image_mutation(); auto& image=image_resource(id); require(alpha==0 || alpha==1,"Invalid image channel"); const auto pixels=image_pixels(image.GetWidth(),image.GetHeight());
    if (alpha && !input && !length) { auto copy=image.Copy(); require(copy.IsOk(),"Image copy failed"); copy.ClearAlpha(); image=std::move(copy); return; }
    require(input && length==pixels*(alpha ? 1:3),"Image input has wrong byte length");
    // Copy-on-write before mutating bytes: bitmap and image copies are independent.
    auto copy=image.Copy(); require(copy.IsOk(),"Image copy failed");
    if (alpha && !copy.HasAlpha()) copy.InitAlpha();
    if (alpha) require(copy.GetAlpha(),"Alpha allocation failed");
    std::memcpy(alpha ? copy.GetAlpha():copy.GetData(),input,length); image=std::move(copy);
}); }
int wxl_image_pixel(wxl_handle id,int x,int y,unsigned char* rgba,int write) { return status([&] {
    auto& image=image_resource(id); require(rgba && x>=0 && x<image.GetWidth() && y>=0 && y<image.GetHeight() && (write==0 || write==1),"Invalid image pixel coordinate/output");
    if (write) { image_mutation(); auto copy=image.Copy(); require(copy.IsOk(),"Image copy failed"); if (rgba[3]!=255 && !copy.HasAlpha()) { copy.InitAlpha(); require(copy.GetAlpha(),"Alpha allocation failed"); } copy.SetRGB(x,y,rgba[0],rgba[1],rgba[2]); if (copy.HasAlpha()) copy.SetAlpha(x,y,rgba[3]); image=std::move(copy); }
    else { rgba[0]=image.GetRed(x,y); rgba[1]=image.GetGreen(x,y); rgba[2]=image.GetBlue(x,y); rgba[3]=image.HasAlpha() ? image.GetAlpha(x,y):255; }
}); }
int wxl_save_image(wxl_handle id,const char* path,size_t length) { return status([&] {
    image_mutation(); auto image=image_resource(id).Copy(); const auto filename=text_value(path,length); require(!filename.empty(),"Image path is empty"); wxLogNull silence;
    wxMemoryOutputStream encoded; wxPNGHandler handler; require(handler.SaveFile(&image,encoded,false),"PNG encoding failed");
    wxFFileOutputStream file(filename); require(file.IsOk(),"Cannot open image output"); wxMemoryInputStream input(encoded); file.Write(input); const bool written=file.IsOk();
    FILE* output=file.GetFile()->Detach(); require(output,"Image output handle unavailable"); const bool flushed=std::fflush(output)==0; const bool closed=std::fclose(output)==0; require(written && flushed && closed,"PNG write/flush/close failed");
}); }
wxl_handle wxl_bitmap(wxl_handle image) { return create([&] { image_mutation(); return publish_bitmap(wxBitmap(image_resource(image))); }); }
wxl_handle wxl_copy_bitmap(wxl_handle bitmap) { return create([&] { image_mutation(); return publish_bitmap(bitmap_resource(bitmap)); }); }
wxl_handle wxl_bitmap_image(wxl_handle bitmap) { return create([&] { image_mutation(); return publish_image(bitmap_resource(bitmap).ConvertToImage()); }); }
int wxl_bitmap_geometry(wxl_handle id,int* logical,int* dip,double* scale) { return status([&] {
    auto& value=bitmap_resource(id); require(logical && dip && scale,"Missing bitmap geometry output");
    const auto size=value.GetLogicalSize(), units=value.GetDIPSize(); logical[0]=size.x; logical[1]=size.y; dip[0]=units.x; dip[1]=units.y; *scale=value.GetScaleFactor();
}); }
int wxl_image_alive(wxl_handle id,int bitmap) { return boolean_result([&] { ready(); require(bitmap==0 || bitmap==1,"Invalid image resource kind"); return bitmap ? bitmap_resources.count(id)!=0:image_resources.count(id)!=0; }); }
int wxl_image_destroy(wxl_handle id,int bitmap) { return status([&] { image_mutation(); require(bitmap==0 || bitmap==1,"Invalid image resource kind"); if (bitmap) { bitmap_resource(id); bitmap_resources.erase(id); } else { image_resource(id); image_resources.erase(id); } }); }
wxl_handle wxl_static_bitmap(wxl_handle parent,wxl_handle bitmap) { return create([&] {
    auto* owner=window(parent); image_mutation(); const auto value=bitmap_resource(bitmap); InputOperation guard(owner);
    auto control=std::make_unique<ManagedWindow<wxStaticBitmap>>(); struct Creation { Creation(){++input_creation_depth;} ~Creation(){--input_creation_depth;} } creation;
    require(control->Create(owner,wxID_ANY,value),"Native static bitmap creation failed"); const auto id=track(control.get()); control.release(); return id;
}); }
wxl_handle wxl_static_bitmap_get(wxl_handle id) { return create([&] { image_mutation(); return publish_bitmap(as<wxStaticBitmap>(id)->GetBitmap()); }); }
int wxl_static_bitmap_set(wxl_handle id,wxl_handle bitmap) { return status([&] { auto* control=as<wxStaticBitmap>(id); image_mutation(); const auto value=bitmap_resource(bitmap); InputOperation guard(control); control->SetBitmap(value); }); }
int wxl_display_count() { int count=0; const auto status_code=status([&] { ready(); count=static_cast<int>(wxDisplay::GetCount()); }); return status_code==0 ? count:-2; }
int wxl_display_info(int index,int client,int* rectangle,int* ppi,double* scale) { return status([&] {
    ready(); require(index>=0 && static_cast<unsigned>(index)<wxDisplay::GetCount() && rectangle && ppi && scale && (client==0 || client==1),"Invalid display index/outputs");
    wxDisplay display(static_cast<unsigned>(index)); const auto bounds=client ? display.GetClientArea():display.GetGeometry(); const auto resolution=display.GetPPI();
    rectangle[0]=bounds.x; rectangle[1]=bounds.y; rectangle[2]=bounds.width; rectangle[3]=bounds.height; ppi[0]=resolution.x; ppi[1]=resolution.y; *scale=display.GetScaleFactor();
}); }
int wxl_window_display(wxl_handle id) { int index=-1; const auto status_code=status([&] { index=wxDisplay::GetFromWindow(window(id)); }); return status_code==0 ? index:-2; }
int wxl_window_point(wxl_handle id,int operation,int x,int y,int* result) { return status([&] {
    auto* control=window(id); require(result && operation>=0 && operation<=4 && x>=-1000000 && x<=1000000 && y>=-1000000 && y<=1000000,"Invalid window coordinate operation/output");
    wxPoint point(x,y);
    if (operation==0) { const auto size=control->GetClientSize(); point=wxPoint(size.x,size.y); }
    else if (operation==1) point=control->ClientToScreen(point);
    else if (operation==2) point=control->ScreenToClient(point);
    else if (operation==3) point=control->FromDIP(point);
    else point=control->ToDIP(point);
    result[0]=point.x; result[1]=point.y;
}); }
int wxl_window_content_scale(wxl_handle id,double* scale) { return status([&] { require(scale,"Missing scale output"); *scale=window(id)->GetContentScaleFactor(); }); }
}

extern "C" {
int wxl_bitmap_toggle_supported() { return boolean_result([&] {
    ready();
#ifdef wxHAS_BITMAPTOGGLEBUTTON
    return true;
#else
    return false;
#endif
}); }
wxl_handle wxl_bitmap_toggle(wxl_handle parent_id,wxl_handle bitmap_id,int checked) { return create([&]() -> wxl_handle {
    image_mutation();
#ifdef wxHAS_BITMAPTOGGLEBUTTON
    auto* parent=window(parent_id);
    require(checked==0 || checked==1,"Invalid toggle state");
    const wxBitmap value=bitmap_resource(bitmap_id);
    struct CreationScope { CreationScope() { ++input_creation_depth; } ~CreationScope() { --input_creation_depth; } } creation;
    auto object=std::make_unique<ManagedWindow<wxBitmapToggleButton>>();
    { wxEventBlocker blocker(object.get());
      require(object->Create(parent,wxID_ANY,wxBitmapBundle::FromBitmap(value)),"Native bitmap toggle creation failed");
      object->SetValue(checked!=0); }
    const auto id=track(object.get()); input_owners.emplace(object.get(),false); object.release(); return id;
#else
    (void)parent_id; (void)bitmap_id; (void)checked;
    throw std::runtime_error("Bitmap toggle buttons are unavailable on this wxWidgets port");
#endif
}); }
wxl_handle wxl_bitmap_toggle_get(wxl_handle id) { return create([&]() -> wxl_handle {
    image_mutation();
#ifdef wxHAS_BITMAPTOGGLEBUTTON
    return publish_bitmap(as<wxBitmapToggleButton>(id)->GetBitmap());
#else
    (void)id; throw std::runtime_error("Bitmap toggle buttons are unavailable on this wxWidgets port");
#endif
}); }
int wxl_bitmap_toggle_set(wxl_handle id,wxl_handle bitmap_id) { return status([&] {
    image_mutation();
#ifdef wxHAS_BITMAPTOGGLEBUTTON
    auto* object=as<wxBitmapToggleButton>(id);
    const wxBitmap value=bitmap_resource(bitmap_id);
    InputOperation operation(object);
    object->SetBitmap(wxBitmapBundle::FromBitmap(value));
#else
    (void)id; (void)bitmap_id; throw std::runtime_error("Bitmap toggle buttons are unavailable on this wxWidgets port");
#endif
}); }
}
