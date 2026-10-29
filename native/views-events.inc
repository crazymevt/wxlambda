// Include before native event dispatch, with wx/listctrl.h and wx/treectrl.h.
bool views_event_kind(int kind) { return (kind>=140 && kind<=145) || (kind>=150 && kind<=155); }
wxEventType views_event_type(int kind) {
 switch(kind) {
 case 140:return wxEVT_LIST_ITEM_SELECTED; case 141:return wxEVT_LIST_ITEM_DESELECTED;
 case 142:return wxEVT_LIST_ITEM_ACTIVATED; case 143:return wxEVT_LIST_ITEM_RIGHT_CLICK;
 case 144:return wxEVT_LIST_CACHE_HINT; case 145:return wxEVT_LIST_COL_CLICK;
 case 150:return wxEVT_TREE_ITEM_ACTIVATED; case 151:return wxEVT_TREE_ITEM_MENU;
 case 152:return wxEVT_TREE_ITEM_EXPANDING; case 153:return wxEVT_TREE_ITEM_EXPANDED;
 case 154:return wxEVT_TREE_ITEM_COLLAPSING; case 155:return wxEVT_TREE_ITEM_COLLAPSED;
 default:return wxEVT_NULL;
 }
}
int views_kind_for_type(wxEventType type) { for(int kind=140;kind<=155;++kind) if(views_event_kind(kind)&&views_event_type(kind)==type)return kind; return 0; }
bool views_tree_event(wxEventType type) { const int kind=views_kind_for_type(type);return kind>=150 && kind<=155; }
bool views_payload_readonly(wxEventType type) { return views_kind_for_type(type)!=0; }
std::vector<wxWeakRef<wxWindow>> views_provider_owners;
bool views_provider_running() {return !views_provider_owners.empty();}
bool protected_views_provider(wxWindow* object) {
 for(const auto& owner:views_provider_owners)
  for(auto* node=owner.get();node;node=node->GetParent())if(node==object)return true;
 return false;
}
void fill_views_detail(wxEvent& event,wxl_event& snapshot) {
 const int kind=views_kind_for_type(event.GetEventType()); if(!kind)return;
 if(kind<150) {
  auto* data=dynamic_cast<wxListEvent*>(&event);require(data,"Expected native list event");
  snapshot.detail[0]=data->GetIndex(); snapshot.detail[1]=data->GetColumn();
  if(kind==144) {snapshot.detail[2]=data->GetCacheFrom();snapshot.detail[3]=data->GetCacheTo();}
  if(kind==143) {snapshot.detail[4]=data->GetPoint().x;snapshot.detail[5]=data->GetPoint().y;}
 } else if(kind==151) {
  auto* data=dynamic_cast<wxTreeEvent*>(&event);require(data,"Expected native tree event");
  snapshot.detail[4]=data->GetPoint().x;snapshot.detail[5]=data->GetPoint().y;
 }
}
