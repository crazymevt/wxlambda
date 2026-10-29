// Include after status/text helpers, outside extern-C; no fake provider event.
namespace {
constexpr int list_virtual_limit=10000000,list_stored_limit=100000;
struct ListTextRequest { uint64_t ticket; wxString text; bool replied=false; unsigned depth=0; };
std::vector<ListTextRequest*> list_text_requests;
uint64_t next_list_text_request=0;
bool virtual_list_container(wxWindow* object) {
 bool registered=false;for(const auto& entry:windows)if(entry.second.get()==object){registered=true;break;}
 if(!registered)return false;
 const auto& type=typeid(*object);
 return type==typeid(ManagedWindow<wxFrame>)||type==typeid(ManagedWindow<wxPanel>)||
        type==typeid(ManagedWindow<wxScrolledWindow>)||type==typeid(ManagedWindow<wxNotebook>)||
        type==typeid(ManagedWindow<DynamicAuiNotebook>)||type==typeid(ManagedWindow<FixedSplitter>)||
        type==typeid(ManagedWindow<DrawingCanvas>)||type==typeid(ManagedWindow<WebContainer>);
}
class ReportList : public wxListCtrl {
public:
 uint64_t provider_token=0; wxl_report_list_text_callback provider=nullptr;
 mutable bool provider_active=false,provider_failed=false;
 bool retiring=false;
 virtual bool DestroyNative() {return wxListCtrl::Destroy();}
 bool Destroy() override {
  wxWeakRef<wxWindow> weak(this);const bool previous=retiring;retiring=true;
  const auto found=input_owners.find(this);const bool tracked=found!=input_owners.end(),old=tracked&&found->second;
  if(tracked)found->second=true;
  auto restore=[&]{if(auto* live=weak.get()){static_cast<ReportList*>(live)->retiring=previous;auto entry=input_owners.find(live);if(tracked&&entry!=input_owners.end())entry->second=old;}};
  try {const bool accepted=DestroyNative();if(!accepted)restore();return accepted;}catch(...){restore();throw;}
 }
 ~ReportList() override {retiring=true;provider=nullptr;provider_token=0;}
 void CheckProvider() const {if(provider_failed){provider_failed=false;throw std::runtime_error("Native virtual list provider failed");}}
 wxString OnGetItemText(long row,long column) const override {
  if(!wxIsMainThread()||retiring||provider_failed||finishing||!dispatch||pending_callback_error||input_creation_depth||!provider||!provider_token)return wxString();
  if(provider_active){provider_failed=true;return wxString();}
  try {
   auto* self=const_cast<ReportList*>(this);
   if(window_destruction_pending(self)||row<0||row>=GetItemCount()||column<0||column>=GetColumnCount())return wxString();
   require(next_list_text_request<std::numeric_limits<uint64_t>::max(),"Virtual list request IDs exhausted");
   require(list_text_requests.size()<64,"Virtual list provider nesting exceeds64");
   ListTextRequest request{++next_list_text_request,wxString(),false};
   struct Scope {
    const ReportList* list;size_t previous;CallbackScope callback;
    Scope(const ReportList* value,ListTextRequest* request):list(value),previous(input_operations.size()){
     bool active=false;for(const auto& entry:input_operations)if(entry.get()==value)active=true;
     if(!active)input_operations.emplace_back(const_cast<ReportList*>(value));
     try {
      views_provider_owners.emplace_back(const_cast<ReportList*>(value));
      try {list_text_requests.push_back(request);}catch(...){views_provider_owners.pop_back();throw;}
     }catch(...){input_operations.resize(previous);throw;}
     request->depth=callback_depth;
     ++native_entry_depth;list->provider_active=true;
    }
    ~Scope(){list_text_requests.pop_back();views_provider_owners.pop_back();list->provider_active=false;--native_entry_depth;input_operations.resize(previous);}
   } scope(this,&request);
   provider(provider_token,request.ticket,int(row),int(column));
   return request.replied&&!pending_callback_error?request.text:wxString();
  }catch(...){provider_failed=true;return wxString();}
 }
};
ReportList* report_list(wxl_handle id) {auto* list=as<ReportList>(id);list->CheckProvider();return list;}
void list_mutation(ReportList* list,bool structure=false) {
 require(callback_depth==0,"Defer list mutations until callbacks/providers return");
 require(!list->retiring,"List destruction is pending");if(structure)require_dispatch_entry(true);
}
void list_column(ReportList* list,int column,bool insert=false) {require(column>=0&&column<list->GetColumnCount()+(insert?1:0),"List column out of bounds");}
void list_row(ReportList* list,int row) {require(row>=0&&row<list->GetItemCount(),"List item out of bounds");}
wxString list_string(const char* value,size_t length,size_t limit=65536) {require(length<=limit,"List text exceeds supported byte limit");const auto text=text_value(value,length);const auto bytes=text.ToUTF8();require(bytes.length()==length&&(!length||!std::memcmp(bytes.data(),value,length)),"List text must be valid UTF8");return text;}
}
extern "C" {
wxl_handle wxl_report_list_control(wxl_handle parent,int virtual_list,uint64_t token,wxl_report_list_text_callback callback) {return create([&]{
 auto* owner=window(parent);require(virtual_list==0||virtual_list==1,"Invalid virtual list flag");
 if(virtual_list)for(auto* ancestor=owner;ancestor;ancestor=ancestor->GetParent())
  require(virtual_list_container(ancestor),"Virtual lists require registered supported frame/panel/book/splitter container ancestors");
 require(virtual_list?(token&&callback):(!token&&!callback),"List provider requires a virtual list");
 struct Creation{Creation(){++input_creation_depth;}~Creation(){--input_creation_depth;}} creation;
 auto list=std::make_unique<ManagedWindow<ReportList>>();list->provider_token=token;list->provider=callback;
 require(list->Create(owner,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxLC_REPORT|wxLC_SINGLE_SEL|(virtual_list?wxLC_VIRTUAL:0)),"Native report list creation failed");
 const auto id=track(list.get());input_owners.emplace(list.get(),false);list.release();return id;
});}
int wxl_report_list_provider_active(uint64_t token) {return boolean_result([&]{ready();for(const auto& entry:windows)if(auto* list=dynamic_cast<ReportList*>(entry.second.get()))if(token&&list->provider_token==token&&!list->retiring&&!window_destruction_pending(list))return true;return false;});}
int wxl_report_list_text_reply(uint64_t ticket,const char* text,size_t length) {return status([&]{ready();require(!list_text_requests.empty()&&list_text_requests.back()->ticket==ticket&&list_text_requests.back()->depth==callback_depth,"Virtual list reply requires current innermost request");auto* request=list_text_requests.back();require(!request->replied,"Virtual list request already replied");request->text=list_string(text,length);request->replied=true;});}
int wxl_report_list_property(wxl_handle id,int field) {int result=-2;status([&]{auto* list=report_list(id);require(field>=0&&field<=3,"Invalid list property");result=field==0?int(list->IsVirtual()):field==1?list->GetColumnCount():field==2?list->GetItemCount():int(list->GetNextItem(-1,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED));});return result;}
int wxl_report_list_validate_cell(wxl_handle id,int row,int column){return status([&]{auto* list=report_list(id);list_row(list,row);list_column(list,column);});}
const char* wxl_report_list_text(wxl_handle id,int row,int column,size_t* length){return metadata_string([&]{auto* list=report_list(id);list_row(list,row);list_column(list,column);require(!list->provider_active,"Recursive native list text query");const auto value=list->GetItemText(row,column);list->CheckProvider();return value;},length);}
int wxl_set_report_list_text(wxl_handle id,int row,int column,const char* text,size_t length){return status([&]{auto* list=report_list(id);list_mutation(list);require(!list->IsVirtual(),"Virtual list text belongs to its provider");list_row(list,row);list_column(list,column);const auto value=list_string(text,length);InputOperation operation(list);wxEventBlocker blocker(list);require(list->SetItem(row,column,value),"Unable to set list text");});}
int wxl_report_list_insert_column(wxl_handle id,int index,const char* text,size_t length,int width){return status([&]{auto* list=report_list(id);list_mutation(list,true);list_column(list,index,true);require(list->GetItemCount()==0,"Clear list items before inserting columns");require(list->GetColumnCount()<64&&width>=1&&width<=10000,"List column count/width exceeds supported bounds");const auto label=list_string(text,length,4096);InputOperation operation(list);require(list->InsertColumn(index,label,wxLIST_FORMAT_LEFT,width)==index,"Unable to insert list column");});}
int wxl_report_list_delete_column(wxl_handle id,int index){return status([&]{auto* list=report_list(id);list_mutation(list,true);list_column(list,index);require(list->GetItemCount()==0,"Clear list items before removing columns");InputOperation operation(list);require(list->DeleteColumn(index),"Unable to delete list column");});}
const char* wxl_report_list_column_text(wxl_handle id,int index,size_t* length){return metadata_string([&]{auto* list=report_list(id);list_column(list,index);wxListItem column;column.SetMask(wxLIST_MASK_TEXT);require(list->GetColumn(index,column),"Unable to read list column");return column.GetText();},length);}
int wxl_set_report_list_column_text(wxl_handle id,int index,const char* text,size_t length){return status([&]{auto* list=report_list(id);list_mutation(list);list_column(list,index);const auto value=list_string(text,length,4096);InputOperation operation(list);wxListItem column;column.SetMask(wxLIST_MASK_TEXT);column.SetText(value);require(list->SetColumn(index,column),"Unable to label list column");});}
int wxl_report_list_column_width(wxl_handle id,int index){int result=-1;status([&]{auto* list=report_list(id);list_column(list,index);result=list->GetColumnWidth(index);});return result;}
int wxl_set_report_list_column_width(wxl_handle id,int index,int width){return status([&]{auto* list=report_list(id);list_mutation(list);list_column(list,index);require(width>=1&&width<=10000,"List width requires1..10000 pixels");InputOperation operation(list);require(list->SetColumnWidth(index,width),"Unable to size list column");});}
int wxl_report_list_append(wxl_handle id,const char*const* texts,const size_t* lengths,size_t count){int result=-1;const int code=status([&]{auto* list=report_list(id);list_mutation(list,true);require(!list->IsVirtual(),"Virtual list count belongs to SetItemCount");require(count&&count==size_t(list->GetColumnCount())&&texts&&lengths,"List row must match existing columns");require(list->GetItemCount()<list_stored_limit,"Stored list exceeds100000 rows");size_t total=0;std::vector<wxString> values;for(size_t i=0;i<count;++i){total+=lengths[i];require(total<=1048576,"List row exceeds1MiB");values.push_back(list_string(texts[i],lengths[i]));}InputOperation operation(list);wxEventBlocker blocker(list);const long row=list->InsertItem(list->GetItemCount(),values[0]);require(row>=0,"Unable to append list item");try{for(size_t i=1;i<count;++i)require(list->SetItem(row,int(i),values[i]),"Unable to fill list row");}catch(...){list->DeleteItem(row);throw;}result=int(row);});return code==0?result:-1;}
int wxl_report_list_delete(wxl_handle id,int row){return status([&]{auto* list=report_list(id);list_mutation(list,true);require(!list->IsVirtual(),"Set virtual item count instead of deleting rows");list_row(list,row);InputOperation operation(list);wxEventBlocker blocker(list);require(list->DeleteItem(row),"Unable to delete list item");});}
int wxl_report_list_clear(wxl_handle id){return status([&]{auto* list=report_list(id);list_mutation(list,true);InputOperation operation(list);wxEventBlocker blocker(list);if(list->IsVirtual())list->SetItemCount(0);else require(list->DeleteAllItems(),"Unable to clear list");});}
int wxl_set_report_list_count(wxl_handle id,int count){return status([&]{auto* list=report_list(id);list_mutation(list,true);require(list->IsVirtual(),"Only virtual lists support SetItemCount");require(count>=0&&count<=list_virtual_limit&&(count==0||list->GetColumnCount()>0),"Virtual list requires columns/count0..10000000");InputOperation operation(list);wxEventBlocker blocker(list);list->SetItemCount(count);list->Refresh();list->CheckProvider();});}
int wxl_set_report_list_selection(wxl_handle id,int row,int notify){return status([&]{auto* list=report_list(id);list_mutation(list);require(notify==0||notify==1,"Invalid list notify flag");require(row>=-1&&row<list->GetItemCount(),"List selection out of bounds");InputOperation operation(list);std::unique_ptr<wxEventBlocker> blocker;if(!notify)blocker=std::make_unique<wxEventBlocker>(list);const long previous=list->GetNextItem(-1,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED);if(previous>=0&&previous!=row)require(list->SetItemState(previous,0,wxLIST_STATE_SELECTED),"Unable to clear list selection");if(row>=0)require(list->SetItemState(row,wxLIST_STATE_SELECTED,wxLIST_STATE_SELECTED),"Unable to select list item");});}
int wxl_report_list_refresh(wxl_handle id,int first,int last){return status([&]{auto* list=report_list(id);list_mutation(list);list_row(list,first);list_row(list,last);require(first<=last,"Reversed list refresh range");InputOperation operation(list);list->RefreshItems(first,last);});}
int wxl_views_detail(uint64_t token,int field,int64_t* result){return status([&]{require(result&&field>=0&&field<=5,"Invalid views event detail");auto& event=current_event(token);const int kind=views_kind_for_type(event.GetEventType());require(kind&&((field==0&&kind>=140&&kind<=143)||(field==1&&kind==145)||(field>=2&&field<=3&&kind==144)||(field>=4&&(kind==143||kind==151))),"Detail unavailable for this views event");wxl_event snapshot{};fill_views_detail(event,snapshot);*result=snapshot.detail[field];});}
int wxl_tree_decoration(wxl_handle id,int field,int64_t* result){return status([&]{require(result&&field>=0&&field<=3,"Invalid tree decoration");const auto& item=tree_node(id);auto* tree=as<wxTreeCtrl>(item.owner);if(field==0)*result=tree->IsBold(item.native);else if(field==3)*result=tree->ItemHasChildren(item.native);else{const auto value=field==1?tree->GetItemTextColour(item.native):tree->GetItemBackgroundColour(item.native);*result=value.IsOk()?int64_t((value.Red()<<16)|(value.Green()<<8)|value.Blue()):-1;}});}
int wxl_set_tree_decoration(wxl_handle id,int field,int64_t value){return status([&]{require(field>=0&&field<=3,"Invalid tree decoration");const auto& item=tree_node(id);auto* tree=as<wxTreeCtrl>(item.owner);TreeOperation operation(tree);if(field==0||field==3){require(value==0||value==1,"Invalid tree boolean");if(field==0)tree->SetItemBold(item.native,value!=0);else tree->SetItemHasChildren(item.native,value!=0);}else{require(value>=-1&&value<=0xffffff,"Tree colour requiresNIL or0xRRGGBB");const auto colour=value<0?wxNullColour:wxColour((value>>16)&255,(value>>8)&255,value&255);if(field==1)tree->SetItemTextColour(item.native,colour);else tree->SetItemBackgroundColour(item.native,colour);}});}
}
