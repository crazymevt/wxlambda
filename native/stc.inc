namespace {
constexpr int stc_document_limit=67108864;
class StyledControl : public wxStyledTextCtrl {
public:
    int undo_depth=0;
    bool selection_colours_set=false; wxColour selection_foreground,selection_background;
};
void stc_validate(wxStyledTextCtrl* control,int64_t value,const char* domain) {
    const std::string type(domain);
    if (type=="unused") return;
    if (type=="position") {
        require(value>=0 && value<=control->GetLength(),"STC position outside document");
        require(value==control->GetLength() || (control->GetCharAt(static_cast<int>(value))&0xc0)!=0x80,"STC position splits a UTF-8 character"); return;
    }
    if (type=="line") { require(value>=0 && value<control->GetLineCount(),"STC line outside document"); return; }
    if (type=="selection") { require(value>=0 && value<control->GetSelections(),"STC selection index outside current selections"); return; }
    int64_t minimum=0,maximum=10000000;
    if (type=="style") maximum=255;
    else if (type=="indicator") maximum=35;
    else if (type=="margin") maximum=4;
    else if (type=="marker") maximum=31;
    else if (type=="marker-symbol") maximum=31;
    else if (type=="boolean") maximum=1;
    else if (type=="mode") maximum=3;
    else if (type=="virtual") maximum=7;
    else if (type=="font-size") { minimum=1; maximum=256; }
    else if (type=="indicator-style") maximum=21;
    else if (type=="margin-type") maximum=6;
    else if (type=="separator") { minimum=1; maximum=127; }
    else if (type=="colour") maximum=0xffffff;
    else if (type=="mask") maximum=2147483647;
    else if (type=="signed") { minimum=-2147483648LL; maximum=2147483647; }
    else if (type=="scroll") { minimum=-1000000; maximum=1000000; }
    else if (type=="fold") maximum=0x3fff;
    require(value>=minimum && value<=maximum,"STC argument outside supported native domain");
}
void stc_ready(StyledControl*) { require(callback_depth==0,"Defer styled text mutation until callbacks return"); }
int64_t stc_pack_colour(const wxColour& value) { return (value.Red()<<16)|(value.Green()<<8)|value.Blue(); }
wxColour stc_unpack_colour(int64_t value) {
    require(value>=0 && value<=0xffffff,"STC colour requires0xRRGGBB"); return wxColour((value>>16)&255,(value>>8)&255,value&255);
}
void stc_utf8(const char* text,size_t length) {
    require((text || !length) && length<=stc_document_limit,"Styled text exceeds64MiB or missing bytes");
    const auto decoded=wxString::FromUTF8(text ? text : "",length); const auto bytes=decoded.ToUTF8();
    require(bytes.length()==length && (!length || std::memcmp(bytes.data(),text,length)==0),"Styled text must be valid UTF-8");
}
}
extern "C" {
wxl_handle wxl_stc(wxl_handle parent,const char* text,size_t length) { return create([&] {
    auto* owner=window(parent); stc_utf8(text,length);
    struct Creation { Creation(){++input_creation_depth;} ~Creation(){--input_creation_depth;} } creation;
    auto control=std::make_unique<ManagedWindow<StyledControl>>();
    { wxEventBlocker blocker(control.get()); require(control->Create(owner,wxID_ANY),"Native STC creation failed");
      control->SetCodePage(wxSTC_CP_UTF8); control->SetLexer(wxSTC_LEX_CONTAINER); control->SetWrapMode(wxSTC_WRAP_NONE);
      control->SetTargetRange(0,0); control->ReplaceTargetRaw(text ? text : "",static_cast<int>(length));
      control->EmptyUndoBuffer(); control->SetSavePoint(); }
    const auto id=track(control.get()); input_owners.emplace(control.get(),false); control.release(); return id;
}); }
int wxl_stc_get(wxl_handle id,int field,int index,int64_t* result) { return status([&] {
    auto* control=as<StyledControl>(id); require(result,"Missing STC result output");
    switch(field) {
#include "stc-get-generated.inc"
        case 48: *result=control->GetTabWidth(); break;
        default:throw std::runtime_error("Unsupported STC property");
    }
}); }
int wxl_stc_set(wxl_handle id,int field,int index,int64_t value) { return status([&] {
    auto* control=as<StyledControl>(id); stc_ready(control); InputOperation operation(control);
    if (field==25 || field==26 || field==32) stc_unpack_colour(value);
    switch(field) {
#include "stc-set-generated.inc"
        case 48: require(value>=1 && value<=16,"Tab width must be1..16"); control->SetTabWidth(static_cast<int>(value)); break;
        default:throw std::runtime_error("Unsupported writable STC property");
    }
}); }
int wxl_stc_action(wxl_handle id,int action,int a,int b,int64_t* result) { return status([&] {
    auto* control=as<StyledControl>(id); require(result,"Missing STC result output"); *result=0;
    const bool query=(action==19 || (action>=24 && action<=30));
    std::unique_ptr<InputOperation> operation;
    const bool styling=(action==10 || action==11) && stc_synchronous_styling(control);
    if (!query && !styling) { stc_ready(control); operation=std::make_unique<InputOperation>(control); }
    switch(action) {
#include "stc-action-generated.inc"
        default:throw std::runtime_error("Unsupported STC operation");
    }
}); }
const char* wxl_stc_text(wxl_handle id,int start,int end,size_t* length) {
    try {
        auto* control=as<StyledControl>(id); require(length,"Missing STC text output");
        stc_validate(control,start,"position"); stc_validate(control,end,"position"); require(start<=end,"Reversed STC range");
        const auto bytes=control->GetTextRangeRaw(start,end);
        metadata_result.assign(end>start ? bytes.data() : "",static_cast<size_t>(end-start));
        *length=metadata_result.size(); return metadata_result.c_str();
    } catch (const std::exception& failure) { error=failure.what(); return nullptr; }
}
int wxl_stc_replace(wxl_handle id,int start,int end,const char* text,size_t length) { return status([&] {
    auto* control=as<StyledControl>(id); stc_ready(control); stc_utf8(text,length);
    stc_validate(control,start,"position"); stc_validate(control,end,"position"); require(start<=end,"Reversed STC range");
    const int64_t final_length=int64_t(control->GetLength())-(end-start)+int64_t(length);
    require(final_length>=0 && final_length<=stc_document_limit,"Styled text exceeds64MiB");
    require(!control->GetReadOnly(),"Read-only styled text"); InputOperation operation(control);
    control->SetTargetRange(start,end); control->ReplaceTargetRaw(text ? text : "",static_cast<int>(length));
}); }
const char* wxl_stc_font(wxl_handle id,int style,size_t* length) { return metadata_string([&] {
    auto* control=as<StyledControl>(id); stc_validate(control,style,"style"); return control->StyleGetFaceName(style);
},length); }
int wxl_stc_string(wxl_handle id,int operation,int argument,const char* text,size_t length) { return status([&] {
    auto* control=as<StyledControl>(id); stc_ready(control); require(length<=1048576,"STC label/list exceeds1MiB");
    const auto value=text_value(text,length); InputOperation guard(control);
    if (operation==0) { stc_validate(control,argument,"style"); control->StyleSetFaceName(argument,value); }
    else if (operation==1) { require(argument>=0 && argument<=control->GetCurrentPos(),"Autocomplete prefix outside caret range"); stc_validate(control,control->GetCurrentPos()-argument,"position"); control->AutoCompShow(argument,value); }
    else if (operation==2) { stc_validate(control,argument,"position"); control->CallTipShow(argument,value); }
    else throw std::runtime_error("Unsupported STC text operation");
}); }
int wxl_stc_point(wxl_handle id,int position,int* x,int* y) { return status([&] {
    auto* control=as<StyledControl>(id); require(x && y,"Missing STC point output"); stc_validate(control,position,"position");
    const auto point=control->PointFromPosition(position); *x=point.x; *y=point.y;
}); }
int wxl_stc_position(wxl_handle id,int x,int y,int* position) { return status([&] {
    auto* control=as<StyledControl>(id); require(position,"Missing STC position output"); require(x>=-1000000 && x<=1000000 && y>=-1000000 && y<=1000000,"STC point outside bounds");
    *position=control->PositionFromPointClose(x,y);
}); }
int wxl_stc_detail(uint64_t token,int field,int64_t* result) { return status([&] {
    require(result && field>=0 && field<13,"Invalid styled event detail output/field");
    auto& event=current_event(token); require(stc_detail_valid(event.GetEventType(),field),"Styled text detail is unavailable for this event kind");
    wxl_event snapshot{}; fill_stc_detail(event,snapshot); *result=snapshot.detail[field];
}); }
}

