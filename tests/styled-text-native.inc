std::function<void(uint64_t,const wxl_event*)> stc_native_probe;
void stc_native_dispatch(uint64_t token,const wxl_event* event) { if(stc_native_probe) stc_native_probe(token,event); }
void exercise_styled_text() {
    struct Restore { wxl_callback old=dispatch; ~Restore(){ dispatch=old; stc_native_probe={}; } } restore;
    dispatch=stc_native_dispatch;
    const auto frame=wxl_frame("STC native",600,400), id=wxl_stc(frame,"λ 😀",7), other=wxl_stc(frame,"other",5);
    expect(id && other,"STC native construction"); int64_t result=0; size_t length=0;
    auto* control=as<StyledControl>(id);
    expect(control->GetCodePage()==wxSTC_CP_UTF8 && control->GetLexer()==wxSTC_LEX_CONTAINER,"Native UTF8/container styling policy");
    expect(wxl_stc_replace(id,1,2,"x",1)==-1 && control->GetLength()==7,"Reject UTF8 boundary split before native edit");
    const char nul[]={'A',0,char(0xce),char(0xbb)};
    expect(wxl_stc_replace(id,0,7,nul,4)==0,"Explicit length embedded NUL edit");
    const auto* copy=wxl_stc_text(id,0,4,&length);
    expect(copy && length==4 && std::memcmp(copy,nul,4)==0,"Native raw text copy preserves NUL and UTF8");
    unsigned modified=0,styled=0; uint64_t outer_context=0;
    stc_native_probe=[&](uint64_t token,const wxl_event* event) {
        if(token==976) {
            ++modified;
            expect(event->kind==31 && event->origin_id==id,"STC actual modification origin");
            expect(wxl_destroy(id)==-1 && wxl_close(frame,1)==-1,"Modification callback protects source/ancestor");
            expect(wxl_stc_replace(id,0,0,"nested",6)==-1 && wxl_stc_action(id,10,0,0,&result)==-1,"No edits/styling in modification callbacks");
            if(outer_context) expect(wxl_stc_detail(outer_context,0,&result)==-1,"Nested callback suspends outer context");
        } else if(token==977) {
            ++styled; outer_context=event->context;
            expect(wxl_stc_detail(outer_context,0,&result)==0,"Actual style-needed context");
            expect(wxl_stc_action(other,10,0,0,&result)==-1,"Styling callback exception cannot target another control");
            expect(wxl_stc_action(id,10,0,0,&result)==0 && wxl_stc_action(id,11,4,2,&result)==0,"Synchronous native style-needed styling");
            expect(wxl_stc_replace(id,0,0,"nested",6)==-1,"Style-needed does not permit document edits");
            outer_context=0;
        }
        expect(wxl_context_set_skipped(event->context,1)==0,"STC context Skip");
    };
    expect(wxl_bind(frame,31,976,wxID_ANY,wxID_ANY,1,0)==0 && wxl_bind(id,33,977,wxID_ANY,wxID_ANY,1,0)==0,"STC native observer/context bindings");
    wxStyledTextEvent style(wxEVT_STC_STYLENEEDED,control->GetId()); style.SetEventObject(control); style.SetPosition(4);
    control->HandleWindowEvent(style);
    expect(styled==1 && control->GetStyleAt(0)==2,"Scintilla native styling readback from style-needed fixture");
    expect(wxl_stc_replace(id,0,1,"Z",1)==0 && modified>0,"Actual Scintilla modified notifications");
    expect(wxl_unbind(976)==1 && wxl_unbind(977)==1,"STC fixture unbind");
    // Editing after the current styling frontier must not reset a bridge-only
    // cursor to0 and permit SetStyleBytes to overrun native remaining text.
    const auto range_probe=wxl_stc(frame,"abcdefgh",8);
    auto* range_control=as<StyledControl>(range_probe);
    expect(wxl_stc_action(range_probe,10,6,0,&result)==0 &&
           wxl_stc_replace(range_probe,7,8,"X",1)==0 && range_control->GetEndStyled()==6,
           "Native styling frontier survives an edit after it");
    const unsigned char too_many[]={1,1,1,1,1,1,1,1};
    expect(wxl_stc_style_bytes(range_probe,too_many,sizeof(too_many))==-1 &&
           wxl_stc_action(range_probe,11,8,1,&result)==-1 && range_control->GetEndStyled()==6,
           "Both styling writers validate actual native frontier before writes");
    unsigned char remaining[]={5,6};
    expect(wxl_stc_style_bytes(range_probe,remaining,sizeof(remaining))==0 &&
           range_control->GetEndStyled()==8 && range_control->GetStyleAt(6)==5 && range_control->GetStyleAt(7)==6,
           "Valid style bytes fill only native remaining range");
    remaining[0]=9;expect(range_control->GetStyleAt(6)==5,"Native style data does not retain caller buffer");
    control->SetCurrentPos(4); // A\0 followed by the two-byte lambda: prefix1 splits it.
    const auto before=control->GetTextLength();
    expect(wxl_stc_string(id,1,1,"abc",3)==-1,"Autocomplete rejects prefix splitting UTF8 when caret invalid start");
    expect(control->GetTextLength()==before,"Rejected autocomplete preserves text");
    int foreign=0; std::thread worker([&]{ foreign=wxl_stc_get(id,0,0,&result); });worker.join();
    expect(foreign==-1,"STC worker reads reject before native access");
    delete window(frame); expect(wxl_stc_get(id,0,0,&result)==-1,"STC actual owner teardown invalidation");
}
