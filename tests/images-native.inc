void exercise_images() {
 unsigned char rgb[]={255,0,0,0,255,0,0,0,255,1,2,3},alpha[]={0,64,128,255};
 auto image=wxl_image(2,2,rgb,12,alpha,4); expect(image,"Create"); rgb[0]=0; alpha[0]=255;
 unsigned char pixel[4]; expect(wxl_image_pixel(image,0,0,pixel,0)==0&&pixel[0]==255&&pixel[3]==0,"Copied input");
 auto bitmap=wxl_bitmap(image),copy=wxl_bitmap_image(bitmap); expect(bitmap&&copy,"Bitmap conversion");
 unsigned char changed[]={4,5,6,200}; expect(wxl_image_pixel(image,0,0,changed,1)==0,"Pixel write");
 expect(wxl_image_pixel(copy,0,0,pixel,0)==0&&pixel[3]==0,"Bitmap alpha/copy independence");
 expect(wxl_image_pixel(copy,1,1,pixel,0)==0&&pixel[0]==1&&pixel[1]==2&&pixel[2]==3&&pixel[3]==255,"Bitmap opaque colour");
 auto scaled=wxl_scale_image(image,8,8,0); expect(scaled,"Scale");
 const char* path="/private/tmp/wxlambda-images-stage.png"; expect(wxl_save_image(image,path,std::strlen(path))==0,"Save PNG");
 auto loaded=wxl_load_image(path,std::strlen(path)); expect(loaded&&wxl_image_pixel(loaded,0,0,pixel,0)==0&&pixel[0]==4&&pixel[3]==200,"PNG round trip");
 auto frame=wxl_frame("Images",400,300),control=wxl_static_bitmap(frame,bitmap); expect(frame&&control,"Display");
#ifdef wxHAS_BITMAPTOGGLEBUTTON
 auto flat=wxl_bitmap_toggle(frame,bitmap,1,1); expect(flat,"Flat bitmap toggle creation");
 expect((as<wxBitmapToggleButton>(flat)->GetWindowStyleFlag()&wxBORDER_MASK)==wxBORDER_NONE,"Actual native borderless style");
 expect(wxl_toggle_button_checked(flat)==1,"Flat selected state");
 expect(wxl_bitmap_toggle(frame,bitmap,0,2)==0,"Invalid native border rejects");
 unsigned char fg[3],bg[3]; expect(wxl_window_colours(frame,fg,bg)==0,"Window colours query");
#endif

 int rectangle[4],ppi[2],point[2]; double scale=0;
 expect(wxl_display_count()>0&&wxl_display_info(0,0,rectangle,ppi,&scale)==0&&rectangle[2]>0&&scale>0,"Display geometry/scale");
 expect(wxl_window_point(control,1,3,4,point)==0&&wxl_window_point(control,2,point[0],point[1],point)==0&&point[0]==3&&point[1]==4,"Client/screen round trip");
 expect(wxl_window_content_scale(control,&scale)==0&&scale>0&&wxl_display_info(-1,0,rectangle,ppi,&scale)==-1,"Coordinate bounds");
 auto canvas=wxl_drawing_canvas(frame); unsigned char rgba_bytes[]={255,0,0,0}; wxl_canvas_item scene{};
 scene.kind=6;scene.width=1;scene.height=1;scene.pixels=rgba_bytes;scene.pixel_length=4;
 expect(wxl_set_canvas_content(canvas,0xffffff,&scene,1)==0,"RGBA canvas construction");
 for(int extent:{0,-1,257}) { scene.width=extent;expect(wxl_set_canvas_content(canvas,0xffffff,&scene,1)==-1,"RGBA native dimension guard"); } scene.width=1;
 const char* canvas_path="/private/tmp/wxlambda-rgba-stage.png";
 expect(wxl_save_canvas_image(canvas,canvas_path,std::strlen(canvas_path),1,1)==0,"Transparent canvas PNG");
 auto rendered=wxl_load_image(canvas_path,std::strlen(canvas_path));expect(rendered&&wxl_image_pixel(rendered,0,0,pixel,0)==0&&pixel[0]==255&&pixel[1]==255&&pixel[2]==255,"RGBA transparency composition");
 rgba_bytes[3]=255;expect(wxl_set_canvas_content(canvas,0xffffff,&scene,1)==0,"Opaque RGBA scene");rgba_bytes[0]=0;
 expect(wxl_save_canvas_image(canvas,canvas_path,std::strlen(canvas_path),1,1)==0,"Copied canvas pixels PNG");
 auto opaque=wxl_load_image(canvas_path,std::strlen(canvas_path));expect(opaque&&wxl_image_pixel(opaque,0,0,pixel,0)==0&&pixel[0]==255&&pixel[1]==0&&pixel[2]==0,"RGBA scene copies source");
 auto displayed=wxl_static_bitmap_get(control); expect(displayed&&wxl_image_destroy(bitmap,1)==0,"Display copy");
 expect(wxl_static_bitmap_set(control,displayed)==0,"Display replacement");
 { CallbackScope callback; expect(wxl_image_pixel(image,0,0,changed,1)==-1&&wxl_static_bitmap_set(control,displayed)==-1,"Callback mutation reject"); }
 std::thread worker([&] { expect(wxl_image_has_alpha(image)==-1,"Worker reject"); }); worker.join();
 expect(wxl_image_set_bytes(image,1,nullptr,0)==0&&wxl_image_has_alpha(image)==0,"Clear alpha");
 expect(wxl_image_copy_bytes(image,1,alpha,4)==0&&alpha[0]==255,"Opaque alpha read");
 expect(wxl_image(0,2,rgb,12,nullptr,0)==0&&wxl_image_pixel(image,2,0,pixel,0)==-1,"Bounds");
 expect(wxl_image_destroy(image,0)==0&&wxl_image_alive(image,0)==0,"Dispose");
 delete window(frame); expect(!image_resources.empty()&&!bitmap_resources.empty(),"Image values intentionally survive until session shutdown");

}
