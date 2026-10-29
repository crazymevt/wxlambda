namespace views_native_fixture {
std::function<void()> extra_checks;
std::function<void(uint64_t,const wxl_event*)> event_checks;
void event_dispatch(uint64_t token,const wxl_event* event){if(event_checks)event_checks(token,event);}
wxl_handle target=0,parent=0;uint64_t expired_request=0;unsigned calls=0;bool fail_provider=false;
void provide(uint64_t token,uint64_t request,int row,int column){
 if(extra_checks)extra_checks();
 ++calls;expect(token==123,"Provider token");if(fail_provider)throw std::runtime_error("Injected provider failure");
 expect(wxl_report_list_validate_cell(target,row,column)==0,"Provider read safety");
 expect(wxl_set_report_list_count(target,1)==-1&&wxl_destroy(target)==-1&&wxl_poll()==-1&&wxl_finish()==-1,"Provider write/destroy/pump guards");
 expect(protected_views_provider(window(target))&&protected_views_provider(window(parent)),"Provider readonly owner ancestry");
 expect(wxl_layout(parent)==-1&&wxl_show(parent)==-1&&wxl_enable(parent,0)==-1&&
        wxl_set_name(parent,"bad",3)==-1&&wxl_set_title(parent,"bad",3)==-1&&
        wxl_spacer(parent,1)==-1&&wxl_stretch(parent,1)==-1,"Inherited ancestor mutation guards");
 expect(wxl_layout(target)==-1&&wxl_show_state(target,0)==-1&&wxl_enable(target,0)==-1&&wxl_set_name(target,"bad",3)==-1,"Inherited source mutation guards");
 {CallbackScope nested;expect(wxl_report_list_text_reply(request,"wrong",5)==-1,"Reply suspended by nested unrelated callback");}
 const std::string text="Row λ "+std::to_string(row)+":"+std::to_string(column);
 expect(wxl_report_list_text_reply(request,text.data(),text.size())==0&&wxl_report_list_text_reply(request,"twice",5)==-1,"Copied one-shot provider reply");expired_request=request;
}
}
void exercise_report_views() {
 using namespace views_native_fixture;
 struct Restore{wxl_callback previous=dispatch;~Restore(){dispatch=previous;event_checks={};extra_checks={};}}restore;
 dispatch=event_dispatch;
 const auto frame=wxl_frame("Views stage",600,400);parent=frame;
 const auto invalid=wxl_button(frame,"invalid");
 expect(!wxl_report_list_control(invalid,1,123,provide),"Virtual list rejects arbitrary control ancestor");
 target=wxl_report_list_control(frame,1,123,provide);expect(target!=0,"Virtual list construction");
 expect(wxl_report_list_insert_column(target,0,"Name",4,200)==0&&wxl_report_list_insert_column(target,1,"Value",5,100)==0,"Columns");
 expect(wxl_set_report_list_count(target,1000000)==0,"Scalable virtual count");size_t length=0;
 const char* text=wxl_report_list_text(target,999999,1,&length);expect(text&&std::string(text,length)=="Row λ 999999:1"&&calls,"Native virtual provider");
 expect(wxl_report_list_text_reply(expired_request,"late",4)==-1&&list_text_requests.empty()&&input_operations.empty()&&callback_depth==0,"Expired reply and scope cleanup");
 fail_provider=true;expect(!wxl_report_list_text(target,0,0,&length)&&list_text_requests.empty()&&input_operations.empty()&&callback_depth==0,"Provider exception containment");fail_provider=false;
 expect(wxl_report_list_property(target,2)==1000000,"Provider failure recovery");
 const auto stored=wxl_report_list_control(frame,0,0,nullptr);expect(stored&&wxl_report_list_insert_column(stored,0,"A",1,100)==0&&wxl_report_list_insert_column(stored,1,"B",1,100)==0,"Stored report");
 const char* values[]={"A λ","x"};const size_t sizes[]={4,1};
 expect(wxl_report_list_append(stored,values,sizes,2)==0&&wxl_set_report_list_text(stored,0,1,"β",2)==0,"Stored row text");
 text=wxl_report_list_text(stored,0,1,&length);expect(text&&std::string(text,length)=="β","Stored copied readback");
 expect(wxl_set_report_list_selection(stored,0,0)==0&&wxl_report_list_property(stored,3)==0&&wxl_report_list_delete(stored,0)==0&&wxl_report_list_property(stored,3)==-1,"Stored single selection/delete");
 const auto tree=wxl_tree(frame);wxl_handle item=0;expect(wxl_tree_add(tree,0,"Root",4,&item)==0,"Tree root");int64_t result=-99;
 expect(wxl_set_tree_decoration(item,0,1)==0&&wxl_tree_decoration(item,0,&result)==0&&result==1,"Tree bold");
 expect(wxl_set_tree_decoration(item,1,0x123456)==0&&wxl_tree_decoration(item,1,&result)==0&&result==0x123456&&wxl_set_tree_decoration(item,1,-1)==0,"Tree copied colour/reset");
 std::atomic<int> foreign{99};std::thread worker([&]{foreign=wxl_report_list_property(target,2);expect(wxl_report_list_provider_active(0)==-1&&wxl_report_list_provider_active(123)==-1&&wxl_report_list_text_reply(expired_request,"late",4)==-1,"Foreign provider root/reply queries reject");});worker.join();expect(foreign==-2,"Foreign thread rejection");
 const auto scroll=wxl_scrolled_panel(frame,1,1),split=wxl_splitter(scroll,0,100,10,0.5),pane=wxl_panel(split),book=wxl_notebook(pane),page=wxl_panel(book);
 expect(scroll&&split&&pane&&book&&page&&wxl_notebook_add(book,page,"Page",4,1)==0,"Supported nested scroll/splitter/notebook ancestry");
 const auto saved_target=target;target=wxl_report_list_control(page,1,123,provide);
 expect(target&&wxl_report_list_insert_column(target,0,"Nested",6,100)==0&&wxl_set_report_list_count(target,1)==0,"Nested virtual provider construction");
 extra_checks=[&]{expect(wxl_set_notebook_selection(book,0,0)==-1&&wxl_set_notebook_label(book,0,"bad",3)==-1&&wxl_set_splitter_int(split,0,50)==-1&&wxl_set_scroll_pair(scroll,0,2,2)==-1,"Ancestor book caption/selection, splitter and scrolling mutations reject");};
 text=wxl_report_list_text(target,0,0,&length);expect(text&&length,"Nested provider readback");extra_checks={};expect(wxl_destroy(target)==0,"Nested provider destruction");target=saved_target;
 unsigned selected=0,tree_expanding=0;uint64_t context=0;
 event_checks=[&](uint64_t token,const wxl_event* event){
  context=event->context;
  expect(wxl_destroy(frame)==-1,"Actual views callback guards ancestor destruction");
  if(token==970){++selected;expect(event->kind==140&&event->origin_id==stored&&event->detail[0]==0,"Actual native list selection snapshot");expect(wxl_set_report_list_text(stored,0,0,"bad",3)==-1,"Native selection rejects report writes");int64_t detail=-1;expect(wxl_views_detail(context,0,&detail)==0&&detail==0&&wxl_context_set_id(context,0)==-1&&wxl_context_set_timestamp(context,0)==-1,"List detail lifetime and readonly ID/timestamp");expect(wxl_context_set_skipped(context,1)==0,"List Skip");}
  else if(token==971){++tree_expanding;expect(event->kind==152&&event->tree_item==item,"Actual native expanding token");expect(wxl_set_tree_decoration(item,0,0)==-1,"Native expansion rejects tree writes");expect(wxl_context_set_vetoed(context,1)==0,"Native tree expansion veto");}
 };
 expect(wxl_report_list_append(stored,values,sizes,2)==0&&wxl_bind(stored,140,970,wxID_ANY,wxID_ANY,1,0)==0,"Actual list event binding");
 as<ReportList>(stored)->SetItemState(0,wxLIST_STATE_SELECTED,wxLIST_STATE_SELECTED);expect(selected==1,"Actual wxListCtrl selection delivery");
 wxl_handle child=0;expect(wxl_tree_add(tree,item,"Child",5,&child)==0&&wxl_bind(tree,152,971,wxID_ANY,wxID_ANY,1,0)==0,"Expansion fixture");
 as<wxTreeCtrl>(tree)->Expand(tree_node(item).native);expect(tree_expanding==1&&!as<wxTreeCtrl>(tree)->IsExpanded(tree_node(item).native),"Actual native expansion veto readback");
 int64_t expired=0;expect(wxl_views_detail(context,0,&expired)==-1&&wxl_unbind(970)==1&&wxl_unbind(971)==1,"Expired views context and fixture unbind");event_checks={};
 unsigned drag_calls=0;
 event_checks=[&](uint64_t,const wxl_event* event){
   ++drag_calls;wxl_handle dropped=0;
   expect(event->kind==156&&event->tree_item==child&&event->detail[4]==11&&event->detail[5]==19,"Tree drag snapshot");
   expect(wxl_context_set_vetoed(event->context,0)==-1,"Built-in tree drag Allow must reject");
   expect(wxl_tree_drag(child,&dropped)==-1&&dropped==0,"Drag cannot enter from callback");
 };
 expect(wxl_bind(tree,156,972,wxID_ANY,wxID_ANY,1,0)==0,"Tree drag binding");
 {wxTreeEvent event(wxEVT_TREE_BEGIN_DRAG,window(tree)->GetId());event.SetEventObject(window(tree));
  event.SetItem(tree_node(child).native);event.SetPoint(wxPoint(11,19));event.Allow();
  window(tree)->GetEventHandler()->ProcessEvent(event);
  expect(!event.IsAllowed()&&drag_calls==1,"Built-in drag must remain vetoed after delivery");}
 expect(wxl_unbind(972)==1,"Tree drag unbind");event_checks={};
 {wxl_handle dropped=99;expect(wxl_tree_drag(child,&dropped)==-1&&dropped==0,"Hidden drag source rejects");}
 {
  auto* native=as<wxTreeCtrl>(tree);const auto hovered=tree_node(child).native;
  native->SetItemTextColour(hovered,wxColour(17,34,51));native->SetItemBackgroundColour(hovered,wxColour(68,85,102));
  {TreeOperation tree_guard(native);InputOperation input_guard(native);
   TreeMoveTarget receiver(native,"fixture");receiver.SetHover(hovered);receiver.OnLeave();
   expect(native->GetItemTextColour(hovered)==wxColour(17,34,51)&&native->GetItemBackgroundColour(hovered)==wxColour(68,85,102),"Leaving drag hover must restore custom colors");
   receiver.SetHover(hovered);}
  expect(native->GetItemTextColour(hovered)==wxColour(17,34,51)&&native->GetItemBackgroundColour(hovered)==wxColour(68,85,102),"Drag teardown must restore custom colors");
 }
 expect(wxl_show(frame)==0&&wxl_poll()==0,"Visible native drag fixture");
 unsigned driver_calls=0;bool cancel_drag=false;
 tree_drag_test_driver=[&](wxTreeCtrl* native,TreeMoveTarget* receiver){
   ++driver_calls;
   expect(callback_depth>0&&!EventScope::stack.empty()&&tree_operations.size()==1&&input_operations.size()==1,
          "Native drag must start inside the live mouse callback with lifetime guards");
   wxl_handle nested=0;
   expect(wxl_tree_delete(child)==-1&&wxl_destroy(tree)==-1&&wxl_destroy(frame)==-1&&
          wxl_tree_drag(child,&nested)==-1,"Active drag protects nodes/ancestors and rejects recursion");
   receiver->target=tree_node(item).native;
   expect(native->GetDropTarget()==receiver,"Temporary drop receiver installed during drag");
   return cancel_drag?wxDragCancel:wxDragMove;
 };
 event_checks=[&](uint64_t,const wxl_event* event){
   expect(event->kind==156,"Expected drag BEGIN");wxl_handle dropped=99;
   expect(wxl_tree_drag(item,&dropped)==-1&&dropped==0,"Drag source must match the current BEGIN item");
   expect(wxl_tree_drag(child,&dropped)==0&&dropped==(cancel_drag?0:item),"Live BEGIN must start drag and return only successful drop targets");
   expect(as<wxTreeCtrl>(tree)->GetDropTarget()==nullptr&&input_operations.empty(),"Native drag teardown completed before callback resumes");
 };
 expect(wxl_bind(tree,156,973,wxID_ANY,wxID_ANY,1,0)==0,"Synchronous drag binding");
 for(int pass=0;pass<2;++pass){cancel_drag=pass!=0;
   wxTreeEvent event(wxEVT_TREE_BEGIN_DRAG,window(tree)->GetId());event.SetEventObject(window(tree));
   event.SetItem(tree_node(child).native);event.Veto();window(tree)->GetEventHandler()->ProcessEvent(event);
   expect(!event.IsAllowed(),"Custom synchronous drag must not enable built-in dragging");
 }
 expect(driver_calls==2&&wxl_unbind(973)==1,"Native drag callback startup/cancel coverage");
 event_checks={};tree_drag_test_driver={};
 expect(views_provider_owners.empty(),"Provider ancestry scope restored");
 {wxListEvent event(wxEVT_LIST_ITEM_RIGHT_CLICK);event.SetIndex(7);event.SetColumn(2);event.SetPoint(wxPoint(11,19));wxl_event snapshot{};fill_views_detail(event,snapshot);expect(snapshot.detail[0]==7&&snapshot.detail[4]==11&&snapshot.detail[5]==19,"Copied native list event fields");}
 {wxListEvent event(wxEVT_LIST_CACHE_HINT);event.SetCacheFrom(3);event.SetCacheTo(900);wxl_event snapshot{};fill_views_detail(event,snapshot);expect(snapshot.detail[2]==3&&snapshot.detail[3]==900,"Copied native cache hint range");}
 expect(wxl_report_list_provider_active(123)==1&&wxl_destroy(target)==0&&wxl_report_list_provider_active(123)==0,"Provider retirement");
 delete window(frame);

}
