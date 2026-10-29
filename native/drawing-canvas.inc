// Included after the bridge implementation; private compiled scene ownership.
namespace {
struct CanvasItem {
    int kind, x, y, width, height, option;
    wxColour colour;
    wxString text;
    wxBitmap bitmap;
};
wxColour canvas_colour(uint32_t rgb) {
    require(rgb<=0xffffff,"Canvas colour must be RGB bytes");
    return wxColour((rgb>>16)&255,(rgb>>8)&255,rgb&255);
}
class DrawingCanvas : public wxPanel {
public:
    std::vector<CanvasItem> items;
    wxColour background{255,255,255};
    std::string paint_error;
    bool Create(wxWindow* parent) {
        if (!wxPanel::Create(parent,wxID_ANY)) return false;
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT,[this](wxPaintEvent&) {
            try { InputOperation operation(this); wxLogNull silence; wxAutoBufferedPaintDC dc(this); render(dc,this); paint_error.clear(); }
            catch (const std::exception& error) { paint_error=error.what(); }
            catch (...) { paint_error="Unknown native canvas painting failure"; }
        });
        Bind(wxEVT_SIZE,[this](wxSizeEvent& event) { Refresh(false); event.Skip(); });
        return true;
    }
    void render(wxDC& dc,wxWindow* window=nullptr) const {
        dc.SetBackground(wxBrush(background)); dc.Clear();
        auto unit=[&](int value) { return window ? window->FromDIP(value) : value; };
        for (const auto& item : items) {
            dc.SetPen(wxPen(item.colour,unit(item.kind==2 ? item.option : 1)));
            dc.SetBrush(wxBrush(item.colour));
            if (item.kind==1) dc.DrawRectangle(unit(item.x),unit(item.y),unit(item.width),unit(item.height));
            else if (item.kind==2) dc.DrawLine(unit(item.x),unit(item.y),unit(item.width),unit(item.height));
            else if (item.kind==3) dc.DrawEllipse(unit(item.x),unit(item.y),unit(item.width),unit(item.height));
            else if (item.kind==4) {
                const wxFont font(wxFontInfo(item.option).Family(wxFONTFAMILY_SWISS));
                require(font.IsOk(),"Canvas font creation failed"); dc.SetFont(font);
                dc.SetTextForeground(item.colour); dc.DrawText(item.text,unit(item.x),unit(item.y));
            } else {
                // Pixel image dimensions are DIP dimensions on screen, pixels in export.
                if (window && (unit(item.width)!=item.width || unit(item.height)!=item.height)) {
                    const auto image=item.bitmap.ConvertToImage(); require(image.IsOk(),"Canvas bitmap conversion failed");
                    const auto resized=image.Scale(unit(item.width),unit(item.height),wxIMAGE_QUALITY_NEAREST);
                    require(resized.IsOk(),"Canvas bitmap scaling failed");
                    const wxBitmap scaled(resized); require(scaled.IsOk(),"Canvas scaled bitmap allocation failed");
                    dc.DrawBitmap(scaled,unit(item.x),unit(item.y),false);
                } else dc.DrawBitmap(item.bitmap,unit(item.x),unit(item.y),false);
            }
        }
    }
};
}
extern "C" {
wxl_handle wxl_drawing_canvas(wxl_handle parent_id) {
    return create([&] {
        auto* parent=window(parent_id);
        auto canvas=std::make_unique<ManagedWindow<DrawingCanvas>>();
        wxEventBlocker blocker(canvas.get());
        require(canvas->Create(parent),"Native canvas creation failed");
        const auto id=track(canvas.get()); canvas.release(); return id;
    });
}
int wxl_set_canvas_content(wxl_handle id,uint32_t background,const wxl_canvas_item* raw,size_t count) { return status([&] {
    auto* canvas=as<DrawingCanvas>(id);
    require(callback_depth==0,"Defer canvas content writes until callbacks return");
    require(count<=512 && (!count || raw),"Canvas supports at most512 scene items");
    const auto colour=canvas_colour(background);
    InputOperation operation(canvas); wxLogNull silence;
    std::vector<CanvasItem> staged; staged.reserve(count);
    size_t text_bytes=0, bitmap_bytes=0;
    for (size_t i=0;i<count;++i) {
        const auto& input=raw[i];
        require(input.kind>=1 && input.kind<=5,"Unsupported canvas item kind");
        require(input.x>=-10000 && input.x<=10000 && input.y>=-10000 && input.y<=10000,"Canvas coordinate outside supported bounds");
        CanvasItem item{input.kind,input.x,input.y,input.width,input.height,input.option,canvas_colour(input.rgb),wxString(),wxBitmap()};
        if (input.kind==2) {
            require(input.width>=-10000 && input.width<=10000 && input.height>=-10000 && input.height<=10000 && input.option>=1 && input.option<=32,"Invalid canvas line endpoint/width");
        } else if (input.kind==1 || input.kind==3 || input.kind==5) {
            const int limit=input.kind==5 ? 256 : 10000;
            require(input.width>=1 && input.width<=limit && input.height>=1 && input.height<=limit,"Canvas item extent outside supported bounds");
        }
        if (input.kind==4) {
            require(input.option>=6 && input.option<=72,"Canvas font point size requires6..72");
            require(input.text_length<=65536-text_bytes,"Canvas text exceeds65536 UTF-8 bytes");
            text_bytes+=input.text_length; item.text=text_value(input.text,input.text_length);
            require(item.text.find('\r')==wxString::npos && item.text.find('\n')==wxString::npos,"Canvas text must be single-line");
        } else require(input.text_length==0,"Non-text canvas item cannot carry text");
        if (input.kind==5) {
            const size_t size=static_cast<size_t>(input.width)*input.height*3;
            require(input.pixels && input.pixel_length==size && size<=2097152-bitmap_bytes,"Canvas bitmap requires exact RGB bytes within2MiB scene total");
            bitmap_bytes+=size;
            wxImage image(input.width,input.height,false); require(image.IsOk(),"Canvas image allocation failed");
            std::memcpy(image.GetData(),input.pixels,size); item.bitmap=wxBitmap(image);
            require(item.bitmap.IsOk(),"Canvas bitmap allocation failed");
        } else require(input.pixel_length==0,"Non-bitmap canvas item cannot carry pixels");
        staged.push_back(std::move(item));
    }
    canvas->items.swap(staged); canvas->background=colour; canvas->paint_error.clear(); canvas->Refresh(false);
}); }
int wxl_canvas_client_size(wxl_handle id,int* width,int* height) { return status([&] {
    auto* canvas=as<DrawingCanvas>(id); require(width && height,"Missing canvas size output");
    require(canvas->paint_error.empty(),canvas->paint_error.c_str());
    const auto size=canvas->ToDIP(canvas->GetClientSize()); *width=size.x; *height=size.y;
}); }
int wxl_save_canvas_image(wxl_handle id,const char* path,size_t length,int width,int height) { return status([&] {
    auto* canvas=as<DrawingCanvas>(id); require(callback_depth==0,"Defer canvas export until callbacks return");
    require(width>=1 && width<=2048 && height>=1 && height<=2048,"Canvas PNG dimensions require1..2048 pixels");
    const auto filename=text_value(path,length); require(!filename.empty(),"Canvas PNG path must be nonempty");
    InputOperation operation(canvas); wxLogNull silence;
    wxBitmap bitmap(width,height,24); require(bitmap.IsOk(),"Canvas export bitmap allocation failed");
    { wxMemoryDC dc(bitmap); canvas->render(dc); dc.SelectObject(wxNullBitmap); }
    wxImage image=bitmap.ConvertToImage(); require(image.IsOk(),"Canvas export image conversion failed");
    wxMemoryOutputStream encoded; wxPNGHandler png;
    require(png.SaveFile(&image,encoded,false),"Canvas PNG encoding failed");
    wxFFileOutputStream file(filename); require(file.IsOk(),"Cannot open canvas PNG path");
    wxMemoryInputStream input(encoded); file.Write(input);
    const bool written=file.IsOk();
    // Detach before checked close: pinned wxFFile retains FILE* on fclose error.
    // Its destructor must not retry closing a stream already consumed by fclose.
    FILE* output=file.GetFile()->Detach(); require(output!=nullptr,"Canvas PNG output handle is unavailable");
    const bool flushed=std::fflush(output)==0;
    const bool closed=std::fclose(output)==0;
    require(written && flushed && closed,"Canvas PNG write/flush/close failed");
}); }
}
