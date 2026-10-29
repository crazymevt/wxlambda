class CanvasOperationProbe : public DrawingCanvas {
public:
    std::function<void()> during_refresh;
    void Refresh(bool erase=true,const wxRect* rect=nullptr) override {
        if (during_refresh) during_refresh(); DrawingCanvas::Refresh(erase,rect);
    }
};
void exercise_drawing_canvas() {
    const auto frame=wxl_frame("Canvas",420,260), panel=wxl_panel(frame), canvas=wxl_drawing_canvas(panel);
    expect(canvas && !wxl_drawing_canvas(0),"Canvas construction/parent validation");
    window(canvas)->SetSize(320,180); wxl_show(frame); wxl_poll();
    int width=0,height=0;
    expect(wxl_canvas_client_size(canvas,&width,&height)==0 && width>0 && height>0,"Native DIP client size");
    unsigned char pixels[]={0,0,255,0,255,0,255,255,255,0,0,0};
    char label[]="Native λ";
    wxl_canvas_item items[4]{};
    items[0].kind=1;items[0].x=10;items[0].y=10;items[0].width=40;items[0].height=20;items[0].rgb=0xff0000;
    items[1].kind=5;items[1].x=60;items[1].y=10;items[1].width=2;items[1].height=2;items[1].pixels=pixels;items[1].pixel_length=12;
    items[2].kind=4;items[2].x=10;items[2].y=50;items[2].option=12;items[2].text=label;items[2].text_length=9;
    items[3].kind=2;items[3].width=100;items[3].height=0;items[3].option=2;
    expect(wxl_set_canvas_content(canvas,0xffffff,items,4)==0,"Compile retained scene");
    pixels[2]=0;label[0]='X';items[0].rgb=0;
    const std::string path="/private/tmp/wxl-canvas-native-λ.png";
    expect(wxl_save_canvas_image(canvas,path.data(),path.size(),160,100)==0,"Native PNG export");
    wxFFileInputStream file(wxString::FromUTF8(path)); wxPNGHandler png; wxImage image;
    expect(png.LoadFile(&image,file,false) && image.GetWidth()==160 && image.GetHeight()==100,"Export PNG decode/dimensions");
    expect(image.GetRed(20,20)==255 && image.GetGreen(20,20)==0 && image.GetBlue(60,10)==255,"Export geometry/bitmap pixels retain copied native scene");
    auto* object=as<DrawingCanvas>(canvas); const auto count=object->items.size();
    wxl_canvas_item bad{};bad.kind=4;bad.text="a\nb";bad.text_length=3;bad.option=12;
    expect(wxl_set_canvas_content(canvas,0,&bad,1)==-1 && object->items.size()==count,"Native text validation preserves scene");
    bad.text="a\rb"; expect(wxl_set_canvas_content(canvas,0,&bad,1)==-1,"Native CR validation");
    bad.text="a\0b";expect(wxl_set_canvas_content(canvas,0,&bad,1)==-1,"Native NUL validation");
    bad={};bad.kind=1;bad.width=0;bad.height=1;
    expect(wxl_set_canvas_content(canvas,0,&bad,1)==-1 && wxl_set_canvas_content(canvas,0,nullptr,513)==-1 &&
           wxl_set_canvas_content(canvas,0x1000000,nullptr,0)==-1 && wxl_canvas_client_size(canvas,nullptr,&height)==-1,"Raw scene/output bounds");
    expect(wxl_save_canvas_image(canvas,path.data(),path.size(),0,10)==-1 && wxl_save_canvas_image(canvas,path.data(),path.size(),2049,10)==-1 &&
           wxl_save_canvas_image(canvas,"/private/tmp",12,10,10)==-1,"PNG dimensions/open errors reject without native prompt");
    window(canvas)->SetSize(240,140); wxl_poll();
    expect(wxl_canvas_client_size(canvas,&width,&height)==0 && std::abs(window(canvas)->FromDIP(width)-window(canvas)->GetClientSize().x)<=1,"Resize/DIP query and native repaint");
    auto* probe=new ManagedWindow<CanvasOperationProbe>();expect(probe->Create(window(panel)),"Canvas probe creation");
    const auto target=track(probe);unsigned calls=0;
    const auto dialog=wxl_message_dialog(frame,"Canvas",6,"Probe",5,0,0);
    canvas_probe=[&](uint64_t,const wxl_event*) {
        ++calls;int result=-1;
        expect(wxl_destroy(target)==-1 && wxl_destroy(panel)==-1 && wxl_close(frame,1)==-1,"Canvas native operation lifetime guards");
        expect(wxl_set_canvas_content(target,0,nullptr,0)==-1 && wxl_save_canvas_image(target,path.data(),path.size(),10,10)==-1,"Canvas callback mutation/export rejects");
        expect(wxl_poll()==-1 && wxl_clipboard_text_available()==-1 && wxl_dialog_show(dialog,&result)==-1,"Canvas nested native entry guards");
    };
    expect(wxl_bind(frame,WXL_EVENT_CLICK,939,wxID_ANY,wxID_ANY,0,0)==0,"Canvas operation bind");
    probe->during_refresh=[&] { wxCommandEvent event(wxEVT_BUTTON,probe->GetId());event.SetEventObject(probe);probe->HandleWindowEvent(event); };
    expect(wxl_set_canvas_content(target,0xffffff,nullptr,0)==0 && calls==1 && input_operations.empty(),"Synchronous native refresh guard injection");
    probe->during_refresh={};canvas_probe={};expect(wxl_unbind(939)==1,"Canvas probe unbind");
    delete window(frame);
    expect(wxl_canvas_client_size(canvas,&width,&height)==-1 && wxl_set_canvas_content(canvas,0,nullptr,0)==-1,"Canvas owner teardown invalidates");
}