extern "C" int wxl_stc_style_bytes(wxl_handle id,const unsigned char* bytes,size_t length) { return status([&] {
    auto* control=as<StyledControl>(id);
    require((bytes || !length) && length<=stc_document_limit,"Invalid styling byte buffer");
    std::unique_ptr<InputOperation> operation;
    if (!stc_synchronous_styling(control)) { stc_ready(control); operation=std::make_unique<InputOperation>(control); }
    require(control->GetEndStyled()>=0 && control->GetEndStyled()<=control->GetLength(),"Start styling again after document changes");
    require(length<=static_cast<size_t>(control->GetLength()-control->GetEndStyled()),"Style bytes exceed document");
    if (length) { control->SetStyleBytes(static_cast<int>(length),reinterpret_cast<char*>(const_cast<unsigned char*>(bytes))); }
}); }

extern "C" {
int wxl_stc_caret_colour(wxl_handle id,int write,int64_t* colour) { return status([&] {
    auto* control=as<StyledControl>(id); require(colour && (write==0 || write==1),"Invalid caret colour arguments");
    if (write) { stc_ready(control); const wxColour value=stc_unpack_colour(*colour); InputOperation operation(control); control->SetCaretForeground(value); }
    *colour=stc_pack_colour(control->GetCaretForeground());
}); }
int wxl_stc_selection_colours(wxl_handle id,int write,int reset,int64_t* foreground,int64_t* background,int* present) { return status([&] {
    auto* control=as<StyledControl>(id); require(foreground && background && present && (write==0 || write==1) && (reset==0 || reset==1),"Invalid selection colour arguments");
    if (write) {
        stc_ready(control); if (!reset) { stc_unpack_colour(*foreground); stc_unpack_colour(*background); }
        InputOperation operation(control);
        const wxColour fg=reset ? *wxBLACK : stc_unpack_colour(*foreground),bg=reset ? *wxWHITE : stc_unpack_colour(*background);
        control->SetSelForeground(!reset,fg); control->SetSelBackground(!reset,bg);
        control->selection_colours_set=!reset; control->selection_foreground=fg; control->selection_background=bg;
    }
    *present=control->selection_colours_set ? 1 : 0;
    if (control->selection_colours_set) { *foreground=stc_pack_colour(control->selection_foreground); *background=stc_pack_colour(control->selection_background); }
}); }
}
