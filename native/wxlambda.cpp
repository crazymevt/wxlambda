#include "wxlambda.h"
#include <wx/wx.h>
#include <wx/thread.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/scrolwin.h>
#include <wx/radiobox.h>
#include <wx/tglbtn.h>
#include <wx/gauge.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/menu.h>
#include <wx/toolbar.h>
#include <wx/statusbr.h>
#include <wx/artprov.h>
#include <wx/sysopt.h>
#include <wx/accel.h>
#include <wx/timer.h>
#include <wx/tooltip.h>
#include <wx/datectrl.h>
#include <wx/timectrl.h>
#include <wx/srchctrl.h>
#include <wx/activityindicator.h>
#include <wx/grid.h>
#include <wx/stc/stc.h>
#include <wx/aui/auibook.h>
#include <wx/webview.h>
#include "webview-cocoa.h"
#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include <wx/image.h>
#include <wx/imagpng.h>
#include <wx/mstream.h>
#include <wx/wfstream.h>
#include <wx/ffile.h>
#include <wx/dateevt.h>
#include <chrono>
#include <wx/notebook.h>
#include <wx/listbox.h>
#include <wx/treectrl.h>
#include <wx/splitter.h>
#include <cmath>
#include <cstdio>
#include <wx/textdlg.h>
#include <wx/choicdlg.h>
#include <wx/generic/msgdlgg.h>
#include <wx/generic/filedlgg.h>
#include <wx/modalhook.h>
#include <wx/filename.h>
#include <wx/weakref.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <memory>
#include <typeinfo>
#include <type_traits>
#include <unordered_set>
#include <vector>
#include <deque>
#include <limits>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#ifdef __APPLE__
#include <pthread.h>
#endif

namespace {
// wxEvtHandler exposes no public storage-kind query. Its getters assert when
// called in the opposite client-data mode, so only our native subclasses expose
// their own protected discriminator through this private implementation view.
class ClientDataAccess {
public:
    virtual ~ClientDataAccess() = default;
    virtual wxClientDataType ClientStorageKind() const = 0;
};
class TokenClientData final : public wxClientData {
public:
    explicit TokenClientData(uint64_t value) : token(value) { }
    const uint64_t token;
};
static_assert(wxEVT_CATEGORY_UI == 1 && wxEVT_CATEGORY_USER_INPUT == 2 &&
              wxEVT_CATEGORY_SOCKET == 4 && wxEVT_CATEGORY_TIMER == 8 &&
              wxEVT_CATEGORY_THREAD == 16,
              "Review the pinned public event-category baseline for this wxWidgets build");
bool session_type(wxEventType type) {
    return type == wxEVT_QUERY_END_SESSION || type == wxEVT_END_SESSION;
}
// Synthetic session probes must never fall through to wxApp's exit defaults.
class StandaloneHandler final : public wxEvtHandler, public ClientDataAccess {
    std::unordered_set<wxEvent*> queued_sessions;
public:
    wxClientDataType ClientStorageKind() const override { return m_clientDataType; }
    void QueueEvent(wxEvent* event) override {
        if (event && session_type(event->GetEventType())) queued_sessions.insert(event);
        wxEvtHandler::QueueEvent(event);
    }
    bool ProcessEvent(wxEvent& event) override {
        queued_sessions.erase(&event);
        return wxEvtHandler::ProcessEvent(event);
    }
    bool HasQueuedSessions() const { return !queued_sessions.empty(); }
    void ClearQueuedSessions() { queued_sessions.clear(); }
private:
    bool TryAfter(wxEvent& event) override {
        return session_type(event.GetEventType()) ? false : wxEvtHandler::TryAfter(event);
    }
};
bool process_deferred_ui_action();
unsigned idle_processing_depth = 0;
class App final : public wxApp, public ClientDataAccess {
public:
    wxClientDataType ClientStorageKind() const override { return m_clientDataType; }
    bool OnInit() override { return true; }
    bool ProcessIdle() override {
        struct IdleDepth { IdleDepth() { ++idle_processing_depth; } ~IdleDepth() { --idle_processing_depth; } } depth;
        const bool more = wxApp::ProcessIdle();
        return process_deferred_ui_action() || more;
    }
};
thread_local std::string error;
thread_local std::string label_result;
thread_local std::string metadata_result;
void require(bool ok, const char* message);
class FixedSplitter : public wxSplitterWindow {
public:
    using wxSplitterWindow::wxSplitterWindow;
    int requested_position = 0;
    void OnDoubleClickSash(int, int) override { } // Fixed two-pane ownership.
};
struct AcceleratorSpec { int key, flags, command; };
std::unordered_map<wxWindow*, std::vector<AcceleratorSpec>> frame_accelerators;
unsigned input_creation_depth = 0;
struct ManagedWindowState { wxWeakRef<wxWindow> lifetime; bool retiring=false; };
std::unordered_map<wxWindow*,ManagedWindowState> managed_window_states;
std::unordered_set<wxWindow*> web_owners;
std::unordered_map<wxWindow*, bool> input_owners; // true during native-base teardown
std::unordered_set<wxWindow*> toolbar_owners;
std::unordered_set<wxWindow*> realized_toolbars;
std::unordered_set<wxWindow*> splitter_owners;
constexpr int splitter_pixel_limit = std::numeric_limits<int>::max() / 4;
void splitter_pixel(int value, bool positive = false) {
    require(value >= (positive ? 1 : 0) && value <= splitter_pixel_limit, "Splitter pixel value out of supported range");
}
struct TreeNode {
    wxl_handle owner, parent;
    wxWindow* window;
    wxTreeItemId native;
    std::unordered_set<wxl_handle> children;
};
std::unordered_map<wxl_handle, TreeNode> tree_nodes;
uint64_t tree_revision = 0;
std::unordered_map<wxWindow*, wxl_handle> tree_owners;
void forget_tree_nodes(wxWindow* object) {
    tree_owners.erase(object); // Suppress callbacks during the native base destructor too.
    bool removed = false;
    for (auto it = tree_nodes.begin(); it != tree_nodes.end(); ) {
        if (it->second.window == object) { it = tree_nodes.erase(it); removed = true; } else ++it;
    }
    if (removed) ++tree_revision;
}
void invalidate_tree_subtree(wxl_handle id) {
    const auto found = tree_nodes.find(id);
    if (found == tree_nodes.end()) return;
    const auto parent = found->second.parent;
    std::vector<wxl_handle> removed{id};
    for (size_t i = 0; i < removed.size(); ++i) {
        const auto node = tree_nodes.find(removed[i]);
        if (node != tree_nodes.end()) removed.insert(removed.end(), node->second.children.begin(), node->second.children.end());
    }
    if (parent && tree_nodes.count(parent)) tree_nodes.at(parent).children.erase(id);
    for (const auto item : removed) tree_nodes.erase(item);
    ++tree_revision;
}
wxl_handle tree_token(wxWindow* owner, const wxTreeItemId& item) {
    if (!item.IsOk()) return 0;
    for (const auto& node : tree_nodes) if (node.second.window == owner && node.second.native == item) return node.first;
    return 0;
}
std::unordered_map<wxl_handle, wxWeakRef<wxWindow>> windows;
std::unordered_map<wxl_handle, std::unique_ptr<wxEvtHandler>> owned_handlers;
struct PushedHandler { wxWeakRef<wxWindow> window; wxl_handle window_id; };
std::unordered_map<wxl_handle, PushedHandler> pushed_handlers;
std::unordered_map<wxl_handle, std::unique_ptr<wxEvent>> owned_events;
std::unordered_map<wxl_handle, uint64_t> handlers;
struct Binding;
std::unordered_map<uint64_t, std::shared_ptr<Binding>> bindings;
std::unordered_set<wxl_handle> pending_destruction;
struct MenuResource { wxObject* object; wxl_handle frame, parent; int kind; };
std::unordered_map<wxl_handle, MenuResource> menu_resources;
class ManagedMenuBar final : public wxMenuBar {
public: wxl_handle identity = 0;
    ~ManagedMenuBar() override { menu_resources.erase(identity); }
};
class ManagedMenu final : public wxMenu {
public: wxl_handle identity = 0;
    ~ManagedMenu() override { menu_resources.erase(identity); }
};
class ManagedMenuItem final : public wxMenuItem {
public: using wxMenuItem::wxMenuItem; wxl_handle identity = 0;
    ~ManagedMenuItem() override { menu_resources.erase(identity); }
};
struct RegisteredEvent { wxEventType type; int family; };
std::unordered_map<int, RegisteredEvent> registered_events;
const wxEventType web_event_types[] = {wxNewEventType(),wxNewEventType(),wxNewEventType(),wxNewEventType()};
bool web_event_kind(int kind) { return kind>=WXL_EVENT_WEB_MESSAGE && kind<=WXL_EVENT_WEB_ERROR; }
bool web_event_type(wxEventType type) { for (auto item:web_event_types) if (item==type) return true; return false; }
constexpr int first_registered_event_kind = 256;
int next_event_kind = first_registered_event_kind;
wxWeakRef<wxDialog> active_modal;
std::unordered_map<wxWindow*, size_t> dialog_choice_counts;
std::unordered_map<wxWindow*, int> dialog_message_buttons;
std::unordered_map<wxWindow*, int> dialog_file_filters;
std::vector<wxWeakRef<wxDialog>> actual_modals;
std::vector<wxWeakRef<wxWindow>> dialog_operation_windows;
bool modal_cancel_requested = false;
bool pending_callback_error = false;
uint64_t modal_workflow_epoch = 0;
wxIdleMode initial_idle_mode = wxIDLE_PROCESS_ALL;
wxl_handle next_handle = 1;
wxl_handle application_handle = 0;
wxl_callback dispatch = nullptr;
wxl_ui_callback ui_dispatch = nullptr;
uint64_t ui_work_sequence = 0;
struct UiAction { uint64_t token; wxl_handle owner_id; wxWeakRef<wxWindow> owner; uint64_t sequence; };
unsigned timer_notify_depth = 0;
bool finishing = false;
bool window_destruction_pending(wxWindow* object);
class UiTimer final : public wxTimer {
public:
    uint64_t token, sequence = 0;
    wxl_handle owner_id;
    wxWeakRef<wxWindow> owner;
    bool one_shot, due = false, retired = false;
    std::chrono::steady_clock::time_point not_before;
    UiTimer(uint64_t id, wxl_handle owner_handle, wxWindow* window, int milliseconds, bool once)
        : token(id), owner_id(owner_handle), owner(window), one_shot(once),
          not_before(std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds)) { }
    ~UiTimer() override { if (IsRunning()) Stop(); }
    void Notify() override {
        struct NotifyScope { NotifyScope() { ++timer_notify_depth; } ~NotifyScope() { --timer_notify_depth; } } depth;
        if (finishing || retired) return;
        if (owner_id && (!owner.get() || window_destruction_pending(owner.get()))) {
            retired = true;
            if (IsRunning()) Stop();
            if (!due) { due = true; sequence = ++ui_work_sequence; }
            try { wxWakeUpIdle(); } catch (...) { } // Due cleanup remains visible to the next poll.
            return;
        }
        if (due || std::chrono::steady_clock::now() < not_before) return;
        // Native IDs can be reused (MSW); the initial steady-clock lower bound
        // ignores a queued old native notification assigned to a new timer.
        due = true; sequence = ++ui_work_sequence;
        if (one_shot && IsRunning()) Stop();
        try { wxWakeUpIdle(); } catch (...) { } // No exception crosses the native notification boundary.
    }
};
std::unordered_map<uint64_t, std::unique_ptr<UiTimer>> ui_timers;
std::deque<UiAction> ui_actions;
bool ui_action_running = false;
uint64_t active_ui_action_token = 0;
thread_local unsigned native_entry_depth = 0;
unsigned ui_driver_depth = 0;
struct UiDriverScope {
    unsigned previous = ui_driver_depth;
    UiDriverScope() { ui_driver_depth = native_entry_depth; }
    ~UiDriverScope() { ui_driver_depth = previous; }
};
bool started = false, used = false, running = false, pumping = false;
unsigned callback_depth = 0;
struct CallbackScope {
    CallbackScope() { ++callback_depth; }
    ~CallbackScope() { --callback_depth; }
};
struct EventScope {
    uint64_t token;
    static uint64_t next;
    struct Borrow { uint64_t token; wxEvent* event; unsigned depth; uint64_t user_data; };
    static std::vector<Borrow> stack;
    explicit EventScope(wxEvent& event, uint64_t user_data = 0) : token(++next) { stack.push_back({token, &event, callback_depth + 1, user_data}); }
    ~EventScope() { stack.pop_back(); }
};
uint64_t EventScope::next = 0;
std::vector<EventScope::Borrow> EventScope::stack;
struct PumpScope {
    PumpScope() { pumping = true; }
    ~PumpScope() { pumping = false; }
};

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
void main_thread() {
#ifdef __APPLE__
    require(pthread_main_np() != 0, "GUI calls must run on the process main thread");
#endif
    require(wxIsMainThread(), "GUI calls must run on the main thread");
}
void ready() { main_thread(); require(started, "Application is not initialized"); }
wxEvent& current_event(uint64_t token) {
    ready();
    require(token && !EventScope::stack.empty() && EventScope::stack.back().token == token
            && EventScope::stack.back().depth == callback_depth,
            "Event context is expired or is not the current callback");
    return *EventScope::stack.back().event;
}
// Destroy() on top-level windows may only schedule deletion: IsBeingDeleted()
// alone stays false until later. Include native scheduled deletion and bridge
// deferred requests for the whole parent chain before any native access.
bool window_destruction_pending(wxWindow* object) {
    for (auto* node = object; node; node = node->GetParent()) {
        const auto managed=managed_window_states.find(node);
        if (managed!=managed_window_states.end() && managed->second.lifetime.get()==node && managed->second.retiring) return true;
        if (node->IsBeingDeleted() || wxTheApp->IsScheduledForDestruction(node)) return true;
        for (const auto id : pending_destruction) {
            const auto found = windows.find(id);
            if (found != windows.end() && found->second.get() == node) return true;
        }
    }
    return false;
}
wxWindow* window(wxl_handle id) {
    ready();
    const auto it = windows.find(id);
    require(it != windows.end(), "Invalid or destroyed window handle");
    require(!pending_destruction.count(id), "Window destruction is pending");
    auto* object = it->second.get();
    require(object != nullptr, "Invalid or destroyed window handle");
    require(!window_destruction_pending(object), "Window or ancestor destruction is pending");
    return object;
}
wxEvtHandler* event_handler(wxl_handle id) {
    ready();
    require(!pending_destruction.count(id), "Handler destruction is pending");
    if (id && id == application_handle) return wxTheApp;
    if (windows.find(id) != windows.end()) return window(id);
    const auto it = owned_handlers.find(id);
    require(it != owned_handlers.end(), "Invalid or destroyed event-handler handle");
    return it->second.get();
}
const TokenClientData* token_client_data(wxEvtHandler* object, bool strict) {
    const auto* view = dynamic_cast<const ClientDataAccess*>(object);
    if (!view) {
        require(!strict, "Client data requires a bridge-managed native handler");
        return nullptr;
    }
    const auto kind = view->ClientStorageKind();
    if (kind == wxClientData_None) return nullptr;
    if (kind != wxClientData_Object) {
        // Even an untyped NULL locks wxEvtHandler into void-data mode and
        // cannot safely be converted with its public SetClientObject API.
        require(!strict, "Untyped native client data is not supported");
        return nullptr;
    }
    const auto* data = object->GetClientObject();
    const auto* token = dynamic_cast<const TokenClientData*>(data);
    require(!strict || !data || token, "Foreign native client object is not supported");
    return token;
}
wxEvtHandler* standalone_handler(wxl_handle id) {
    auto* object = event_handler(id);
    require(owned_handlers.count(id) != 0, "Handler chains require owned standalone handlers");
    require(!pushed_handlers.count(id), "Pop/remove a pushed handler before standalone chain access");
    return object;
}
wxl_handle owned_handler_id(wxEvtHandler* object) {
    if (!object) return 0;
    for (const auto& item : owned_handlers) {
        if (item.second.get() == object) {
            require(!pending_destruction.count(item.first), "Chain peer destruction is pending");
            return item.first;
        }
    }
    throw std::runtime_error("Chain peer is not a managed standalone handler");
}
void validate_chain_neighbors(wxEvtHandler* object) {
    if (auto* next = object->GetNextHandler()) {
        owned_handler_id(next);
        require(next->GetPreviousHandler() == object, "Inconsistent next-handler link");
    }
    if (auto* previous = object->GetPreviousHandler()) {
        owned_handler_id(previous);
        require(previous->GetNextHandler() == object, "Inconsistent previous-handler link");
    }
}
wxl_handle origin_id(const wxEvent& event) {
    // Compare identities only: GetEventObject may be null, untracked or stale.
    const auto* origin = event.GetEventObject();
    if (!origin) return 0;
    if (application_handle && origin == wxTheApp) return application_handle;
    for (const auto& item : owned_handlers)
        if (origin == item.second.get()) return item.first;
    for (const auto& item : windows)
        if (item.second.get() && origin == item.second.get()) return item.first;
    for (const auto& item : menu_resources)
        if (origin == item.second.object) return item.first;
    return 0;
}
bool isolated_handler(wxl_handle id) {
    return owned_handlers.count(id) && !pushed_handlers.count(id);
}
wxl_handle find_owned_handler(wxEvtHandler* object) noexcept {
    for (const auto& item : owned_handlers)
        if (item.second.get() == object) return item.first;
    return 0;
}
void validate_window_stack(wxWindow* owner) {
    require(!web_owners.count(owner), "WebViews retain private event delivery; managed window stacks are unsupported");
    require(!dynamic_cast<wxAuiNotebook*>(owner) && !dynamic_cast<wxStyledTextCtrl*>(owner), "AUI/STC private native handlers cannot be replaced");
    require(!dynamic_cast<wxGrid*>(owner), "Grids retain private scroll handlers; managed window stacks are unsupported");
    require(!dynamic_cast<wxScrolledWindow*>(owner), "Scrolled panels retain a private native scroll helper; managed window stacks are unsupported");
    auto* node = owner->GetEventHandler();
    wxEvtHandler* previous = nullptr;
    size_t count = 0;
    while (node != owner) {
        require(node != nullptr && ++count <= owned_handlers.size(), "Invalid window handler stack");
        const auto id = owned_handler_id(node);
        const auto attached = pushed_handlers.find(id);
        require(attached != pushed_handlers.end() && attached->second.window.get() == owner,
                "Window stack contains an unmanaged handler");
        require(node->GetPreviousHandler() == previous, "Inconsistent window handler stack");
        previous = node;
        node = node->GetNextHandler();
    }
}
void detach_window_stack(wxWindow* owner) noexcept {
    // Run before wxWindowBase destruction, even if destroy events were consumed.
    while (auto* head = owner->GetEventHandler()) {
        if (head == owner) return;
        const auto id = find_owned_handler(head);
        const auto attached = pushed_handlers.find(id);
        if (!id || attached == pushed_handlers.end() || attached->second.window.get() != owner) return;
        owner->PopEventHandler(false);
        pushed_handlers.erase(attached);
    }
}
void remove_pushed_handler(wxl_handle id) {
    const auto attached = pushed_handlers.find(id);
    if (attached == pushed_handlers.end()) return;
    auto* owner = attached->second.window.get();
    require(owner != nullptr, "Pushed handler owner is no longer live");
    validate_window_stack(owner);
    require(owner->RemoveEventHandler(owned_handlers.at(id).get()), "Pushed handler not found");
    pushed_handlers.erase(attached);
}
template<class T> class ManagedWindow final : public T, public ClientDataAccess {
public:
    using T::T;
    bool Destroy() override {
        wxWeakRef<wxWindow> weak(this);
        const auto state=managed_window_states.find(this);
        const bool managed=state!=managed_window_states.end(), retired=managed && state->second.retiring;
        if (managed) state->second.retiring=true;
        const auto owner=input_owners.find(this);
        const bool tracked=owner!=input_owners.end(), previous=tracked && owner->second;
        if (tracked) owner->second=true;
        const auto restore=[&] {
            if (!weak.get()) return;
            if (managed) { const auto current=managed_window_states.find(weak.get()); if(current!=managed_window_states.end()) current->second.retiring=retired; }
            if (tracked) { const auto current=input_owners.find(weak.get()); if(current!=input_owners.end()) current->second=previous; }
        };
        try { const bool accepted=T::Destroy(); if(!accepted) restore(); return accepted; }
        catch (...) { restore(); throw; }
    }
    wxClientDataType ClientStorageKind() const override { return this->m_clientDataType; }
    ~ManagedWindow() override { if(auto state=managed_window_states.find(this);state!=managed_window_states.end()) state->second.retiring=true; if (auto found = input_owners.find(this); found != input_owners.end()) found->second = true; frame_accelerators.erase(this); toolbar_owners.erase(this); realized_toolbars.erase(this); splitter_owners.erase(this); forget_tree_nodes(this); dialog_choice_counts.erase(this); dialog_message_buttons.erase(this); dialog_file_filters.erase(this); detach_window_stack(this); }
};
void set_registered_type(wxEvent& event, int type) {
        const RegisteredEvent* source = nullptr;
        const RegisteredEvent* target = nullptr;
        for (const auto& item : registered_events) {
            if (item.second.type == event.GetEventType()) source = &item.second;
            if (item.second.type == type) target = &item.second;
        }
        require(source && target, "Type mutation requires registered source and target tags");
        require(source->family == target->family, "Type mutation cannot change event family");
        auto* command = dynamic_cast<wxCommandEvent*>(&event);
        auto* notify = dynamic_cast<wxNotifyEvent*>(&event);
        require((source->family == WXL_FAMILY_COMMAND && command && !notify) ||
                (source->family == WXL_FAMILY_NOTIFY && notify),
                "Registered tag does not match the actual native event class");
        event.SetEventType(type);
}
template<class T> T* as(wxl_handle id) {
    auto* object = dynamic_cast<T*>(window(id));
    require(object != nullptr, "Wrong control type");
    return object;
}
wxBoxSizer* box(wxWindow* parent) {
    auto* sizer = dynamic_cast<wxBoxSizer*>(parent->GetSizer());
    require(sizer != nullptr, "Create a row or column before adding layout items");
    return sizer;
}
std::vector<wxWeakRef<wxNotebook>> notebook_operations;
bool protected_notebook_ancestor(wxWindow* object) {
    for (const auto& book : notebook_operations)
        for (auto* node = static_cast<wxWindow*>(book.get()); node; node = node->GetParent())
            if (node == object) return true;
    return false;
}
bool attached_notebook_page(wxWindow* object) {
    auto* book = dynamic_cast<wxNotebook*>(object->GetParent());
    return book && book->FindPage(object) != wxNOT_FOUND;
}
struct NotebookOperation {
    explicit NotebookOperation(wxNotebook* book) {
        for (const auto& active : notebook_operations)
            require(active.get() != book, "Notebook operation is already in progress");
        notebook_operations.emplace_back(book);
    }
    ~NotebookOperation() { notebook_operations.pop_back(); }
};
struct NotebookCallbackOperation {
    bool added = false, allow = true;
    explicit NotebookCallbackOperation(wxEvent& event) {
        if (event.GetEventType() != wxEVT_NOTEBOOK_PAGE_CHANGING && event.GetEventType() != wxEVT_NOTEBOOK_PAGE_CHANGED) return;
        auto* book = dynamic_cast<wxNotebook*>(event.GetEventObject());
        if (!book || window_destruction_pending(book)) { allow = false; return; }
        for (const auto& active : notebook_operations) if (active.get() == book) return;
        notebook_operations.emplace_back(book); added = true;
    }
    ~NotebookCallbackOperation() { if (added) notebook_operations.pop_back(); }
};
std::vector<wxWeakRef<wxListBox>> list_callback_operations;
bool protected_list_ancestor(wxWindow* object) {
    for (const auto& list : list_callback_operations)
        for (auto* node = static_cast<wxWindow*>(list.get()); node; node = node->GetParent())
            if (node == object) return true;
    return false;
}
struct ListCallbackOperation {
    bool added = false, allow = true;
    explicit ListCallbackOperation(wxEvent& event) {
        if (event.GetEventType() != wxEVT_LISTBOX && event.GetEventType() != wxEVT_LISTBOX_DCLICK) return;
        auto* list = dynamic_cast<wxListBox*>(event.GetEventObject());
        if (!list) return; // Synthetic command events may target any handler.
        if (window_destruction_pending(list)) { allow = false; return; }
        for (const auto& active : list_callback_operations) if (active.get() == list) return;
        list_callback_operations.emplace_back(list); added = true;
    }
    ~ListCallbackOperation() { if (added) list_callback_operations.pop_back(); }
};
std::vector<wxWeakRef<wxTreeCtrl>> tree_operations;
bool protected_tree_ancestor(wxWindow* object) {
    for (const auto& tree : tree_operations)
        for (auto* node = static_cast<wxWindow*>(tree.get()); node; node = node->GetParent())
            if (node == object) return true;
    return false;
}
struct TreeOperation {
    explicit TreeOperation(wxTreeCtrl* tree) {
        for (const auto& active : tree_operations) require(active.get() != tree, "Defer tree mutation until its callback/operation returns");
        tree_operations.emplace_back(tree);
    }
    ~TreeOperation() { tree_operations.pop_back(); }
};
struct TreeCallbackOperation {
    bool added = false, allow = true;
    explicit TreeCallbackOperation(wxEvent& event) {
        if (event.GetEventType() != wxEVT_TREE_SEL_CHANGING && event.GetEventType() != wxEVT_TREE_SEL_CHANGED) return;
        auto* tree = dynamic_cast<wxTreeCtrl*>(event.GetEventObject());
        if (!tree || !tree_owners.count(tree) || window_destruction_pending(tree)) { allow = false; return; }
        for (const auto& active : tree_operations) if (active.get() == tree) return;
        tree_operations.emplace_back(tree); added = true;
    }
    ~TreeCallbackOperation() { if (added) tree_operations.pop_back(); }
};
wxl_handle event_tree_token(wxEvent& event) {
    auto* tree_event = dynamic_cast<wxTreeEvent*>(&event);
    auto* tree = dynamic_cast<wxTreeCtrl*>(event.GetEventObject());
    return tree_event && tree ? tree_token(tree, tree_event->GetItem()) : 0;
}
TreeNode& tree_node(wxl_handle id) {
    ready();
    const auto found = tree_nodes.find(id);
    require(found != tree_nodes.end(), "Invalid or deleted tree item");
    require(as<wxTreeCtrl>(found->second.owner) == found->second.window, "Tree item owner is invalid");
    return found->second;
}
struct SplitterActive { wxWeakRef<wxSplitterWindow> splitter; wxWeakRef<wxWindow> first, second; };
std::vector<SplitterActive> splitter_operations;
bool sash_event(wxEvent& event) {
    return event.GetEventType() == wxEVT_SPLITTER_SASH_POS_CHANGING || event.GetEventType() == wxEVT_SPLITTER_SASH_POS_CHANGED;
}
bool protected_splitter_ancestor(wxWindow* object) {
    for (const auto& active : splitter_operations) {
        if (active.first.get() == object || active.second.get() == object) return true;
        for (auto* node = static_cast<wxWindow*>(active.splitter.get()); node; node = node->GetParent()) if (node == object) return true;
    }
    return false;
}
bool attached_splitter_pane(wxWindow* object) {
    auto* splitter = dynamic_cast<wxSplitterWindow*>(object->GetParent());
    return splitter && (splitter->GetWindow1() == object || splitter->GetWindow2() == object);
}
struct SplitterOperation {
    explicit SplitterOperation(wxSplitterWindow* splitter, wxWindow* first = nullptr, wxWindow* second = nullptr) {
        for (const auto& active : splitter_operations) require(active.splitter.get() != splitter, "Defer splitter mutation until callback/operation returns");
        splitter_operations.push_back({splitter, first, second});
    }
    ~SplitterOperation() { splitter_operations.pop_back(); }
};
struct SplitterCallbackOperation {
    bool added = false, allow = true;
    explicit SplitterCallbackOperation(wxEvent& event) {
        if (!sash_event(event)) return;
        auto* splitter = dynamic_cast<wxSplitterWindow*>(event.GetEventObject());
        if (!splitter || !splitter_owners.count(splitter) || window_destruction_pending(splitter)) { allow = false; return; }
        for (const auto& active : splitter_operations) if (active.splitter.get() == splitter) return;
        splitter_operations.push_back({splitter, splitter->GetWindow1(), splitter->GetWindow2()}); added = true;
    }
    ~SplitterCallbackOperation() { if (added) splitter_operations.pop_back(); }
};
int event_sash_value(wxEvent& event) {
    auto* splitter = dynamic_cast<wxSplitterEvent*>(&event);
    return splitter && sash_event(event) ? splitter->GetSashPosition() : 0;
}
// Origin comparison never dereferences an untracked event-object pointer.
std::vector<wxWeakRef<wxToolBar>> toolbar_operations;
bool protected_toolbar_ancestor(wxWindow* object) {
    for (const auto& bar : toolbar_operations)
        for (auto* node = static_cast<wxWindow*>(bar.get()); node; node = node->GetParent())
            if (node == object) return true;
    return false;
}
void toolbar_mutable(wxToolBar* bar) {
    for (const auto& active : toolbar_operations)
        require(active.get() != bar, "Defer same-toolbar mutation/activation until action returns");
}
struct ToolbarOperation {
    explicit ToolbarOperation(wxToolBar* bar) { toolbar_mutable(bar); toolbar_operations.emplace_back(bar); }
    ~ToolbarOperation() { toolbar_operations.pop_back(); }
};
struct ToolbarCallbackOperation {
    bool added = false, allow = true;
    explicit ToolbarCallbackOperation(wxEvent& event) {
        if (event.GetEventType() != wxEVT_TOOL) return;
        for (const auto& entry : windows) {
            auto* live = entry.second.get();
            if (!live || event.GetEventObject() != live) continue;
            auto* bar = dynamic_cast<wxToolBar*>(live);
            if (!bar) return;
            if (!toolbar_owners.count(bar) || window_destruction_pending(bar)) { allow = false; return; }
            for (const auto& active : toolbar_operations) if (active.get() == bar) return;
            toolbar_operations.emplace_back(bar); added = true; return;
        }
    }
    ~ToolbarCallbackOperation() { if (added) toolbar_operations.pop_back(); }
};
// Native accelerator commands have no portable origin marker. Protect all
// installed frame subtrees for any MENU callback, including app/stack receivers.
std::vector<wxWeakRef<wxWindow>> accelerator_action_frames;
bool protected_accelerator_window(wxWindow* object) {
    for (const auto& frame : accelerator_action_frames)
        for (auto* node = object; node; node = node->GetParent())
            if (frame.get() == node) return true;
    return false;
}
struct AcceleratorCallbackOperation {
    size_t previous;
    explicit AcceleratorCallbackOperation(wxEvent& event) : previous(accelerator_action_frames.size()) {
        if (event.GetEventType() != wxEVT_MENU) return;
        for (const auto& entry : frame_accelerators) {
            if (entry.second.empty()) continue;
            bool present = false;
            for (const auto& active : accelerator_action_frames) if (active.get() == entry.first) { present = true; break; }
            if (!present) accelerator_action_frames.emplace_back(entry.first);
        }
    }
    ~AcceleratorCallbackOperation() { accelerator_action_frames.resize(previous); }
};
int accelerator_key(int key) {
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) return key;
    require(key >= 1001 && key <= 1012, "Unsupported accelerator key"); return WXK_F1 + key - 1001;
}
int accelerator_flags(int flags) {
    require(flags >= 0 && flags <= 15 && (flags & 9) != 9, "Invalid/duplicate control modifiers");
    return ((flags & 1) ? wxACCEL_CTRL : 0) | ((flags & 2) ? wxACCEL_SHIFT : 0) |
           ((flags & 4) ? wxACCEL_ALT : 0) | ((flags & 8) ? wxACCEL_RAW_CTRL : 0);
}
unsigned focus_operation_depth = 0;
unsigned unknown_input_callback_depth=0;
std::vector<wxWeakRef<wxWindow>> input_operations;
bool protected_input_ancestor(wxWindow* object) {
    if (focus_operation_depth || unknown_input_callback_depth) return true; // Traversal may touch any sibling or old focus window.
    for (const auto& active : input_operations)
        for (auto* node = active.get(); node; node = node->GetParent()) if (node == object) return true;
    return false;
}
struct InputOperation {
    explicit InputOperation(wxWindow* object) {
        for (const auto& active : input_operations) require(active.get() != object, "Defer same-control input mutation until callback/operation returns");
        input_operations.emplace_back(object);
    }
    ~InputOperation() { input_operations.pop_back(); }
};
bool input_origin_contains(wxWindow* object, const wxObject* origin) {
    if (origin == object) return true;
    for (auto* child : object->GetChildren()) if (input_origin_contains(child, origin)) return true;
    return false;
}
#include "stc-events.inc"
#include "input-events.inc"
#include "aui-events.inc"
struct InputCallbackOperation {
    size_t previous=input_operations.size();
    bool allow=true,unknown=false;
    void protect(wxWindow* object) {
        if (window_destruction_pending(object)) { allow=false; return; }
        for (const auto& active:input_operations) if (active.get()==object) return;
        input_operations.emplace_back(object);
    }
    explicit InputCallbackOperation(wxEvent& event) {
        const bool raw=input_kind_for_type(event.GetEventType())!=0;
        if (aui_kind_for_type(event.GetEventType()) && !aui_public_event(event)) { allow=false; return; }
        if (!raw && !stc_kind_for_type(event.GetEventType()) && !aui_kind_for_type(event.GetEventType()) && !web_event_type(event.GetEventType()) && event.GetEventType()!=wxEVT_BUTTON && event.GetEventType()!=wxEVT_SEARCH && event.GetEventType()!=wxEVT_SEARCH_CANCEL && event.GetEventType()!=wxEVT_TIME_CHANGED && event.GetEventType()!=wxEVT_DATE_CHANGED && event.GetEventType()!=wxEVT_COMBOBOX && event.GetEventType()!=wxEVT_TOGGLEBUTTON && event.GetEventType()!=wxEVT_RADIOBOX && event.GetEventType()!=wxEVT_SLIDER && event.GetEventType()!=wxEVT_SPINCTRL && event.GetEventType()!=wxEVT_TEXT) return;
        if (input_creation_depth) { allow=false; return; }
        // DESTROY can be consumed/disabled. Weak lifetimes still invalidate both maps.
        for (auto state=managed_window_states.begin();state!=managed_window_states.end();) {
            if (!state->second.lifetime.get()) { input_owners.erase(state->first); state=managed_window_states.erase(state); }
            else ++state;
        }
        for (const auto& entry:managed_window_states)
            if (entry.second.retiring && entry.second.lifetime.get() &&
                (!event.GetEventObject() || input_origin_contains(entry.first,event.GetEventObject()))) { allow=false; return; }
        for (const auto& entry:input_owners) if (entry.second &&
            (!event.GetEventObject() || input_origin_contains(entry.first,event.GetEventObject()))) { allow=false; return; }
        wxWindow* composite=nullptr;
        for (const auto& entry:input_owners) if (event.GetEventObject()==entry.first) { composite=entry.first; break; }
        if (!composite) for (const auto& entry:input_owners)
            if (!window_destruction_pending(entry.first) && input_origin_contains(entry.first,event.GetEventObject())) { composite=entry.first; break; }
        if (composite) protect(composite);
        if (raw) {
            wxWindow* exact=nullptr;
            for (const auto& entry:windows) if (entry.second.get() && entry.second.get()==event.GetEventObject()) { exact=entry.second.get(); break; }
            if (exact) protect(exact);
            else {
                bool known=false;
                // Compare the origin only. Never dereference an untracked/stale event pointer.
                for (const auto& entry:windows) if (auto* owner=entry.second.get())
                    if (!window_destruction_pending(owner) && input_origin_contains(owner,event.GetEventObject())) { protect(owner); known=true; }
                if (!known && !composite) {
                    ++unknown_input_callback_depth; unknown=true;
                    input_operations.emplace_back(nullptr); // Gate nested GUI entry even without a known source.
                }
            }
        }
    }
    ~InputCallbackOperation() { input_operations.resize(previous); if (unknown) --unknown_input_callback_depth; }
};
// Calendar values are local civil dates, never Unix times or universal-time.
int calendar_code(const wxDateTime& value) {
    if (!value.IsValid()) return 0;
    const int year=value.GetYear(), month=static_cast<int>(value.GetMonth())+1, day=value.GetDay();
    if (year<1900 || year>2100) return 0;
    return year*10000+month*100+day;
}
wxDateTime calendar_value(int year, int month, int day) {
    require(year>=1900 && year<=2100 && month>=1 && month<=12, "Date requires year1900..2100 and month1..12");
    const auto native_month=static_cast<wxDateTime::Month>(month-1);
    require(day>=1 && day<=wxDateTime::GetNumberOfDays(native_month,year), "Invalid Gregorian day");
    const wxDateTime value(static_cast<wxDateTime::wxDateTime_t>(day),native_month,year);
    require(calendar_code(value)==year*10000+month*100+day, "Date is not representable in the native local calendar");
    return value;
}
int event_calendar_code(wxEvent& event) {
    auto* date=dynamic_cast<wxDateEvent*>(&event);
    return date && event.GetEventType()==wxEVT_DATE_CHANGED ? calendar_code(date->GetDate()) : 0;
}
// A positive scalar preserves midnight while zero denotes absent event metadata.
int clock_code(const wxDateTime& value) {
    if (!value.IsValid()) return 0;
    const int hour=value.GetHour(), minute=value.GetMinute(), second=value.GetSecond();
    if (hour<0 || hour>23 || minute<0 || minute>59 || second<0 || second>59) return 0;
    return 1+hour*3600+minute*60+second;
}
wxDateTime clock_value(int hour, int minute, int second) {
    require(hour>=0 && hour<=23 && minute>=0 && minute<=59 && second>=0 && second<=59,
            "Time requires hour0..23 and minute/second0..59");
    // Match wxTimePickerCtrl::SetTime's fixed anchor, avoiding today's DST gaps.
    const wxDateTime value(1,wxDateTime::Jan,2012,hour,minute,second);
    require(clock_code(value)==1+hour*3600+minute*60+second,
            "Time is not representable in the native fixed local calendar");
    return value;
}
int event_clock_code(wxEvent& event) {
    auto* time=dynamic_cast<wxDateEvent*>(&event);
    return time && event.GetEventType()==wxEVT_TIME_CHANGED ? clock_code(time->GetDate()) : 0;
}
void numeric_bounds(int minimum, int maximum) {
    require(minimum >= -1000000 && maximum <= 1000000 && minimum < maximum, "Numeric range requires -1000000 <= minimum < maximum <= 1000000");
}
wxWindow* numeric_control(wxl_handle id) {
    auto* object = window(id);
    require(dynamic_cast<wxSlider*>(object) || dynamic_cast<wxSpinCtrl*>(object), "Expected slider or integer spin control"); return object;
}
int numeric_min(wxWindow* object) {
    if (auto* slider = dynamic_cast<wxSlider*>(object)) return slider->GetMin();
    return static_cast<wxSpinCtrl*>(object)->GetMin();
}
int numeric_max(wxWindow* object) {
    if (auto* slider = dynamic_cast<wxSlider*>(object)) return slider->GetMax();
    return static_cast<wxSpinCtrl*>(object)->GetMax();
}
int numeric_read(wxWindow* object) {
    int value;
    if (auto* slider = dynamic_cast<wxSlider*>(object)) value = slider->GetValue();
    else {
        auto* spin = static_cast<wxSpinCtrl*>(object); long long parsed;
        // Do not feed NaN/overflowing raw edits to port-specific native parsers.
        require(spin->GetTextValue().ToLongLong(&parsed, 10), "Spin edit is not currently a decimal integer");
        value = spin->GetValue();
    }
    require(value >= numeric_min(object) && value <= numeric_max(object), "Native numeric value is outside its range"); return value;
}
int selected_event_index(wxEvent& event) {
    if (auto* aui=dynamic_cast<wxAuiNotebookEvent*>(&event)) return aui->GetSelection();
    if (auto* book = dynamic_cast<wxBookCtrlEvent*>(&event)) return book->GetSelection();
    auto* command = dynamic_cast<wxCommandEvent*>(&event);
    return command && (event.GetEventType() == wxEVT_COMBOBOX || event.GetEventType() == wxEVT_RADIOBOX || event.GetEventType() == wxEVT_CHOICE || event.GetEventType() == wxEVT_LISTBOX || event.GetEventType() == wxEVT_LISTBOX_DCLICK) ? command->GetSelection() : -1;
}
void make_box(wxl_handle id, int orientation) {
    auto* parent = window(id);
    require(!dynamic_cast<wxToolBar*>(parent) && !dynamic_cast<wxStatusBar*>(parent) && !dynamic_cast<wxNotebook*>(parent) && !dynamic_cast<wxSplitterWindow*>(parent), "Lay out page/pane panels, not their native container");
    require(parent->GetSizer() == nullptr, "Window already has a sizer");
    parent->SetSizer(new wxBoxSizer(orientation));
}
void invoke_callback(uint64_t token, const wxl_event& event) {
    CallbackScope scope;
    dispatch(token, &event);
}
void require_dispatch_entry(bool structure = false) {
    require(!active_modal.get() && actual_modals.empty(), "Cannot enter structural dispatch during a modal dialog");
    require(notebook_operations.empty() && list_callback_operations.empty() && tree_operations.empty() && splitter_operations.empty() && toolbar_operations.empty() && accelerator_action_frames.empty() && input_operations.empty(), "Cannot enter structural dispatch during navigation callbacks");
    const bool safe_action = structure && ui_action_running;
    require(!ui_action_running || structure, "Nested RUN/pump/finish is forbidden in a deferred UI action");
    require(!running || safe_action, "Event loop is already running");
    require((!pumping || safe_action) && callback_depth == 0, "Cannot enter event dispatch from a pump or callback");
}
void emit_event(wxl_handle id, const wxl_event& event) {
    const auto it = handlers.find(id);
    if (it != handlers.end() && dispatch && !pending_destruction.count(id)) {
        const auto token = it->second;
        invoke_callback(token, event);
    }
}
wxEventTypeTag<wxEvent> native_event_type(int kind) {
    if (stc_event_kind(kind)) return wxEventTypeTag<wxEvent>(stc_event_type(kind));
    if (input_event_kind(kind)) return wxEventTypeTag<wxEvent>(input_event_type(kind));
    if (aui_event_kind(kind)) return wxEventTypeTag<wxEvent>(aui_event_type(kind));
    if (web_event_kind(kind)) return wxEventTypeTag<wxEvent>(web_event_types[kind-WXL_EVENT_WEB_MESSAGE]);
    switch (kind) {
        case WXL_EVENT_CLICK: return wxEventTypeTag<wxEvent>(wxEVT_BUTTON);
        case WXL_EVENT_TEXT_CHANGE: return wxEventTypeTag<wxEvent>(wxEVT_TEXT);
        case WXL_EVENT_TOGGLE: return wxEventTypeTag<wxEvent>(wxEVT_CHECKBOX);
        case WXL_EVENT_SELECTION: return wxEventTypeTag<wxEvent>(wxEVT_CHOICE);
        case WXL_EVENT_CLOSE: return wxEventTypeTag<wxEvent>(wxEVT_CLOSE_WINDOW);
        case WXL_EVENT_QUERY_END_SESSION: return wxEventTypeTag<wxEvent>(wxEVT_QUERY_END_SESSION);
        case WXL_EVENT_END_SESSION: return wxEventTypeTag<wxEvent>(wxEVT_END_SESSION);
        case WXL_EVENT_IDLE: return wxEventTypeTag<wxEvent>(wxEVT_IDLE);
        case WXL_EVENT_MENU: return wxEventTypeTag<wxEvent>(wxEVT_MENU);
        case WXL_EVENT_NOTEBOOK_CHANGING: return wxEventTypeTag<wxEvent>(wxEVT_NOTEBOOK_PAGE_CHANGING);
        case WXL_EVENT_NOTEBOOK_CHANGED: return wxEventTypeTag<wxEvent>(wxEVT_NOTEBOOK_PAGE_CHANGED);
        case WXL_EVENT_LISTBOX_SELECTION: return wxEventTypeTag<wxEvent>(wxEVT_LISTBOX);
        case WXL_EVENT_LISTBOX_DOUBLE_CLICK: return wxEventTypeTag<wxEvent>(wxEVT_LISTBOX_DCLICK);
        case WXL_EVENT_TREE_CHANGING: return wxEventTypeTag<wxEvent>(wxEVT_TREE_SEL_CHANGING);
        case WXL_EVENT_TREE_CHANGED: return wxEventTypeTag<wxEvent>(wxEVT_TREE_SEL_CHANGED);
        case WXL_EVENT_SPLITTER_CHANGING: return wxEventTypeTag<wxEvent>(wxEVT_SPLITTER_SASH_POS_CHANGING);
        case WXL_EVENT_SLIDER_CHANGE: return wxEventTypeTag<wxEvent>(wxEVT_SLIDER);
        case WXL_EVENT_SEARCH: return wxEventTypeTag<wxEvent>(wxEVT_SEARCH);
        case WXL_EVENT_SEARCH_CANCEL: return wxEventTypeTag<wxEvent>(wxEVT_SEARCH_CANCEL);
        case WXL_EVENT_TIME_CHANGE: return wxEventTypeTag<wxEvent>(wxEVT_TIME_CHANGED);
        case WXL_EVENT_DATE_CHANGE: return wxEventTypeTag<wxEvent>(wxEVT_DATE_CHANGED);
        case WXL_EVENT_COMBO_SELECTION: return wxEventTypeTag<wxEvent>(wxEVT_COMBOBOX);
        case WXL_EVENT_TOGGLE_BUTTON_CHANGE: return wxEventTypeTag<wxEvent>(wxEVT_TOGGLEBUTTON);
        case WXL_EVENT_RADIO_SELECTION: return wxEventTypeTag<wxEvent>(wxEVT_RADIOBOX);
        case WXL_EVENT_SPIN_CHANGE: return wxEventTypeTag<wxEvent>(wxEVT_SPINCTRL);
        case WXL_EVENT_SPLITTER_CHANGED: return wxEventTypeTag<wxEvent>(wxEVT_SPLITTER_SASH_POS_CHANGED);
        default: {
            const auto it = registered_events.find(kind);
            require(it != registered_events.end(), "Unsupported event kind");
            return wxEventTypeTag<wxEvent>(it->second.type);
        }
    }
}
int native_event_family(int kind) {
    if (stc_event_kind(kind)) return WXL_FAMILY_COMMAND;
    if (input_event_kind(kind)) return kind==70 ? WXL_FAMILY_COMMAND : WXL_FAMILY_BASE;
    if (aui_event_kind(kind)) return WXL_FAMILY_NOTIFY;
    if (web_event_kind(kind)) return WXL_FAMILY_COMMAND;
    if (kind >= WXL_EVENT_CLICK && kind <= WXL_EVENT_SELECTION) return WXL_FAMILY_COMMAND;
    if (kind >= WXL_EVENT_CLOSE && kind <= WXL_EVENT_END_SESSION) return WXL_FAMILY_CLOSE;
    if (kind == WXL_EVENT_IDLE) return WXL_FAMILY_IDLE;
    if (kind == WXL_EVENT_SEARCH || kind == WXL_EVENT_SEARCH_CANCEL || kind == WXL_EVENT_TIME_CHANGE || kind == WXL_EVENT_DATE_CHANGE || kind == WXL_EVENT_COMBO_SELECTION || kind == WXL_EVENT_TOGGLE_BUTTON_CHANGE || kind == WXL_EVENT_RADIO_SELECTION || kind == WXL_EVENT_SLIDER_CHANGE || kind == WXL_EVENT_MENU || kind == WXL_EVENT_LISTBOX_SELECTION || kind == WXL_EVENT_LISTBOX_DOUBLE_CLICK) return WXL_FAMILY_COMMAND;
    if (kind == WXL_EVENT_SPIN_CHANGE || kind == WXL_EVENT_NOTEBOOK_CHANGING || kind == WXL_EVENT_NOTEBOOK_CHANGED || kind == WXL_EVENT_TREE_CHANGING || kind == WXL_EVENT_TREE_CHANGED || kind == WXL_EVENT_SPLITTER_CHANGING || kind == WXL_EVENT_SPLITTER_CHANGED) return WXL_FAMILY_NOTIFY;
    const auto it = registered_events.find(kind);
    require(it != registered_events.end(), "Unsupported event kind");
    return it->second.family;
}
bool event_can_veto(wxEvent& event) {
    if (auto* close = dynamic_cast<wxCloseEvent*>(&event)) return close->CanVeto();
    return dynamic_cast<wxNotifyEvent*>(&event) != nullptr;
}
bool event_vetoed(wxEvent& event) {
    if (auto* close = dynamic_cast<wxCloseEvent*>(&event)) return close->GetVeto();
    if (auto* notify = dynamic_cast<wxNotifyEvent*>(&event)) return !notify->IsAllowed();
    return false;
}
std::unique_ptr<wxEvent> synthetic_event(wxEvtHandler* receiver, int kind, int event_id,
                                       const char* text, size_t length, int value,
                                       int can_veto, int vetoed, int logging_off) {
    require(!stc_event_kind(kind) && !input_event_kind(kind) && !aui_event_kind(kind) && !web_event_kind(kind) && kind != WXL_EVENT_SEARCH && kind != WXL_EVENT_SEARCH_CANCEL && kind != WXL_EVENT_TIME_CHANGE && kind != WXL_EVENT_DATE_CHANGE && kind != WXL_EVENT_COMBO_SELECTION && kind != WXL_EVENT_TOGGLE_BUTTON_CHANGE && kind != WXL_EVENT_RADIO_SELECTION && kind != WXL_EVENT_SLIDER_CHANGE && kind != WXL_EVENT_SPIN_CHANGE && kind != WXL_EVENT_NOTEBOOK_CHANGING && kind != WXL_EVENT_NOTEBOOK_CHANGED && kind != WXL_EVENT_TREE_CHANGING && kind != WXL_EVENT_TREE_CHANGED && kind != WXL_EVENT_SPLITTER_CHANGING && kind != WXL_EVENT_SPLITTER_CHANGED,
            "Derived navigation events require native interaction; synthetic construction is unsupported");
    const auto type = native_event_type(kind);
    const int family = native_event_family(kind);
    require(text || length == 0, "Null event text");
    require((kind != WXL_EVENT_SELECTION && kind != WXL_EVENT_LISTBOX_SELECTION && kind != WXL_EVENT_LISTBOX_DOUBLE_CLICK) || value >= -1, "Invalid selection event index");
    require(session_type(type) || logging_off == 0, "Logging-off applies only to session events");
    require(family == WXL_FAMILY_NOTIFY || vetoed == 0, "Initial veto applies only to notify types");
    std::unique_ptr<wxEvent> event;
    if (family == WXL_FAMILY_IDLE) {
        require(length == 0 && value == 0, "Idle events do not take command payloads");
        auto idle = std::make_unique<wxIdleEvent>();
        idle->SetId(event_id);
        event = std::move(idle);
    } else if (family == WXL_FAMILY_CLOSE) {
        require(length == 0 && value == 0, "Close events do not take command payloads");
        auto close = std::make_unique<wxCloseEvent>(type, event_id);
        close->SetCanVeto(can_veto != 0);
        if (session_type(type)) close->SetLoggingOff(logging_off != 0);
        event = std::move(close);
    } else if (family == WXL_FAMILY_NOTIFY) {
        auto notify = std::make_unique<wxNotifyEvent>(type, event_id);
        notify->SetString(wxString::FromUTF8(text ? text : "", length));
        notify->SetInt(value);
        if (vetoed) notify->Veto();
        event = std::move(notify);
    } else {
        auto command = std::make_unique<wxCommandEvent>(type, event_id);
        if (kind == WXL_EVENT_TEXT_CHANGE || kind == WXL_EVENT_LISTBOX_SELECTION || kind == WXL_EVENT_LISTBOX_DOUBLE_CLICK || kind >= first_registered_event_kind)
            command->SetString(wxString::FromUTF8(text ? text : "", length));
        command->SetInt(value);
        if (kind == WXL_EVENT_LISTBOX_SELECTION || kind == WXL_EVENT_LISTBOX_DOUBLE_CLICK) command->SetExtraLong(value >= 0 ? 1 : 0);
        event = std::move(command);
    }
    event->SetEventObject(receiver);
    return event;
}
int kind_of(wxEvent& event) {
    if (const auto kind=stc_kind_for_type(event.GetEventType())) return kind;
    if (const auto kind=input_kind_for_type(event.GetEventType())) return kind;
    if (const auto kind=aui_kind_for_type(event.GetEventType())) return kind;
    for (int kind = WXL_EVENT_CLICK; kind <= WXL_EVENT_WEB_ERROR; ++kind)
        if (native_event_type(kind) == event.GetEventType()) return kind;
    for (const auto& item : registered_events)
        if (item.second.type == event.GetEventType()) return item.first;
    throw std::runtime_error("Unsupported native event type");
}
wxEvent& owned_event(wxl_handle id) {
    ready();
    const auto found = owned_events.find(id);
    require(found != owned_events.end(), "Invalid or destroyed owned-event handle");
    return *found->second;
}
std::unique_ptr<wxEvent> detached_clone(wxEvent& event) {
    require(typeid(event) == typeid(wxCommandEvent) || typeid(event) == typeid(wxNotifyEvent) ||
            typeid(event) == typeid(wxCloseEvent) || typeid(event) == typeid(wxIdleEvent), "Cloning this native event class is not supported");
    const auto kind = kind_of(event);
    require(!stc_event_kind(kind) && !input_event_kind(kind) && !aui_event_kind(kind) && !web_event_kind(kind) && kind != WXL_EVENT_SEARCH && kind != WXL_EVENT_SEARCH_CANCEL && kind != WXL_EVENT_TIME_CHANGE && kind != WXL_EVENT_DATE_CHANGE && kind != WXL_EVENT_COMBO_SELECTION && kind != WXL_EVENT_TOGGLE_BUTTON_CHANGE && kind != WXL_EVENT_RADIO_SELECTION && kind != WXL_EVENT_SLIDER_CHANGE && kind != WXL_EVENT_SPIN_CHANGE, "Bounded input control events cannot be detached/cloned");
    require(event.GetEventUserData() == nullptr, "Cannot clone borrowed event userData");
    if (auto* command = dynamic_cast<wxCommandEvent*>(&event))
        require(!command->GetClientData() && !command->GetClientObject(), "Cannot clone borrowed command client data");
    // Freeze effective text before detachment: wxEVT_TEXT can lazily read its
    // wxTextEntry origin instead of the stored m_cmdString.
    auto* command = dynamic_cast<wxCommandEvent*>(&event);
    const wxString text = command ? command->GetString() : wxString();
    std::unique_ptr<wxEvent> result(event.Clone());
    require(result != nullptr, "Native event clone failed");
    if (command) static_cast<wxCommandEvent*>(result.get())->SetString(text);
    result->SetEventObject(nullptr);
    return result;
}
int64_t read_event_field(wxEvent& event, int field) {
    auto* close = dynamic_cast<wxCloseEvent*>(&event);
    auto* command = dynamic_cast<wxCommandEvent*>(&event);
    switch (field) {
        case 0: return kind_of(event);
        case 1: return event.GetId();
        case 2: return event.GetTimestamp();
        case 3: return event.GetEventType();
        case 4: return event_can_veto(event);
        case 5: require(close || dynamic_cast<wxNotifyEvent*>(&event), "This event family has no veto state"); return event_vetoed(event);
        case 6: require(close && session_type(event.GetEventType()), "Logging-off requires a session event"); return close->GetLoggingOff();
        case 7: return command ? command->GetInt() : 0;
        case 8: return event.GetEventCategory();
        case 9: return event.IsCommandEvent();
        case 10: return event.GetSkipped();
        case 11: return event.ShouldPropagate();
        case 12: return native_event_family(kind_of(event));
        case 14: return command ? command->GetExtraLong() : 0;
        case 15: return command && (event.GetEventType() == wxEVT_TOGGLEBUTTON || event.GetEventType() == wxEVT_CHECKBOX || event.GetEventType() == wxEVT_MENU) && command->IsChecked();
        case 16: return selected_event_index(event);
        case 17: return command && command->IsSelection();
        case 18: { auto* idle = dynamic_cast<wxIdleEvent*>(&event); return idle && idle->MoreRequested(); }
        case 19:
            require(event.GetEventType()==wxEVT_DATE_CHANGED && dynamic_cast<wxDateEvent*>(&event), "Date metadata requires a native date-change context");
            return event_calendar_code(event);
        case 20:
            require(event.GetEventType()==wxEVT_TIME_CHANGED && dynamic_cast<wxDateEvent*>(&event), "Time metadata requires a native time-change context");
            return event_clock_code(event);
        default: throw std::runtime_error("Unknown event field");
    }
}
void write_event_field(wxEvent& event, int field, int64_t value) {
    require(!stc_payload_readonly(event.GetEventType()) && !input_payload_readonly(event.GetEventType()) && !aui_payload_readonly(event.GetEventType()) && !web_event_type(event.GetEventType()), "Web event payload metadata is read-only");
    auto* close = dynamic_cast<wxCloseEvent*>(&event);
    auto* notify = dynamic_cast<wxNotifyEvent*>(&event);
    auto* command = dynamic_cast<wxCommandEvent*>(&event);
    if (field == 1 || field == 3 || field == 7 || field == 11)
        require(value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max(), "Event integer outside signed int range");
    switch (field) {
        case 18: {
            auto* idle = dynamic_cast<wxIdleEvent*>(&event);
            require(idle != nullptr, "RequestMore requires an idle event");
            idle->RequestMore(value != 0); break;
        }
        case 1: event.SetId(static_cast<int>(value)); break;
        case 2:
            require(value >= std::numeric_limits<long>::min() && value <= std::numeric_limits<long>::max(), "Timestamp outside native LONG range");
            event.SetTimestamp(static_cast<long>(value)); break;
        case 3: set_registered_type(event, static_cast<int>(value)); break;
        case 4: require(close != nullptr, "CanVeto mutation requires a close-family event"); close->SetCanVeto(value != 0); break;
        case 5:
            if (close) { require(close->CanVeto(), "This close event cannot be vetoed"); close->Veto(value != 0); }
            else if (notify) { if (value) notify->Veto(); else notify->Allow(); }
            else throw std::runtime_error("This event family has no veto state");
            break;
        case 6: require(close && session_type(event.GetEventType()), "Logging-off requires a session event"); close->SetLoggingOff(value != 0); break;
        case 7: require(command != nullptr, "Integer payload requires a command/notify event"); command->SetInt(static_cast<int>(value)); break;
        case 10: event.Skip(value != 0); break;
        case 11: require(value >= 0, "Propagation level must be nonnegative"); event.ResumePropagation(static_cast<int>(value)); break;
        case 13:
            require(value == 0, "Native event origin supports clearing only");
            if (event.GetEventObject()) {
                const RegisteredEvent* registered = nullptr;
                for (const auto& item : registered_events)
                    if (item.second.type == event.GetEventType()) { registered = &item.second; break; }
                require(registered && ((registered->family == WXL_FAMILY_COMMAND && command && !notify) ||
                                       (registered->family == WXL_FAMILY_NOTIFY && notify)),
                        "Clearing a non-null origin requires a compatible registered command/notify tag");
                event.SetEventObject(nullptr);
            }
            break;
        case 14:
            require(command != nullptr, "Extra-long payload requires a command/notify event");
            require(value >= std::numeric_limits<long>::min() && value <= std::numeric_limits<long>::max(),
                    "Extra-long payload outside native LONG range");
            command->SetExtraLong(static_cast<long>(value)); break;
        case 15:
            require(command && (event.GetEventType() == wxEVT_CHECKBOX || event.GetEventType() == wxEVT_MENU), "Checked metadata requires a toggle/menu event");
            command->SetInt(value != 0 ? 1 : 0); break;
        case 16:
            require(command && (event.GetEventType() == wxEVT_CHOICE || event.GetEventType() == wxEVT_LISTBOX || event.GetEventType() == wxEVT_LISTBOX_DCLICK), "Selection metadata requires a choice/listbox selection event");
            require(value >= -1 && value <= std::numeric_limits<int>::max(), "Selection index outside supported range");
            command->SetInt(static_cast<int>(value)); break;
        default: throw std::runtime_error("This event field is read-only or unknown");
    }
}
// wxEvtHandler owns this adapter through its dynamic binding entry.
class BindingUserData final : public wxObject {
public:
    explicit BindingUserData(uint64_t value) : token(value) {}
    const uint64_t token;
};
struct Binding {
    wxWeakRef<wxEvtHandler> receiver;
    wxl_handle source;
    uint64_t token;
    int kind, id, last_id;
    bool with_context;
    BindingUserData* user_data = nullptr; // Comparison only; native binding owns it.
    bool live() const {
        auto* object = receiver.get();
        auto* widget = dynamic_cast<wxWindow*>(object);
        return object && !pending_destruction.count(source) && (!widget || !window_destruction_pending(widget));
    }
    void on_event(wxEvent& event) {
        StcCallbackScope stc_scope(event);
        NotebookCallbackOperation notebook_guard(event);
        ListCallbackOperation list_guard(event);
        TreeCallbackOperation tree_guard(event);
        SplitterCallbackOperation splitter_guard(event);
        ToolbarCallbackOperation toolbar_guard(event);
        AcceleratorCallbackOperation accelerator_guard(event);
        InputCallbackOperation numeric_guard(event);
        if (!notebook_guard.allow || !list_guard.allow || !tree_guard.allow || !splitter_guard.allow || !toolbar_guard.allow || !numeric_guard.allow) { event.Skip(); return; }
        const auto found = bindings.find(token);
        if (found == bindings.end()) { event.Skip(); return; }
        // Keep the method receiver alive if this or another binding is removed.
        auto keep = found->second;
        if (live() && dispatch) {
            auto* command = dynamic_cast<wxCommandEvent*>(&event);
            int snapshot_kind = kind;
            for (const auto& item : registered_events)
                if (item.second.type == event.GetEventType()) { snapshot_kind = item.first; break; }
            const int family = native_event_family(snapshot_kind);
            const bool carries_text = stc_event_kind(snapshot_kind) || web_event_kind(snapshot_kind) || snapshot_kind == WXL_EVENT_SEARCH || snapshot_kind == WXL_EVENT_SEARCH_CANCEL || snapshot_kind == WXL_EVENT_COMBO_SELECTION || snapshot_kind == WXL_EVENT_TOGGLE_BUTTON_CHANGE || snapshot_kind == WXL_EVENT_RADIO_SELECTION || snapshot_kind == WXL_EVENT_TEXT_CHANGE || snapshot_kind == WXL_EVENT_LISTBOX_SELECTION || snapshot_kind == WXL_EVENT_LISTBOX_DOUBLE_CLICK || snapshot_kind >= first_registered_event_kind;
            const auto text = command ? command->GetString() : wxString();
            const auto bytes = text.ToUTF8();
            wxl_event snapshot{snapshot_kind, carries_text ? (bytes.data() ? bytes.data() : "") : nullptr,
                                     (snapshot_kind == WXL_EVENT_TOGGLE_BUTTON_CHANGE || snapshot_kind == WXL_EVENT_TOGGLE || snapshot_kind == WXL_EVENT_MENU) && command && command->IsChecked() ? 1 : 0,
                                     selected_event_index(event),
                                     event.GetId(), carries_text ? bytes.length() : 0,
                                     0, event.GetEventType(), event_can_veto(event) ? 1 : 0, event_vetoed(event) ? 1 : 0,
                                     family, command ? command->GetInt() : 0,
                                     session_type(event.GetEventType()) ? (static_cast<wxCloseEvent&>(event).GetLoggingOff() ? 1 : 0) : 0,
                                     event.GetTimestamp(), event.IsCommandEvent() ? 1 : 0, origin_id(event), static_cast<int>(event.GetEventCategory()),
                                     command ? command->GetExtraLong() : 0, command && command->IsSelection() ? 1 : 0,
                                     user_data && event.GetEventUserData() == user_data ? token : 0,
                                     dynamic_cast<wxIdleEvent*>(&event) && static_cast<wxIdleEvent&>(event).MoreRequested() ? 1 : 0,
                                     event_tree_token(event), event_sash_value(event)};
            snapshot.date_code = event_calendar_code(event);
            snapshot.time_code = event_clock_code(event);
            fill_stc_detail(event,snapshot); fill_input_detail(event,snapshot); fill_aui_detail(event,snapshot);
            if (with_context) {
                event.Skip(false);
                EventScope scope(event, snapshot.user_data);
                snapshot.context = scope.token;
                invoke_callback(token, snapshot);
                return;
            }
            invoke_callback(token, snapshot);
        }
        // Observer bindings continue dispatch; contextual bindings decide explicitly.
        event.Skip();
    }
};
bool remove_binding(uint64_t token) {
    const auto found = bindings.find(token);
    if (found == bindings.end()) return false;
    auto binding = found->second;
    if (auto* receiver = binding->receiver.get()) {
        require(receiver->Unbind(native_event_type(binding->kind), &Binding::on_event, binding.get(),
                                 binding->id, binding->last_id, binding->user_data), "Native event binding not found");
    }
    bindings.erase(token);
    return true;
}
wxl_handle track(wxWindow* object) {
    const auto id = next_handle++;
    const auto previous=managed_window_states.find(object);
    if (previous!=managed_window_states.end() && !previous->second.lifetime.get()) input_owners.erase(object);
    windows.emplace(id, object);
    managed_window_states[object]=ManagedWindowState{object,false};
    object->Bind(wxEVT_DESTROY, [id, object](wxWindowDestroyEvent& event) {
        if (event.GetEventObject() == object) {
            // Keep the weak retirement marker until final base teardown, even
            // after DESTROY invalidates the public identity.
            if (auto state=managed_window_states.find(object);state!=managed_window_states.end()) state->second.retiring=true;
            input_owners.erase(object);
            windows.erase(id);
            handlers.erase(id);
        }
        event.Skip();
    });
    return id;
}
template<class F> int status(F fn) noexcept {
    struct NativeEntry { NativeEntry() { ++native_entry_depth; } ~NativeEntry() { --native_entry_depth; } } entry;
    error.clear();
    try { fn(); return 0; }
    catch (const std::exception& e) { error = e.what(); }
    catch (...) { error = "Unknown native exception"; }
    return -1;
}
// Boolean operations need a distinct error sentinel: zero is a valid false.
template<class F> int boolean_result(F fn) noexcept {
    bool value = false;
    const int result = status([&] { value = fn(); });
    return result == 0 ? (value ? 1 : 0) : -1;
}
// Keep the wxString alive while copying its possibly borrowed UTF-8 storage.
// Explicit byte counts preserve embedded NULs in window metadata.
template<class F> const char* metadata_string(F fn, size_t* length) noexcept {
    const int result = status([&] {
        require(length != nullptr, "Metadata length output must not be null");
        *length = 0;
        const auto text = fn();
        const auto bytes = text.ToUTF8();
        metadata_result.assign(bytes.data() ? bytes.data() : "", bytes.length());
        *length = metadata_result.size();
    });
    return result == 0 ? metadata_result.c_str() : nullptr;
}
template<class F> wxl_handle create(F fn) noexcept {
    wxl_handle result = 0;
    status([&] { ready(); result = fn(); });
    return result;
}
wxString utf8(const char* value) {
    require(value != nullptr, "Null UTF-8 string");
    return wxString::FromUTF8(value);
}
bool protected_modal_ancestor(wxWindow* object) {
    for (wxWindow* node = active_modal.get(); node; node = node->GetParent()) if (node == object) return true;
    for (const auto& dialog : actual_modals)
        for (wxWindow* node = dialog.get(); node; node = node->GetParent()) if (node == object) return true;
    for (const auto& operation : dialog_operation_windows)
        for (wxWindow* node = operation.get(); node; node = node->GetParent()) if (node == object) return true;
    return false;
}
// File-control setters/creation can synchronously open validation prompts even
// outside public ShowModal. Keep their receiver/parent alive across those loops.
struct DialogOperationScope {
    bool protects;
    explicit DialogOperationScope(wxWindow* object) : protects(object != nullptr) {
        if (protects) dialog_operation_windows.emplace_back(object);
    }
    ~DialogOperationScope() { if (protects) dialog_operation_windows.pop_back(); }
};
bool request_modal_cancel();
class ModalCloseGuard {
    wxDialog* protected_dialog;
    std::vector<wxWindow*> ancestors;
    size_t bound = 0;
    void on_close(wxCloseEvent& event) {
        if (event.GetEventObject() == protected_dialog && protected_dialog && protected_dialog->IsModal()) {
            if (protected_dialog == active_modal.get()) request_modal_cancel();
            else protected_dialog->EndModal(wxID_CANCEL);
        }
        event.Skip(false);
    }
public:
    explicit ModalCloseGuard(wxWindow* object) : protected_dialog(dynamic_cast<wxDialog*>(object)) {
        for (auto* node = object; node; node = node->GetParent()) {
            require(!window_destruction_pending(node), "Dialog ancestor destruction is pending");
            ancestors.push_back(node);
        }
        try {
            for (auto* node : ancestors) { node->Bind(wxEVT_CLOSE_WINDOW, &ModalCloseGuard::on_close, this); ++bound; }
        } catch (...) { detach(); throw; }
    }
    void detach() noexcept {
        while (bound) ancestors[--bound]->Unbind(wxEVT_CLOSE_WINDOW, &ModalCloseGuard::on_close, this);
    }
    ~ModalCloseGuard() { detach(); }
};
// Generic message controls are created eagerly, so parent/title/lifetime exist
// in Lisp AFTER hooks. Bypass the generic lazy ShowModal recreation path.
class PreparedMessageDialog : public wxGenericMessageDialog {
public:
    PreparedMessageDialog(wxWindow* parent, const wxString& message, const wxString& title, long style)
        : wxGenericMessageDialog(parent, message, title, style) { DoCreateMsgdialog(); }
    int ShowModal() override { return wxMessageDialogBase::ShowModal(); }
};
void cancel_innermost_modal() {
    wxDialog* object = actual_modals.empty() ? active_modal.get() : actual_modals.back().get();
    if (object && object->IsModal()) object->EndModal(wxID_CANCEL);
    if (!object) return;
    const auto epoch = modal_workflow_epoch;
    wxWeakRef<wxDialog> weak(object);
    wxTheApp->CallAfter([weak, epoch] {
        if (epoch != modal_workflow_epoch || !modal_cancel_requested) return;
        if (auto* live = weak.get(); live &&
            (actual_modals.empty() ? active_modal.get() == live : actual_modals.back().get() == live))
            live->EndModal(wxID_CANCEL); // Also handles cancellation during Show before m_eventLoop exists.
    });
}
bool request_modal_cancel() {
    if (!active_modal.get() && actual_modals.empty()) return false;
    modal_cancel_requested = true;
    cancel_innermost_modal();
    return true;
}
class WorkflowModalHook final : public wxModalDialogHook {
    struct Level { unsigned references; std::unique_ptr<ModalCloseGuard> close_guard; };
    std::vector<Level> levels;
protected:
    int Enter(wxDialog* object) override {
        // Lisp may defer rethrow until its nearest notifying operation returns.
        // Do not open a new native prompt while its callback error is pending.
        if (pending_callback_error || !notebook_operations.empty() || !list_callback_operations.empty() || !tree_operations.empty() || !splitter_operations.empty() || !toolbar_operations.empty() || !accelerator_action_frames.empty() || !input_operations.empty()) return wxID_CANCEL;
        if (modal_cancel_requested && (active_modal.get() || !actual_modals.empty())) return wxID_CANCEL;
        // Native NSAlert modal loops cannot be aborted using wxDialog::EndModal.
        // Replace them before their platform ShowModal implementation is entered.
        if (auto* message = dynamic_cast<wxMessageDialogBase*>(object);
            message && !dynamic_cast<wxGenericMessageDialog*>(object)) {
            wxGenericMessageDialog replacement(message->GetParent(), message->GetMessage(), message->GetCaption(), message->GetMessageDialogStyle());
            replacement.SetExtendedMessage(message->GetExtendedMessage());
            replacement.SetYesNoLabels(message->GetYesLabel(), message->GetNoLabel());
            replacement.SetOKCancelLabels(message->GetOKLabel(), message->GetCancelLabel());
            replacement.SetHelpLabel(message->GetHelpLabel());
            return replacement.ShowModal();
        }
        if (!actual_modals.empty() && actual_modals.back().get() == object) {
            ++levels.back().references;
            return wxID_NONE;
        }
        if (actual_modals.empty() && !active_modal.get()) { ++modal_workflow_epoch; modal_cancel_requested = false; }
        auto guard = std::make_unique<ModalCloseGuard>(object);
        levels.push_back({1, std::move(guard)});
        try { actual_modals.emplace_back(object); }
        catch (...) { levels.pop_back(); throw; }
        ++callback_depth; // Suspend an outer borrowed context throughout this loop.
        return wxID_NONE;
    }
    void Exit(wxDialog* object) override {
        if (actual_modals.empty() || actual_modals.back().get() != object) return;
        if (--levels.back().references) return;
        actual_modals.pop_back();
        levels.pop_back();
        --callback_depth;
        // End only one loop at a time. Ending parent and child simultaneously
        // can leave wxModalEventLoop running indefinitely on the Cocoa backend.
        if (modal_cancel_requested && !actual_modals.empty()) cancel_innermost_modal();
        if (actual_modals.empty() && !active_modal.get()) { modal_cancel_requested = false; ++modal_workflow_epoch; }
    }
};
std::unique_ptr<WorkflowModalHook> workflow_modal_hook;
void validate_dialog_result(wxDialog* object, int code) {
    require(code >= 0 && code <= 3, "Invalid modal result");
    if (code == 1) return; // Cancellation always remains available to unwind safely.
    const auto found = dialog_message_buttons.find(object);
    if (found == dialog_message_buttons.end()) { require(code == 0, "This dialog supports only OK or Cancel"); return; }
    require(code == 0 ? found->second <= 1 : found->second >= 2, "Result button is not offered by this message dialog");
}
int bridge_dialog_result(int code) {
    if (code == wxID_OK) return 0;
    if (code == wxID_CANCEL) return 1;
    if (code == wxID_YES) return 2;
    if (code == wxID_NO) return 3;
    throw std::runtime_error("Unsupported modal result");
}
int native_dialog_result(int code) { return code == 0 ? wxID_OK : code == 1 ? wxID_CANCEL : code == 2 ? wxID_YES : wxID_NO; }
wxString text_value(const char* value, size_t length) {
    require(value || length == 0, "Null text value");
    require(!length || !std::memchr(value, 0, length), "Text cannot contain NUL");
    const auto text = wxString::FromUTF8(value ? value : "", length);
    require(!length || !text.empty(), "Invalid UTF-8 text");
    return text;
}
void text_range(wxTextCtrl* control, int64_t from, int64_t to) {
    require(from >= 0 && to >= from && to <= control->GetLastPosition(), "Text range is out of bounds");
}
wxString menu_string(const char* text, size_t length, bool allow_empty) {
    require(text || length == 0, "Null menu text");
    require(allow_empty || length != 0, "Menu label must be nonempty");
    require(!length || !std::memchr(text, 0, length), "Menu text cannot contain NUL");
    const auto value = wxString::FromUTF8(text ? text : "", length);
    require(!length || !value.empty(), "Invalid UTF-8 menu text");
    return value;
}
MenuResource menu_resource(wxl_handle id, int kind = -1) {
    ready();
    const auto found = menu_resources.find(id);
    require(found != menu_resources.end(), "Invalid or destroyed menu resource");
    window(found->second.frame);
    require(kind < 0 || found->second.kind == kind, "Wrong menu resource type");
    return found->second;
}
size_t menu_position(wxMenuBar* bar, wxMenu* menu) {
    for (size_t i = 0; i < bar->GetMenuCount(); ++i) if (bar->GetMenu(i) == menu) return i;
    throw std::runtime_error("Menu is not attached to its managed bar");
}

#if wxUSE_CLIPBOARD
#ifdef WXLAMBDA_NATIVE_TESTS
// Native regression executable only: never exported or compiled into the bridge.
wxClipboardBase* test_clipboard = nullptr;
#endif
// MSW/OLE keeps caller ownership on failure despite the broad header contract.
bool transfer_clipboard_data(wxClipboardBase& clipboard, std::unique_ptr<wxDataObject> data,
                             bool owns_failed_transfer) {
    if (owns_failed_transfer) return clipboard.SetData(data.release());
    const bool success = clipboard.SetData(data.get());
    if (success) data.release();
    return success;
}
bool clipboard_busy = false;
class ClipboardSession {
    wxClipboardBase* clipboard;
    bool primary;
    CallbackScope borrowed_context_suspension;
public:
    ClipboardSession() {
        ready();
        require(notebook_operations.empty() && list_callback_operations.empty() && tree_operations.empty() && splitter_operations.empty() && toolbar_operations.empty() && accelerator_action_frames.empty() && input_operations.empty(), "Defer clipboard operations until navigation callbacks return");
        require(!clipboard_busy, "Clipboard operation is already in progress");
#ifdef WXLAMBDA_NATIVE_TESTS
        clipboard = test_clipboard ? test_clipboard : wxTheClipboard;
#else
        clipboard = wxTheClipboard;
#endif
        require(clipboard != nullptr, "Clipboard is unavailable");
        require(!clipboard->IsOpened(), "Clipboard is already open");
        primary = clipboard->IsUsingPrimarySelection();
        clipboard_busy = true;
        try {
            clipboard->UsePrimarySelection(false);
            require(clipboard->Open(), "Unable to open clipboard");
        } catch (...) {
            clipboard->UsePrimarySelection(primary);
            clipboard_busy = false;
            throw;
        }
    }
    ~ClipboardSession() {
        clipboard->Close();
        clipboard->UsePrimarySelection(primary);
        clipboard_busy = false;
    }
    wxClipboardBase* operator->() { return clipboard; }
};
#endif

bool ui_action_owner_live(const UiAction& action) {
    if (!action.owner_id) return true;
    auto* owner = action.owner.get();
    return owner && !window_destruction_pending(owner);
}
bool ui_action_ready() {
    return started && !finishing && ui_dispatch && !ui_action_running && !pending_callback_error &&
           callback_depth == 0 && timer_notify_depth == 0 && input_creation_depth == 0 && EventScope::stack.empty() &&
           ui_driver_depth != 0 && native_entry_depth == ui_driver_depth && idle_processing_depth == 1 &&
           !active_modal.get() && actual_modals.empty() && dialog_operation_windows.empty() && !clipboard_busy &&
           notebook_operations.empty() && list_callback_operations.empty() && tree_operations.empty() &&
           splitter_operations.empty() && toolbar_operations.empty() && accelerator_action_frames.empty() && input_operations.empty();
}
bool ui_timer_owner_live(const UiTimer& timer) {
    return !timer.retired && (!timer.owner_id || (timer.owner.get() && !window_destruction_pending(timer.owner.get())));
}
bool ui_work_pending() {
    if (!ui_actions.empty()) return true;
    for (const auto& entry : ui_timers) if (entry.second->due || !ui_timer_owner_live(*entry.second)) return true;
    return false;
}
bool process_deferred_ui_action() {
    if (!ui_action_ready()) return false;
    uint64_t timer_token = 0;
    auto sequence = ui_actions.empty() ? std::numeric_limits<uint64_t>::max() : ui_actions.front().sequence;
    for (const auto& entry : ui_timers) {
        const auto& timer = *entry.second;
        if (!ui_timer_owner_live(timer)) { timer_token = entry.first; break; }
        if (timer.due && timer.sequence < sequence) { sequence = timer.sequence; timer_token = entry.first; }
    }
    uint64_t token = 0; int kind = 0;
    if (timer_token) {
        // Scope all native references before invoking Lisp. Canceling this or
        // another timer in its function can delete native timer objects safely.
        const auto found = ui_timers.find(timer_token);
        token = timer_token;
        if (!ui_timer_owner_live(*found->second)) ui_timers.erase(found);
        else if (found->second->one_shot) { kind = 3; ui_timers.erase(found); }
        else { kind = 2; found->second->due = false; }
    } else if (!ui_actions.empty()) {
        const auto action = ui_actions.front(); ui_actions.pop_front();
        token = action.token; kind = ui_action_owner_live(action) ? 1 : 0;
    } else return false;
    struct ActionScope {
        explicit ActionScope(uint64_t token) { ui_action_running = true; active_ui_action_token = token; }
        ~ActionScope() { ui_action_running = false; active_ui_action_token = 0; }
    };
    { ActionScope scope(token); ui_dispatch(token, kind); }
    return ui_work_pending() && ui_action_ready();
}

}
extern "C" {
int wxl_abi_version() { return 57; }
const char* wxl_last_error() { return error.c_str(); }
int wxl_clipboard_text_available() {
    return boolean_result([&] {
        ready();
#if wxUSE_CLIPBOARD
        ClipboardSession clipboard;
        return clipboard->IsSupported(wxDF_UNICODETEXT) || clipboard->IsSupported(wxDF_TEXT);
#else
        throw std::runtime_error("Clipboard support is unavailable in this wxWidgets build");
        return false;
#endif
    });
}
const char* wxl_clipboard_text(size_t* length, int* available) {
    return metadata_string([&]() -> wxString {
        ready();
        require(available != nullptr, "Clipboard availability output must not be null");
        *available = 0;
#if wxUSE_CLIPBOARD
        ClipboardSession clipboard;
        if (!clipboard->IsSupported(wxDF_UNICODETEXT) && !clipboard->IsSupported(wxDF_TEXT)) return {};
        wxTextDataObject data;
        require(clipboard->GetData(data), "Unable to retrieve clipboard text");
        const auto text = data.GetText();
        require(text.find(wxUniChar(0)) == wxString::npos, "Clipboard text cannot contain NUL");
        *available = 1;
        return text;
#else
        throw std::runtime_error("Clipboard support is unavailable in this wxWidgets build");
#endif
    }, length);
}
int wxl_set_clipboard_text(const char* value, size_t length) {
    return status([&] {
        ready();
        require(value || length == 0, "Null clipboard text");
        require(!length || !std::memchr(value, 0, length), "Clipboard text cannot contain NUL");
        const auto text = wxString::FromUTF8(value ? value : "", length);
        require(!length || !text.empty(), "Invalid UTF-8 clipboard text");
#if wxUSE_CLIPBOARD
        ClipboardSession clipboard;
#if defined(__WXMSW__) && wxUSE_OLE_CLIPBOARD
        constexpr bool owns_failed_transfer = false;
#else
        constexpr bool owns_failed_transfer = true;
#endif
        require(transfer_clipboard_data(*clipboard.operator->(), std::make_unique<wxTextDataObject>(text),
                                        owns_failed_transfer), "Unable to set clipboard text");
#else
        throw std::runtime_error("Clipboard support is unavailable in this wxWidgets build");
#endif
    });
}
int wxl_flush_clipboard() {
    return boolean_result([&] {
        ready();
#if wxUSE_CLIPBOARD
        ClipboardSession clipboard;
        return clipboard->Flush();
#else
        throw std::runtime_error("Clipboard support is unavailable in this wxWidgets build");
        return false;
#endif
    });
}
int wxl_start(wxl_callback callback, wxl_ui_callback ui_callback) {
    return status([&] {
        main_thread();
        require(!used, "Only one application session is supported per process");
        require(wxApp::GetInstance() == nullptr, "A wxWidgets application already exists");
        require(callback != nullptr && ui_callback != nullptr, "Missing event/UI action dispatcher");
        pending_callback_error = false;
        initial_idle_mode = wxIdleEvent::GetMode();
        used = true;
        wxApp::SetInstance(new App);
        static char name[] = "wxlambda";
        static char* argv[] = {name, nullptr};
        int argc = 1;
        if (!wxEntryStart(argc, argv)) {
            throw std::runtime_error("wxEntryStart failed");
        }
        if (!wxTheApp->CallOnInit()) {
            wxEntryCleanup();
            throw std::runtime_error("wxWidgets application initialization failed");
        }
        dispatch = callback; ui_dispatch = ui_callback; finishing = false;
        application_handle = next_handle++;
        started = true;
        workflow_modal_hook = std::make_unique<WorkflowModalHook>();
        workflow_modal_hook->Register();
    });
}
int wxl_defer_ui_action(uint64_t token, wxl_handle owner_id) { return status([&] {
    ready(); require(!finishing && ui_dispatch, "UI action queue is shutting down");
    require(token && token != active_ui_action_token && !ui_timers.count(token), "Invalid/active UI action token");
    for (const auto& action : ui_actions) require(action.token != token, "Duplicate UI action token");
    require(ui_actions.size() < 1024, "At most 1024 pending UI actions are supported");
    auto* owner = owner_id ? window(owner_id) : nullptr;
    wxWakeUpIdle(); // Wake is non-inline; failed allocation below publishes no queue entry.
    ui_actions.push_back({token, owner_id, owner, ++ui_work_sequence});
}); }
int wxl_cancel_ui_action(uint64_t token) { return boolean_result([&] {
    ready(); for (auto it = ui_actions.begin(); it != ui_actions.end(); ++it)
        if (it->token == token) { ui_actions.erase(it); return true; }
    return false;
}); }
int wxl_ui_action_pending(uint64_t token) { return boolean_result([&] {
    ready(); for (auto it = ui_actions.begin(); it != ui_actions.end(); ++it)
        if (it->token == token) {
            if (!ui_action_owner_live(*it)) { ui_actions.erase(it); return false; }
            return true;
        }
    return false;
}); }
int wxl_start_ui_timer(uint64_t token, int milliseconds, int one_shot, wxl_handle owner_id) { return status([&] {
    ready(); require(!finishing && ui_dispatch, "UI timers are shutting down");
#if defined(__WXOSX__) && !(wxMAJOR_VERSION == 3 && wxMINOR_VERSION == 2 && wxRELEASE_NUMBER == 12)
    // Audited cancellation fix is present in 3.2.12, but absent from some newer
    // branches (including 3.3.0/3.3.1). Do not infer safety from numeric ordering.
    require(false, "Safe OSX UI timers are currently verified only with wxWidgets 3.2.12");
#endif
    require(token && token != active_ui_action_token && !ui_timers.count(token), "Invalid/duplicate UI timer token");
    for (const auto& action : ui_actions) require(action.token != token, "UI timer token is already queued");
    require(milliseconds >= 1 && milliseconds <= 86400000 && (one_shot == 0 || one_shot == 1), "Invalid UI timer interval/mode");
    require(ui_timers.size() < 256, "At most 256 active UI timers are supported");
    auto timer = std::make_unique<UiTimer>(token, owner_id, owner_id ? window(owner_id) : nullptr, milliseconds, one_shot != 0);
    // All native timers are continuous; the bridge stops one-shots only after
    // their lower-bound interval has elapsed, so an early stale native tick
    // cannot stop/discard a new one-shot before its valid first notification.
    require(timer->Start(milliseconds, false), "Native UI timer start failed");
    ui_timers.emplace(token, std::move(timer));
}); }
int wxl_cancel_ui_timer(uint64_t token) { return boolean_result([&] {
    ready(); return ui_timers.erase(token) != 0;
}); }
int wxl_ui_timer_active(uint64_t token) { return boolean_result([&] {
    ready(); const auto found = ui_timers.find(token);
    if (found == ui_timers.end()) return false;
    if (!ui_timer_owner_live(*found->second)) { ui_timers.erase(found); return false; }
    return true; // Includes a stopped one-shot awaiting safe delivery.
}); }
int wxl_run() {
    return status([] {
        ready();
        require_dispatch_entry();
        require(!wxTopLevelWindows.empty(), "Create a frame before running the event loop");
        UiDriverScope driver;
        running = true;
        try { wxTheApp->OnRun(); }
        catch (...) { running = false; throw; }
        running = false;
    });
}
int wxl_poll() {
    return status([] {
        ready();
        require_dispatch_entry();
        UiDriverScope driver;
        PumpScope scope;
        wxTheApp->Yield(true);
        wxTheApp->ProcessIdle();
    });
}
int wxl_main_loop_running() {
    return boolean_result([] { ready(); return wxApp::IsMainLoopRunning(); });
}
int wxl_pending_events() {
    return boolean_result([] { ready(); return wxTheApp->HasPendingEvents(); });
}
int wxl_process_pending_events() {
    return status([] {
        ready();
        require_dispatch_entry();
        PumpScope scope;
        wxTheApp->ProcessPendingEvents();
    });
}
int wxl_stop() { return status([] { ready(); require(running, "Event loop is not running"); wxTheApp->ExitMainLoop(); }); }
int wxl_finish() {
    return status([] {
        ready();
        require_dispatch_entry();
        finishing = true; ui_timers.clear(); ui_actions.clear(); ui_dispatch = nullptr;
        struct RestoreIdleMode {
            ~RestoreIdleMode() { wxIdleEvent::SetMode(initial_idle_mode); }
        } restore_idle_mode;
        while (!bindings.empty()) remove_binding(bindings.begin()->first);
        // Detach stacks before any native window/handler teardown.
        for (const auto& item : windows)
            if (auto* live = item.second.get()) detach_window_stack(live);
        // Destroy remaining top-level windows before OnExit (also on setup failure).
        while (!wxTopLevelWindows.empty())
            delete wxTopLevelWindows.GetFirst()->GetData();
        owned_events.clear();
        pushed_handlers.clear();
        owned_handlers.clear();
        application_handle = 0;
        wxTheApp->OnExit();
        workflow_modal_hook.reset();
        wxEntryCleanup();
        managed_window_states.clear(); input_owners.clear(); windows.clear(); handlers.clear(); pending_destruction.clear(); registered_events.clear(); dispatch = nullptr; started = false; pending_callback_error = false;
    });
}
wxl_handle wxl_message_dialog(wxl_handle parent, const char* message, size_t message_length,
                             const char* title, size_t title_length, int buttons, int icon) {
    return create([&] {
        require(buttons >= 0 && buttons <= 3 && icon >= 0 && icon <= 4, "Invalid message dialog configuration");
        const auto msg = menu_string(message, message_length, true);
        const auto caption = menu_string(title, title_length, true);
        const long button_styles[] = {wxOK, wxOK | wxCANCEL, wxYES_NO, wxYES_NO | wxCANCEL};
        const long icon_styles[] = {0, wxICON_INFORMATION, wxICON_WARNING, wxICON_ERROR, wxICON_QUESTION};
        auto* requested_parent = parent ? window(parent) : nullptr;
        auto object = std::make_unique<ManagedWindow<PreparedMessageDialog>>(requested_parent, msg, caption, button_styles[buttons] | icon_styles[icon]);
        if (object->GetParent() != requested_parent) require(object->Reparent(requested_parent), "Cannot preserve requested dialog parent");
        dialog_message_buttons.emplace(object.get(), buttons);
        const auto id = track(object.get()); object.release(); return id;
    });
}
wxl_handle wxl_file_dialog(wxl_handle parent, const char* title, size_t title_length,
                          const char* directory, size_t directory_length, const char* filename, size_t filename_length,
                          const char* wildcard, size_t wildcard_length, int mode, int must_exist, int overwrite_prompt) {
    return create([&] {
        require(mode == 0 || mode == 1, "Invalid file dialog mode");
        require((must_exist == 0 || must_exist == 1) && (overwrite_prompt == 0 || overwrite_prompt == 1), "Invalid file dialog flags");
        require(mode == 0 ? !overwrite_prompt : !must_exist, "File dialog flag does not apply to this mode");
        const auto caption = menu_string(title, title_length, true);
        const auto dir = menu_string(directory, directory_length, true);
        const auto name = menu_string(filename, filename_length, true);
        const auto filters = menu_string(wildcard, wildcard_length, false);
        require(dir.empty() || wxDirExists(dir), "File dialog directory must exist");
        size_t parts = 0, start = 0;
        const std::string raw(wildcard, wildcard_length);
        while (true) {
            const auto delimiter = raw.find('|', start);
            const auto end = delimiter == std::string::npos ? raw.size() : delimiter;
            require(end != start, "File filters require nonempty description/pattern pairs");
            ++parts;
            if (delimiter == std::string::npos) break;
            start = delimiter + 1;
        }
        require(parts % 2 == 0 && parts / 2 <= static_cast<size_t>(std::numeric_limits<int>::max()), "File filters require description/pattern pairs");
        const long flags = (mode == 0 ? wxFD_OPEN : wxFD_SAVE) | (must_exist ? wxFD_FILE_MUST_EXIST : 0) | (overwrite_prompt ? wxFD_OVERWRITE_PROMPT : 0);
        auto* requested_parent = parent ? window(parent) : nullptr;
        DialogOperationScope operation(requested_parent);
        auto object = std::make_unique<ManagedWindow<wxGenericFileDialog>>(requested_parent, caption, dir, name, filters, flags);
        // Cocoa wxDialog::Create strips button flags which overlap FD_SAVE and
        // FD_OVERWRITE_PROMPT. Restore the requested file policy after creation.
        object->SetWindowStyleFlag(object->GetWindowStyleFlag() | flags);
        if (object->GetParent() != requested_parent) require(object->Reparent(requested_parent), "Cannot preserve requested dialog parent");
        dialog_file_filters.emplace(object.get(), static_cast<int>(parts / 2));
        const auto id = track(object.get()); object.release(); return id;
    });
}
const char* wxl_dialog_message(wxl_handle id, size_t* length) { return metadata_string([&] { return as<PreparedMessageDialog>(id)->GetMessage(); }, length); }
int wxl_message_dialog_buttons(wxl_handle id) {
    int value = -1; const auto result = status([&] { auto* object = as<PreparedMessageDialog>(id); value = dialog_message_buttons.at(object); }); return result == 0 ? value : -1;
}
int wxl_file_dialog_mode(wxl_handle id) { int mode = -1; const auto result = status([&] { mode = as<wxGenericFileDialog>(id)->HasFdFlag(wxFD_SAVE) ? 1 : 0; }); return result == 0 ? mode : -1; }
const char* wxl_file_dialog_path(wxl_handle id, size_t* length) { return metadata_string([&] { return as<wxGenericFileDialog>(id)->GetPath(); }, length); }
int wxl_set_file_dialog_path(wxl_handle id, const char* path, size_t length) { return status([&] {
    auto* object = as<wxGenericFileDialog>(id);
    const auto value = menu_string(path, length, true);
    const auto directory = wxFileName(value).GetPath();
    require(directory.empty() || wxDirExists(directory), "File dialog path directory must exist");
    DialogOperationScope operation(object);
    object->SetPath(value);
}); }
int wxl_file_dialog_filter_index(wxl_handle id) { int value = -1; const auto result = status([&] { value = as<wxGenericFileDialog>(id)->GetFilterIndex(); }); return result == 0 ? value : -1; }
int wxl_set_file_dialog_filter_index(wxl_handle id, int value) { return status([&] {
    auto* object = as<wxGenericFileDialog>(id);
    require(value >= 0 && value < dialog_file_filters.at(object), "File dialog filter index out of range");
    DialogOperationScope operation(object);
    object->SetFilterIndex(value);
}); }
wxl_handle wxl_text_entry_dialog(wxl_handle parent, const char* message, size_t message_length,
                                const char* title, size_t title_length, const char* value, size_t value_length) {
    return create([&] {
        const auto msg = menu_string(message, message_length, true);
        const auto caption = menu_string(title, title_length, true);
        const auto initial = menu_string(value, value_length, true);
        auto object = std::make_unique<ManagedWindow<wxTextEntryDialog>>(parent ? window(parent) : nullptr, msg, caption, initial);
        auto* requested_parent = parent ? window(parent) : nullptr;
        if (object->GetParent() != requested_parent) require(object->Reparent(requested_parent), "Cannot preserve requested dialog parent");
        const auto id = track(object.get()); object.release(); return id;
    });
}
wxl_handle wxl_single_choice_dialog(wxl_handle parent, const char* message, size_t message_length,
                                   const char* title, size_t title_length, const char** items, size_t count, int selection) {
    return create([&] {
        require(items && count && count <= static_cast<size_t>(std::numeric_limits<int>::max()), "Choice dialog requires nonempty bounded items");
        require(selection >= 0 && static_cast<size_t>(selection) < count, "Invalid dialog selection");
        const auto msg = menu_string(message, message_length, true);
        const auto caption = menu_string(title, title_length, true);
        wxArrayString choices;
        for (size_t index = 0; index < count; ++index) { require(items[index], "Null choice dialog item"); choices.Add(menu_string(items[index], std::strlen(items[index]), true)); }
        auto object = std::make_unique<ManagedWindow<wxSingleChoiceDialog>>(parent ? window(parent) : nullptr, msg, caption, choices);
        auto* requested_parent = parent ? window(parent) : nullptr;
        if (object->GetParent() != requested_parent) require(object->Reparent(requested_parent), "Cannot preserve requested dialog parent");
        object->SetSelection(selection);
        dialog_choice_counts.emplace(object.get(), count);
        const auto id = track(object.get()); object.release(); return id;
    });
}
int wxl_dialog_show(wxl_handle id, int* code) {
    return status([&] {
        auto* object = as<wxDialog>(id);
        require(code, "Missing modal result output");
        require(notebook_operations.empty() && list_callback_operations.empty() && tree_operations.empty() && splitter_operations.empty() && toolbar_operations.empty() && accelerator_action_frames.empty() && input_operations.empty(), "Defer dialogs until navigation callbacks return");
        require(!active_modal.get() && actual_modals.empty(), "Nested modal dialogs are not supported");
        require(!object->IsShown() && !object->IsModal(), "Dialog is already shown");
        struct ModalScope {
            CallbackScope suspended_context;
            ModalCloseGuard close_guard;
            explicit ModalScope(wxDialog* object) : close_guard(object) {
                ++modal_workflow_epoch; modal_cancel_requested = false; active_modal = object;
            }
            ~ModalScope() {
                if (auto* object = active_modal.get(); object && object->IsModal()) object->EndModal(wxID_CANCEL);
                active_modal = nullptr; modal_cancel_requested = false; ++modal_workflow_epoch;
            }
        } scope(object);
        const auto native_code = object->ShowModal();
        const int result = bridge_dialog_result(native_code);
        validate_dialog_result(object, result);
        *code = result;
    });
}
int wxl_dialog_end(wxl_handle id, int code) {
    return status([&] {
        auto* object = as<wxDialog>(id);
        validate_dialog_result(object, code);
        require(active_modal.get() == object, "Dialog is not currently modal");
        require(object->IsModal(), "Dialog is not currently modal");
        if (code == 1) { request_modal_cancel(); return; }
        require(actual_modals.size() == 1 && actual_modals.back().get() == object, "Cannot accept outer dialog during an internal prompt");
        object->EndModal(native_dialog_result(code));
    });
}
int wxl_dialog_abort() {
    return boolean_result([] {
        ready(); return request_modal_cancel();
    });
}
int wxl_callback_error_pending(int pending) {
    return status([&] { ready(); require(pending == 0 || pending == 1, "Invalid pending callback error flag"); pending_callback_error = pending != 0; });
}
int wxl_dialog_depth() { int depth = -1; const auto result = status([&] { ready(); depth = static_cast<int>(actual_modals.size()); }); return result == 0 ? depth : -1; }
int wxl_dialog_modal(wxl_handle id) { return boolean_result([&] { return as<wxDialog>(id)->IsModal(); }); }
const char* wxl_dialog_title(wxl_handle id, size_t* length) { return metadata_string([&] { return as<wxDialog>(id)->GetTitle(); }, length); }
int wxl_set_dialog_title(wxl_handle id, const char* title, size_t length) { return status([&] { as<wxDialog>(id)->SetTitle(menu_string(title, length, true)); }); }
const char* wxl_dialog_value(wxl_handle id, size_t* length) { return metadata_string([&] { return as<wxTextEntryDialog>(id)->GetValue(); }, length); }
int wxl_set_dialog_value(wxl_handle id, const char* value, size_t length) { return status([&] { as<wxTextEntryDialog>(id)->SetValue(menu_string(value, length, true)); }); }
int wxl_dialog_selection(wxl_handle id) {
    int value = -1; const auto result = status([&] { value = as<wxSingleChoiceDialog>(id)->GetSelection(); }); return result == 0 ? value : -1;
}
int wxl_set_dialog_selection(wxl_handle id, int value) { return status([&] {
    auto* object = as<wxSingleChoiceDialog>(id);
    const auto found = dialog_choice_counts.find(object);
    require(found != dialog_choice_counts.end() && value >= 0 && static_cast<size_t>(value) < found->second, "Invalid dialog selection");
    object->SetSelection(value);
}); }
const char* wxl_dialog_selection_text(wxl_handle id, size_t* length) { return metadata_string([&] { return as<wxSingleChoiceDialog>(id)->GetStringSelection(); }, length); }
int wxl_set_frame_accelerators(wxl_handle id, const int* keys, const int* flags, const int* commands, size_t count) {
    return status([&] {
        require_dispatch_entry(true); auto* frame = as<wxFrame>(id);
        require(count <= 256 && (!count || (keys && flags && commands)), "Invalid accelerator arrays/count");
        std::vector<wxAcceleratorEntry> entries; entries.reserve(count);
        std::vector<AcceleratorSpec> specs; specs.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            const auto key = accelerator_key(keys[i]), mods = accelerator_flags(flags[i]);
            require(commands[i] >= 1 && commands[i] <= 32767, "Accelerator command ID must be 1..32767");
            require(keys[i] >= 1001 || (flags[i] & 13), "Letter/digit accelerators require control, alt or raw control");
            for (const auto& entry : entries)
                require(entry.GetKeyCode() != key || entry.GetFlags() != mods, "Duplicate effective accelerator chord");
            entries.emplace_back(mods, key, commands[i]); // No borrowed wxMenuItem pointer.
            specs.push_back({keys[i], flags[i], commands[i]});
        }
        wxAcceleratorTable table = count ? wxAcceleratorTable(static_cast<int>(count), entries.data()) : wxNullAcceleratorTable;
        require(!count || table.IsOk(), "Native accelerator table creation failed");
        // Allocate registry storage before publishing. Native table owns a copied,
        // refcounted value; moving the prepared metadata cannot allocate afterward.
        auto& stored = frame_accelerators[frame];
        frame->SetAcceleratorTable(table); stored = std::move(specs);
    });
}
int wxl_frame_accelerator_count(wxl_handle id) {
    int count = -1; status([&] { auto* frame = as<wxFrame>(id); const auto found = frame_accelerators.find(frame);
        count = found == frame_accelerators.end() ? 0 : static_cast<int>(found->second.size()); }); return count;
}
int wxl_frame_accelerator_entry(wxl_handle id, int index, int* key, int* flags, int* command) { return status([&] {
    auto* frame = as<wxFrame>(id); const auto found = frame_accelerators.find(frame);
    require(key && flags && command && index >= 0 && found != frame_accelerators.end() && static_cast<size_t>(index) < found->second.size(), "Invalid accelerator index/output");
    const auto& entry = found->second[index]; *key = entry.key; *flags = entry.flags; *command = entry.command;
}); }
// wxOSX Cocoa NSToolbar identifiers embed C++ addresses and restored item IDs
// embed tool pointers. Address reuse can resurrect stale native placeholders.
// Select wx's ordinary in-window control backend during construction instead.
struct ToolbarBackendScope {
#if defined(__WXOSX__) && wxUSE_SYSTEM_OPTIONS
    wxString previous = wxSystemOptions::GetOption("mac.toolbar.no-native");
    ToolbarBackendScope() { wxSystemOptions::SetOption("mac.toolbar.no-native", 1); }
    ~ToolbarBackendScope() { wxSystemOptions::SetOption("mac.toolbar.no-native", previous); }
#else
    ToolbarBackendScope() {
#ifdef __WXOSX__
        require(false, "Safe OSX toolbar backend requires wxUSE_SYSTEM_OPTIONS");
#endif
    }
#endif
};
// Frame bars use native window ownership. Tool IDs are scoped to each toolbar.
wxl_handle wxl_toolbar(wxl_handle frame_id) {
    return create([&] {
        require_dispatch_entry(true); auto* frame = as<wxFrame>(frame_id);
        require(!frame->GetToolBar(), "Frame already has a toolbar");
        ToolbarBackendScope backend;
        auto bar = std::make_unique<ManagedWindow<wxToolBar>>(frame, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT);
        const auto id = track(bar.get()); toolbar_owners.insert(bar.get()); frame->SetToolBar(bar.get()); frame->Layout(); bar.release(); return id;
    });
}
wxl_handle wxl_status_bar(wxl_handle frame_id, int fields) {
    return create([&] {
        require_dispatch_entry(true); auto* frame = as<wxFrame>(frame_id);
        require(fields >= 1 && fields <= 16, "Status field count must be 1..16");
        require(!frame->GetStatusBar(), "Frame already has a status bar");
        auto bar = std::make_unique<ManagedWindow<wxStatusBar>>(frame, wxID_ANY);
        bar->SetFieldsCount(fields); const auto id = track(bar.get());
        frame->SetStatusBar(bar.get()); frame->SetStatusBarPane(-1); // App owns all text.
        bar.release(); return id;
    });
}
int wxl_add_tool(wxl_handle id, int tool_id, const char* label, size_t length, int art, int kind, const char* help, size_t help_length) {
    return status([&] {
        require_dispatch_entry(true); auto* bar = as<wxToolBar>(id);
        require(!realized_toolbars.count(bar), "Tools are fixed after Realize");
        require(kind >= 0 && kind <= 2, "Unsupported tool kind");
        if (kind == 2) {
            require(tool_id == 0 && art == -1 && !length && !help_length, "Separator requires zero ID, no art or text");
            require(bar->AddSeparator() != nullptr, "Native separator creation failed"); return;
        }
        require(tool_id > 0 && !bar->FindById(tool_id), "Tool ID must be positive and unique within toolbar");
        const wxArtID arts[] = {wxART_NEW, wxART_FILE_OPEN, wxART_FILE_SAVE, wxART_COPY, wxART_PASTE, wxART_QUIT, wxART_INFORMATION};
        require(art >= 0 && art < 7, "Unsupported stock tool art");
        const auto text = menu_string(label, length, false), hint = menu_string(help, help_length, true);
        const auto bitmap = wxArtProvider::GetBitmap(arts[art], wxART_TOOLBAR, wxSize(16,16));
        require(bitmap.IsOk(), "Stock tool art unavailable on this backend");
        require(bar->AddTool(tool_id, text, wxBitmapBundle::FromBitmap(bitmap), wxBitmapBundle(), kind ? wxITEM_CHECK : wxITEM_NORMAL, hint) != nullptr, "Native tool creation failed");
    });
}
int wxl_realize_toolbar(wxl_handle id) { return status([&] {
    require_dispatch_entry(true); auto* bar = as<wxToolBar>(id);
    require(!realized_toolbars.count(bar), "Toolbar already realized");
    require(bar->GetToolsCount() != 0, "Add tools before Realize");
    ToolbarOperation operation(bar); require(bar->Realize(), "Native toolbar realization failed"); realized_toolbars.insert(bar);
    bar->GetParent()->Layout();
}); }
int wxl_tool_count(wxl_handle id) {
    int value = -1; status([&] { value = static_cast<int>(as<wxToolBar>(id)->GetToolsCount()); }); return value;
}
// Field 0 enabled, 1 checked. Queries reject separators and wrong-kind checks.
int wxl_tool_state(wxl_handle id, int tool_id, int field) { return boolean_result([&] {
    auto* bar = as<wxToolBar>(id); auto* tool = bar->FindById(tool_id);
    require(tool && !tool->IsSeparator(), "Unknown/non-action tool ID");
    require(field == 0 || (field == 1 && tool->CanBeToggled()), "Tool is not checkable or field is invalid");
    return field == 0 ? bar->GetToolEnabled(tool_id) : bar->GetToolState(tool_id);
}); }
int wxl_set_tool_state(wxl_handle id, int tool_id, int field, int value) { return status([&] {
    auto* bar = as<wxToolBar>(id); toolbar_mutable(bar); auto* tool = bar->FindById(tool_id);
    require(tool && !tool->IsSeparator(), "Unknown/non-action tool ID");
    require(field == 0 || (field == 1 && tool->CanBeToggled()), "Tool is not checkable or field is invalid");
    require(value == 0 || value == 1, "Invalid boolean tool state");
    if (field == 0) bar->EnableTool(tool_id, value != 0); else bar->ToggleTool(tool_id, value != 0);
}); }
int wxl_activate_tool(wxl_handle id, int tool_id) { return boolean_result([&] {
    auto* bar = as<wxToolBar>(id); toolbar_mutable(bar);
    require(realized_toolbars.count(bar), "Realize toolbar before activation");
    auto* tool = bar->FindById(tool_id); require(tool && !tool->IsSeparator(), "Unknown/non-action tool ID");
    if (!tool->IsEnabled()) return false;
    ToolbarOperation operation(bar);
    if (tool->CanBeToggled()) bar->ToggleTool(tool_id, !tool->IsToggled());
    bar->OnLeftClick(tool_id, tool->CanBeToggled() && tool->IsToggled());
    return true; // Accepted activation, independent of event handled/Skip result.
}); }
int wxl_status_fields(wxl_handle id) {
    int value = -1; status([&] { value = as<wxStatusBar>(id)->GetFieldsCount(); }); return value;
}
const char* wxl_status_text(wxl_handle id, int field, size_t* length) { return metadata_string([&] {
    auto* bar = as<wxStatusBar>(id); require(field >= 0 && field < bar->GetFieldsCount(), "Status field out of range"); return bar->GetStatusText(field);
}, length); }
int wxl_set_status_text(wxl_handle id, int field, const char* text, size_t length) { return status([&] {
    auto* bar = as<wxStatusBar>(id); require(field >= 0 && field < bar->GetFieldsCount(), "Status field out of range"); bar->SetStatusText(menu_string(text, length, true), field);
}); }
wxl_handle wxl_menu_bar(wxl_handle frame_id) {
    return create([&] {
        require_dispatch_entry(true); auto* owner = as<wxFrame>(frame_id);
        require(!owner->GetMenuBar(), "Frame already has a menu bar");
        auto object = std::make_unique<ManagedMenuBar>();
        const auto id = next_handle++; object->identity = id;
        menu_resources.emplace(id, MenuResource{object.get(), frame_id, frame_id, 0});
        try { owner->SetMenuBar(object.get()); }
        catch (...) { if (owner->GetMenuBar() == object.get()) owner->SetMenuBar(nullptr); throw; }
        object.release(); return id;
    });
}
wxl_handle wxl_menu(wxl_handle bar_id, const char* label, size_t length) {
    return create([&] {
        require_dispatch_entry(true); const auto parent = menu_resource(bar_id, 0);
        const auto title = menu_string(label, length, false);
        auto object = std::make_unique<ManagedMenu>();
        const auto id = next_handle++; object->identity = id;
        menu_resources.emplace(id, MenuResource{object.get(), parent.frame, bar_id, 1});
        auto* bar = static_cast<wxMenuBar*>(parent.object);
        try { require(bar->Append(object.get(), title), "Native menu append failed"); }
        catch (...) {
            for (size_t index = 0; index < bar->GetMenuCount(); ++index)
                if (bar->GetMenu(index) == object.get()) { bar->Remove(index); break; }
            throw;
        }
        object.release(); return id;
    });
}
wxl_handle wxl_menu_item(wxl_handle menu_id, int item_id, const char* label, size_t length,
                         const char* help, size_t help_length, int kind) {
    return create([&] {
        require_dispatch_entry(true); const auto parent = menu_resource(menu_id, 1);
        require(kind >= 0 && kind <= 2, "Unsupported menu item kind");
        require((label || !length) && (help || !help_length), "Null menu item text");
        require(kind == 2 ? item_id == 0 && !length && !help_length : item_id > 0,
                "Menu item requires a positive ID; separator requires zero and empty text");
        require(kind == 2 || length != 0, "Menu item label must be nonempty");
        const auto menu_record = menu_resource(parent.parent, 0);
        auto* bar = static_cast<wxMenuBar*>(menu_record.object);
        require(kind == 2 || !bar->FindItem(item_id), "Duplicate menu item ID in menu bar");
        const auto item_label = menu_string(label, length, kind == 2);
        const auto item_help = menu_string(help, help_length, true);
        auto object = std::make_unique<ManagedMenuItem>(static_cast<wxMenu*>(parent.object),
            kind == 2 ? wxID_SEPARATOR : item_id, item_label, item_help,
            kind == 2 ? wxITEM_SEPARATOR : kind ? wxITEM_CHECK : wxITEM_NORMAL);
        const auto id = next_handle++; object->identity = id;
        menu_resources.emplace(id, MenuResource{object.get(), parent.frame, menu_id, 2});
        auto* menu = static_cast<wxMenu*>(parent.object);
        try { require(menu->Append(object.get()) != nullptr, "Native item append failed"); }
        catch (...) {
            for (const auto* entry : menu->GetMenuItems())
                if (entry == object.get()) { menu->Remove(object.get()); break; }
            throw;
        }
        object.release(); return id;
    });
}
int wxl_menu_alive(wxl_handle id) {
    return boolean_result([&] {
        ready(); const auto found = menu_resources.find(id); if (found == menu_resources.end()) return false;
        const auto owner = windows.find(found->second.frame);
        return owner != windows.end() && owner->second.get() && !window_destruction_pending(owner->second.get());
    });
}
int wxl_menu_destroy(wxl_handle id) {
    return status([&] {
        ready(); require_dispatch_entry(true); const auto resource = menu_resource(id);
        if (resource.kind == 0) { as<wxFrame>(resource.frame)->SetMenuBar(nullptr); delete static_cast<wxMenuBar*>(resource.object); }
        else if (resource.kind == 1) {
            auto* bar = static_cast<wxMenuBar*>(menu_resource(resource.parent, 0).object);
            delete bar->Remove(menu_position(bar, static_cast<wxMenu*>(resource.object)));
        } else require(static_cast<wxMenu*>(menu_resource(resource.parent, 1).object)->Destroy(static_cast<wxMenuItem*>(resource.object)), "Native item destruction failed");
    });
}
int wxl_menu_count(wxl_handle id) {
    int value = -1; const auto result = status([&] { const auto r = menu_resource(id); require(r.kind != 2, "Item has no children");
        value = static_cast<int>(r.kind == 0 ? static_cast<wxMenuBar*>(r.object)->GetMenuCount() : static_cast<wxMenu*>(r.object)->GetMenuItemCount()); });
    return result == 0 ? value : -1;
}
int wxl_menu_item_id(wxl_handle id, int* value) { return status([&] { const auto r = menu_resource(id, 2); require(value, "Missing menu ID output"); *value = static_cast<wxMenuItem*>(r.object)->GetId(); }); }
int wxl_menu_item_kind(wxl_handle id) {
    int value = -1; const auto result = status([&] { auto* item = static_cast<wxMenuItem*>(menu_resource(id, 2).object); value = item->IsSeparator() ? 2 : item->IsCheckable() ? 1 : 0; }); return result == 0 ? value : -1;
}
const char* wxl_menu_label(wxl_handle id, size_t* length) {
    return metadata_string([&] {
        const auto r = menu_resource(id);
        require(r.kind != 0, "Bar has no label");
        return r.kind == 2 ? static_cast<wxMenuItem*>(r.object)->GetItemLabel() :
            static_cast<wxMenuBar*>(menu_resource(r.parent, 0).object)->GetMenuLabel(menu_position(static_cast<wxMenuBar*>(menu_resource(r.parent, 0).object), static_cast<wxMenu*>(r.object)));
    }, length);
}
int wxl_set_menu_label(wxl_handle id, const char* text, size_t length) { return status([&] {
    const auto r = menu_resource(id); require(r.kind != 0, "Bar has no label"); const auto value = menu_string(text, length, false);
    if (r.kind == 2) { auto* item = static_cast<wxMenuItem*>(r.object); require(!item->IsSeparator(), "Separator has no label"); item->SetItemLabel(value); }
    else { auto* bar = static_cast<wxMenuBar*>(menu_resource(r.parent, 0).object); bar->SetMenuLabel(menu_position(bar, static_cast<wxMenu*>(r.object)), value); }
}); }
int wxl_menu_enabled(wxl_handle id) { return boolean_result([&] { auto* item = static_cast<wxMenuItem*>(menu_resource(id, 2).object); require(!item->IsSeparator(), "Separator cannot be enabled"); return item->IsEnabled(); }); }
int wxl_set_menu_enabled(wxl_handle id, int value) { return boolean_result([&] { auto* item = static_cast<wxMenuItem*>(menu_resource(id, 2).object); require(!item->IsSeparator(), "Separator cannot be enabled"); const bool changed = item->IsEnabled() != (value != 0); item->Enable(value != 0); return changed; }); }
int wxl_menu_checked(wxl_handle id) { return boolean_result([&] { auto* item = static_cast<wxMenuItem*>(menu_resource(id, 2).object); require(item->IsCheckable(), "Item is not checkable"); return item->IsChecked(); }); }
int wxl_set_menu_checked(wxl_handle id, int value) { return status([&] { auto* item = static_cast<wxMenuItem*>(menu_resource(id, 2).object); require(item->IsCheckable(), "Item is not checkable"); item->Check(value != 0); }); }
int wxl_activate_menu(wxl_handle id) { return boolean_result([&] {
    const auto r = menu_resource(id, 2); auto* item = static_cast<wxMenuItem*>(r.object);
    require(!item->IsSeparator(), "Cannot activate a separator"); if (!item->IsEnabled()) return false;
    return as<wxFrame>(r.frame)->ProcessCommand(item->GetId());
}); }
int wxl_idle_mode() {
    int mode = -1;
    const int result = status([&] { ready(); mode = wxIdleEvent::GetMode() == wxIDLE_PROCESS_SPECIFIED ? 1 : 0; });
    return result == 0 ? mode : -1;
}
int wxl_set_idle_mode(int mode) {
    return status([&] {
        ready(); require(mode == 0 || mode == 1, "Invalid idle mode");
        wxIdleEvent::SetMode(mode ? wxIDLE_PROCESS_SPECIFIED : wxIDLE_PROCESS_ALL);
    });
}
int wxl_window_process_idle(wxl_handle id) {
    return boolean_result([&] { return (window(id)->GetExtraStyle() & wxWS_EX_PROCESS_IDLE) != 0; });
}
int wxl_set_window_process_idle(wxl_handle id, int enabled) {
    return status([&] {
        auto* object = window(id);
        const auto style = object->GetExtraStyle();
        object->SetExtraStyle(enabled ? style | wxWS_EX_PROCESS_IDLE : style & ~wxWS_EX_PROCESS_IDLE);
    });
}
int wxl_wake_up_idle() { return status([] { ready(); wxWakeUpIdle(); }); }
wxl_handle wxl_application_handler() {
    return create([] { return application_handle; });
}
wxl_handle wxl_event_handler() {
    return create([] {
        auto object = std::make_unique<StandaloneHandler>();
        const auto id = next_handle++;
        owned_handlers.emplace(id, std::move(object));
        return id;
    });
}
int wxl_handler_alive(wxl_handle id) {
    return boolean_result([&] {
        ready();
        if (id && id == application_handle) return true;
        if (pending_destruction.count(id)) return false;
        const auto it = windows.find(id);
        if (it != windows.end()) {
            auto* object = it->second.get();
            return object && !window_destruction_pending(object);
        }
        return owned_handlers.find(id) != owned_handlers.end();
    });
}
int wxl_handler_enabled(wxl_handle id) {
    return boolean_result([&] { return event_handler(id)->GetEvtHandlerEnabled(); });
}
int wxl_set_handler_enabled(wxl_handle id, int enabled) {
    return status([&] { event_handler(id)->SetEvtHandlerEnabled(enabled != 0); });
}
int wxl_handler_client_data(wxl_handle id, uint64_t* token) {
    return status([&] {
        auto* object = event_handler(id);
        require(token != nullptr, "Missing client-data token output");
        const auto* data = token_client_data(object, true);
        *token = data ? data->token : 0;
    });
}
int wxl_set_handler_client_data(wxl_handle id, uint64_t token) {
    return status([&] {
        auto* object = event_handler(id);
        token_client_data(object, true);
        // Allocate first: allocation failure leaves the previous owned object
        // intact. SetClientObject then owns/deletes replacement and old data.
        auto replacement = token ? std::make_unique<TokenClientData>(token) : nullptr;
        object->SetClientObject(replacement.get());
        replacement.release();
    });
}
int wxl_client_data_active(uint64_t token) {
    return boolean_result([&] {
        ready();
        if (!token) return false;
        const auto matches = [token](wxEvtHandler* object) {
            const auto* data = token_client_data(object, false);
            return data && data->token == token;
        };
        if (application_handle && matches(wxTheApp)) return true;
        for (const auto& item : owned_handlers)
            if (matches(item.second.get())) return true;
        // A pending-destruction object can still own its token until actual
        // deletion; only dereference windows whose weak reference is non-null.
        for (const auto& item : windows)
            if (auto* object = item.second.get(); object && matches(object)) return true;
        return false;
    });
}
int wxl_handler_destroy(wxl_handle id) {
    return status([&] {
        ready();
        require(id != application_handle, "Application event handler is borrowed and cannot be destroyed");
        require(windows.find(id) == windows.end(), "Window event handlers are borrowed; use destroy-window");
        const auto it = owned_handlers.find(id);
        require(it != owned_handlers.end(), "Invalid or destroyed event-handler handle");
        require(!pending_destruction.count(id), "Handler destruction is pending");
        require(!callback_depth || it->second->IsUnlinked(), "Cannot destroy a linked handler during a callback");
        if (callback_depth) {
            pending_destruction.insert(id);
            try {
                wxTheApp->CallAfter([id] { owned_handlers.erase(id); pending_destruction.erase(id); });
            } catch (...) { pending_destruction.erase(id); throw; }
        } else {
            if (pushed_handlers.count(id)) { require_dispatch_entry(true); remove_pushed_handler(id); }
            owned_handlers.erase(it);
        }
    });
}
int wxl_handler_chain_peer(wxl_handle id, int previous, wxl_handle* peer) {
    return status([&] {
        require(peer != nullptr, "Missing handler peer output");
        auto* source = standalone_handler(id);
        *peer = owned_handler_id(previous ? source->GetPreviousHandler() : source->GetNextHandler());
    });
}
int wxl_handler_unlinked(wxl_handle id) {
    return boolean_result([&] {
        auto* object = event_handler(id);
        require(owned_handlers.count(id), "Unlinked-state query requires an owned handler");
        return object->IsUnlinked();
    });
}
int wxl_link_handlers(wxl_handle id, wxl_handle peer_id, int previous) {
    return status([&] {
        ready();
        require_dispatch_entry(true);
        auto* source = standalone_handler(id);
        validate_chain_neighbors(source);
        if (!peer_id) {
            if (previous) {
                if (auto* peer = source->GetPreviousHandler()) {
                    peer->SetNextHandler(nullptr);
                    source->SetPreviousHandler(nullptr);
                }
            } else if (auto* peer = source->GetNextHandler()) {
                peer->SetPreviousHandler(nullptr);
                source->SetNextHandler(nullptr);
            }
            return;
        }
        auto* peer = standalone_handler(peer_id);
        validate_chain_neighbors(peer);
        require(source != peer, "A handler cannot link to itself");
        auto* left = previous ? peer : source;
        auto* right = previous ? source : peer;
        if (left->GetNextHandler() == right && right->GetPreviousHandler() == left) return;
        require(!left->GetNextHandler() && !right->GetPreviousHandler(),
                "Disconnect occupied chain endpoints before linking");
        for (auto* node = right; node; node = node->GetNextHandler()) {
            owned_handler_id(node);
            require(node != left, "Handler chains cannot contain cycles");
        }
        left->SetNextHandler(right);
        right->SetPreviousHandler(left);
    });
}
int wxl_unlink_handler(wxl_handle id) {
    return status([&] {
        ready();
        require_dispatch_entry(true);
        auto* source = standalone_handler(id);
        validate_chain_neighbors(source);
        source->Unlink();
    });
}
int wxl_event_type(int kind) {
    int type = 0;
    const int result = status([&] { ready(); type = native_event_type(kind); });
    return result == 0 ? type : -1;
}
int wxl_context_user_data(uint64_t token, uint64_t* value) {
    return status([&] {
        current_event(token);
        require(value != nullptr, "Missing user-data output");
        *value = EventScope::stack.back().user_data;
    });
}
int wxl_event_user_data(wxl_handle id, uint64_t* value) {
    return status([&] {
        auto& event = owned_event(id);
        require(value != nullptr, "Missing user-data output");
        require(event.GetEventUserData() == nullptr, "Owned event contains borrowed native user data");
        *value = 0;
    });
}
int wxl_context_active(uint64_t token) {
    return boolean_result([&] {
        ready();
        return token && !EventScope::stack.empty() && EventScope::stack.back().token == token
            && EventScope::stack.back().depth == callback_depth;
    });
}
int wxl_context_skipped(uint64_t token) {
    return boolean_result([&] { return current_event(token).GetSkipped(); });
}
int wxl_context_set_skipped(uint64_t token, int skip) {
    return status([&] { current_event(token).Skip(skip != 0); });
}
int wxl_context_propagates(uint64_t token) {
    return boolean_result([&] { return current_event(token).ShouldPropagate(); });
}
int wxl_context_stop_propagation(uint64_t token) {
    int level = 0;
    const int result = status([&] { level = current_event(token).StopPropagation(); });
    return result == 0 ? level : -1;
}
int wxl_context_can_veto(uint64_t token) {
    return boolean_result([&] {
        return event_can_veto(current_event(token));
    });
}
int wxl_context_set_can_veto(uint64_t token, int value) {
    return status([&] {
        auto* close = dynamic_cast<wxCloseEvent*>(&current_event(token));
        require(close != nullptr, "CanVeto mutation requires a close-family event");
        close->SetCanVeto(value != 0);
    });
}
int wxl_event_category_mask(int category) {
    switch (category) {
        case 0: return wxEVT_CATEGORY_UI;
        case 1: return wxEVT_CATEGORY_USER_INPUT;
        case 2: return wxEVT_CATEGORY_SOCKET;
        case 3: return wxEVT_CATEGORY_TIMER;
        case 4: return wxEVT_CATEGORY_THREAD;
        case 5: return wxEVT_CATEGORY_ALL;
        default: return -1;
    }
}
int wxl_context_category(uint64_t token, int* value) {
    return status([&] {
        require(value != nullptr, "Missing event category output");
        *value = static_cast<int>(current_event(token).GetEventCategory());
    });
}
int wxl_context_set_native_type(uint64_t token, int type) {
    return status([&] { set_registered_type(current_event(token), type); });
}
int wxl_context_native_type(uint64_t token, int* value) {
    return status([&] {
        require(value != nullptr, "Missing native type output");
        *value = current_event(token).GetEventType();
    });
}
int wxl_context_origin_id(uint64_t token, wxl_handle* value) {
    return status([&] {
        require(value != nullptr, "Missing event origin output");
        *value = origin_id(current_event(token));
    });
}
int wxl_context_id(uint64_t token, int* value) {
    return status([&] {
        require(value != nullptr, "Missing event ID output");
        *value = current_event(token).GetId();
    });
}
int wxl_context_set_id(uint64_t token, int value) {
    return status([&] { auto& event=current_event(token);
        require(!aui_payload_readonly(event.GetEventType()) && !stc_payload_readonly(event.GetEventType()) && !input_payload_readonly(event.GetEventType()), "Native input/styled/AUI event IDs are read-only"); event.SetId(value); });
}
int wxl_context_timestamp(uint64_t token, int64_t* value) {
    return status([&] {
        require(value != nullptr, "Missing timestamp output");
        *value = current_event(token).GetTimestamp();
    });
}
int wxl_context_set_timestamp(uint64_t token, int64_t value) {
    return status([&] {
        auto& event = current_event(token);
        require(!aui_payload_readonly(event.GetEventType()) && !stc_payload_readonly(event.GetEventType()) && !input_payload_readonly(event.GetEventType()), "Native input/styled/AUI event timestamps are read-only");
        require(value >= std::numeric_limits<long>::min() && value <= std::numeric_limits<long>::max(),
                "Timestamp is outside the native long range");
        event.SetTimestamp(static_cast<long>(value));
    });
}
int wxl_context_command(uint64_t token) {
    return boolean_result([&] { return current_event(token).IsCommandEvent(); });
}
int wxl_context_vetoed(uint64_t token) {
    return boolean_result([&] {
        auto& event = current_event(token);
        require(dynamic_cast<wxCloseEvent*>(&event) || dynamic_cast<wxNotifyEvent*>(&event),
                "This event family has no veto state");
        return event_vetoed(event);
    });
}
int wxl_context_set_vetoed(uint64_t token, int vetoed) {
    return status([&] {
        auto& event = current_event(token);
        if (auto* close = dynamic_cast<wxCloseEvent*>(&event)) {
            require(close->CanVeto(), "This close event cannot be vetoed");
            close->Veto(vetoed != 0);
        } else if (auto* notify = dynamic_cast<wxNotifyEvent*>(&event)) {
            if (vetoed) notify->Veto(); else notify->Allow();
        } else throw std::runtime_error("This event family has no veto state");
    });
}
int wxl_context_logging_off(uint64_t token) {
    return boolean_result([&] {
        auto& event = current_event(token);
        require(session_type(event.GetEventType()), "Logging-off requires a session event");
        return static_cast<wxCloseEvent&>(event).GetLoggingOff();
    });
}
int wxl_context_set_logging_off(uint64_t token, int value) {
    return status([&] {
        auto& event = current_event(token);
        require(session_type(event.GetEventType()), "Logging-off requires a session event");
        static_cast<wxCloseEvent&>(event).SetLoggingOff(value != 0);
    });
}
int wxl_close(wxl_handle id, int force) {
    return boolean_result([&] { auto* object = as<wxFrame>(id); require(!protected_modal_ancestor(object), "Cannot close an active modal dialog ancestor"); require(!protected_notebook_ancestor(object) && !protected_list_ancestor(object) && !protected_tree_ancestor(object) && !protected_splitter_ancestor(object) && !protected_toolbar_ancestor(object) && !protected_accelerator_window(object) && !protected_aui_ancestor(object) && !protected_input_ancestor(object), "Cannot close a navigation callback ancestor"); return object->Close(force != 0); });
}
int wxl_context_resume_propagation(uint64_t token, int level) {
    return status([&] {
        auto& event = current_event(token);
        require(level >= 0, "Propagation level must be nonnegative");
        event.ResumePropagation(level);
    });
}
int wxl_bind(wxl_handle id, int kind, uint64_t token, int first, int last, int with_context, int user_data) {
    return status([&] {
        auto* receiver = event_handler(id);
        const auto type = native_event_type(kind);
        require(!session_type(type) || id == application_handle || isolated_handler(id),
                "Session bindings require the application view or an owned standalone handler");
        require(with_context == 0 || with_context == 1, "Invalid binding context mode");
        require(user_data == 0 || user_data == 1, "Invalid binding user-data mode");
        require(token && !bindings.count(token), "Invalid or duplicate binding token");
        require(last == wxID_ANY || (first != wxID_ANY && first <= last), "Invalid event ID range");
        auto binding = std::make_shared<Binding>();
        binding->receiver = receiver;
        binding->source = id; binding->token = token;
        binding->kind = kind; binding->id = first; binding->last_id = last;
        binding->with_context = with_context != 0;
        auto adapter = user_data ? std::make_unique<BindingUserData>(token) : nullptr;
        binding->user_data = adapter.get();
        bindings.emplace(token, binding);
        try {
            receiver->Bind(type, &Binding::on_event, binding.get(), first, last, adapter.get());
            adapter.release(); // Native dynamic binding now owns the adapter.
        }
        catch (...) { bindings.erase(token); throw; }
    });
}
int wxl_unbind(uint64_t token) {
    return boolean_result([&] {
        ready();
        const auto it = bindings.find(token);
        const bool active = it != bindings.end() && it->second->live();
        remove_binding(token);
        return active;
    });
}
int wxl_binding_active(uint64_t token) {
    return boolean_result([&] {
        ready();
        const auto it = bindings.find(token);
        if (it == bindings.end()) return false;
        if (it->second->live()) return true;
        remove_binding(token);
        return false;
    });
}
int wxl_register_event_type(int family) {
    int kind = 0;
    const int result = status([&] {
        ready();
        require(family == WXL_FAMILY_COMMAND || family == WXL_FAMILY_NOTIFY, "Unsupported custom event family");
        require(next_event_kind < std::numeric_limits<int>::max(), "Event kind space exhausted");
        kind = next_event_kind++;
        registered_events.emplace(kind, RegisteredEvent{wxNewEventType(), family});
    });
    return result == 0 ? kind : -1;
}
int wxl_queue_event(wxl_handle id, int kind, int event_id, const char* text, size_t length,
                    int value, int can_veto, int vetoed, int logging_off) {
    return status([&] {
        auto* receiver = event_handler(id);
        require(!session_type(native_event_type(kind)) || isolated_handler(id),
                "Synthetic session events require an isolated owned standalone handler");
        auto event = synthetic_event(receiver, kind, event_id, text, length, value, can_veto, vetoed, logging_off);
        receiver->QueueEvent(event.release());
    });
}
int wxl_process_event(wxl_handle id, int kind, int event_id, const char* text, size_t length,
                      int value, int can_veto, int vetoed, int logging_off, int* final_veto) {
    return boolean_result([&] {
        require(final_veto != nullptr, "Missing final veto output");
        *final_veto = 0;
        auto* receiver = event_handler(id);
        require(!session_type(native_event_type(kind)) || isolated_handler(id),
                "Synthetic session events require an isolated owned standalone handler");
        auto event = synthetic_event(receiver, kind, event_id, text, length, value, can_veto, vetoed, logging_off);
        const bool handled = receiver->ProcessEvent(*event);
        *final_veto = event_vetoed(*event) ? 1 : 0;
        return handled;
    });
}
wxl_handle wxl_owned_event(int kind, int event_id, const char* text, size_t length,
                            int value, int can_veto, int vetoed, int logging_off) {
    return create([&] {
        auto event = synthetic_event(nullptr, kind, event_id, text, length, value, can_veto, vetoed, logging_off);
        const auto id = next_handle++;
        owned_events.emplace(id, std::move(event));
        return id;
    });
}
wxl_handle wxl_clone_event(uint64_t source, int contextual) {
    return create([&] {
        auto copy = detached_clone(contextual ? current_event(source) : owned_event(source));
        const auto id = next_handle++;
        owned_events.emplace(id, std::move(copy));
        return id;
    });
}
int wxl_event_alive(wxl_handle id) {
    return boolean_result([&] { ready(); return owned_events.count(id) != 0; });
}
int wxl_event_destroy(wxl_handle id) {
    return status([&] { owned_event(id); owned_events.erase(id); });
}
int wxl_event_field(uint64_t source, int contextual, int field, int64_t* value) {
    return status([&] {
        require(value != nullptr, "Missing event field output");
        *value = read_event_field(contextual ? current_event(source) : owned_event(source), field);
    });
}
int wxl_set_event_field(uint64_t source, int contextual, int field, int64_t value) {
    return status([&] { write_event_field(contextual ? current_event(source) : owned_event(source), field, value); });
}
const char* wxl_event_text(uint64_t source, int contextual, size_t* length) {
    return metadata_string([&] {
        auto& event = contextual ? current_event(source) : owned_event(source);
        auto* command = dynamic_cast<wxCommandEvent*>(&event);
        require(command != nullptr, "Text payload requires a command/notify event");
        return command->GetString();
    }, length);
}
int wxl_set_event_text(uint64_t source, int contextual, const char* text, size_t length) {
    return status([&] {
        auto& event = contextual ? current_event(source) : owned_event(source);
        auto* command = dynamic_cast<wxCommandEvent*>(&event);
        require(command != nullptr, "Text payload requires a command/notify event");
        if (event.GetEventType() == wxEVT_TEXT && event.GetEventObject()) {
            const auto origin = origin_id(event);
            require(origin != 0, "Cannot mutate text with an untracked native origin");
            const auto found = windows.find(origin);
            auto* window = found == windows.end() ? nullptr : found->second.get();
            require(!window || !dynamic_cast<wxSearchCtrl*>(window),
                    "Search-origin text-change payloads cannot be mutated");
            require(!window || !window->WXGetTextEntry(),
                    "Text-entry-origin text-change payloads are lazy and cannot be mutated");
        }
        require(!stc_payload_readonly(event.GetEventType()) && !input_payload_readonly(event.GetEventType()) && !aui_payload_readonly(event.GetEventType()) && !web_event_type(event.GetEventType()), "Web event text is read-only");
        require(text || length == 0, "Null event text");
        command->SetString(wxString::FromUTF8(text ? text : "", length));
    });
}
int wxl_owned_stop(wxl_handle id) {
    int level = 0;
    const auto result = status([&] { level = owned_event(id).StopPropagation(); });
    return result == 0 ? level : -1;
}
int wxl_dispatch_owned(wxl_handle receiver_id, wxl_handle event_id, int mode, int* final_veto) {
    return boolean_result([&] {
        require(final_veto != nullptr, "Missing final veto output");
        *final_veto = 0;
        require(mode >= 0 && mode <= 6, "Unknown owned-event delivery mode");
        auto* receiver = event_handler(receiver_id);
        auto& source = owned_event(event_id);
        require(!session_type(source.GetEventType()) || isolated_handler(receiver_id),
                "Synthetic session events require an isolated owned standalone handler");
        auto* receiver_window = dynamic_cast<wxWindow*>(receiver);
        require(source.GetEventType() != wxEVT_TEXT || !receiver_window || !receiver_window->WXGetTextEntry(),
                "Owned text-change dispatch to text-entry receivers would shadow the payload");
        if (mode == 3) {
            // All rejecting preflight checks precede the ownership handoff.
            // Native QueueEvent owns the actual event, not another clone.
            source.SetEventObject(receiver);
            auto event = std::move(owned_events.at(event_id));
            owned_events.erase(event_id);
            wxQueueEvent(receiver, event.release());
            return true;
        }
        wxWindow* owner = nullptr;
        if (mode >= 5) {
            owner = window(receiver_id);
            validate_window_stack(owner);
        }
        auto copy = detached_clone(source);
        copy->SetEventObject(receiver);
        if (mode == 1) { receiver->QueueEvent(copy.release()); return true; }
        if (mode == 2) { wxPostEvent(receiver, *copy); return true; }
        const bool handled = mode == 6 ? owner->ProcessWindowEventLocally(*copy)
                           : mode == 5 ? owner->ProcessWindowEvent(*copy)
                           : mode == 4 ? receiver->ProcessEventLocally(*copy) : receiver->ProcessEvent(*copy);
        *final_veto = event_vetoed(*copy) ? 1 : 0;
        return handled;
    });
}
int wxl_delete_pending_events(wxl_handle id) {
    return status([&] {
        ready();
        require_dispatch_entry(true);
        auto* receiver = event_handler(id);
        require(id != application_handle, "Cannot discard the application handler's deferred cleanup queue");
        receiver->DeletePendingEvents();
        if (const auto found = owned_handlers.find(id); found != owned_handlers.end())
            static_cast<StandaloneHandler*>(found->second.get())->ClearQueuedSessions();
        // DeletePendingEvents itself leaves the wxApp registration behind.
        // Remove it too, or a later app pump could invoke an empty-queue check.
        wxTheApp->RemovePendingEventHandler(receiver);
    });
}
int wxl_window_handler(wxl_handle window_id, wxl_handle* handler_id) {
    return status([&] {
        require(handler_id != nullptr, "Missing window handler output");
        auto* owner = window(window_id);
        validate_window_stack(owner);
        auto* head = owner->GetEventHandler();
        *handler_id = head == owner ? window_id : owned_handler_id(head);
    });
}
int wxl_push_handler(wxl_handle window_id, wxl_handle handler_id) {
    return status([&] {
        ready(); require_dispatch_entry(true);
        auto* owner = window(window_id);
        auto* handler = standalone_handler(handler_id);
        validate_window_stack(owner);
        require(handler->IsUnlinked(), "Unlink the handler before pushing it on a window");
        require(!static_cast<StandaloneHandler*>(handler)->HasQueuedSessions(), "Process/delete queued session events before pushing");
        for (const auto& item : bindings)
            require(item.second->source != handler_id || !session_type(native_event_type(item.second->kind)),
                    "Unbind session callbacks before pushing a handler");
        pushed_handlers.emplace(handler_id, PushedHandler{owner, window_id});
        owner->PushEventHandler(handler);
    });
}
int wxl_pop_handler(wxl_handle window_id, int destroy, wxl_handle* handler_id) {
    return status([&] {
        ready(); require_dispatch_entry(true);
        require(handler_id != nullptr, "Missing popped handler output");
        auto* owner = window(window_id);
        validate_window_stack(owner);
        require(owner->GetEventHandler() != owner, "No handler was pushed on this window");
        const auto id = owned_handler_id(owner->GetEventHandler());
        pushed_handlers.erase(id);
        if (destroy) {
            auto object = std::move(owned_handlers.at(id));
            owned_handlers.erase(id);
            object.release(); // Native Pop(true) now owns deletion.
            owner->PopEventHandler(true);
        } else owner->PopEventHandler(false);
        *handler_id = id;
    });
}
int wxl_remove_handler(wxl_handle window_id, wxl_handle handler_id) {
    return boolean_result([&] {
        ready(); require_dispatch_entry(true);
        auto* owner = window(window_id);
        auto* handler = event_handler(handler_id);
        require(owned_handlers.count(handler_id), "Window stacks require owned handlers");
        validate_window_stack(owner);
        const auto attached = pushed_handlers.find(handler_id);
        if (attached == pushed_handlers.end() || attached->second.window.get() != owner) return false;
        require(owner->RemoveEventHandler(handler), "Managed pushed handler not found");
        pushed_handlers.erase(attached);
        return true;
    });
}
wxl_handle wxl_frame(const char* title, int width, int height) {
    return create([&] {
        require(width > 0 && height > 0, "Frame dimensions must be positive");
        return track(new ManagedWindow<wxFrame>(nullptr, wxID_ANY, utf8(title), wxDefaultPosition, wxSize(width, height)));
    });
}
wxl_handle wxl_panel(wxl_handle parent) { return create([&] { return track(new ManagedWindow<wxPanel>(window(parent))); }); }
wxl_handle wxl_button(wxl_handle parent, const char* label) {
    return create([&] {
        auto* control = new ManagedWindow<wxButton>(window(parent), wxID_ANY, utf8(label));
        auto id = track(control);
        control->Bind(wxEVT_BUTTON, [id](wxCommandEvent& event) {
            emit_event(id, {WXL_EVENT_CLICK, nullptr, 0, -1, event.GetId(), 0, 0, event.GetEventType(), 0, 0, WXL_FAMILY_COMMAND, event.GetInt(), 0, event.GetTimestamp(), event.IsCommandEvent() ? 1 : 0, origin_id(event), static_cast<int>(event.GetEventCategory()), event.GetExtraLong(), event.IsSelection() ? 1 : 0, 0, 0});
            event.Skip();
        });
        return id;
    });
}
wxl_handle wxl_splitter(wxl_handle parent, int orientation, int position, int minimum, double gravity) {
    return create([&] {
        require(orientation == 0 || orientation == 1, "Invalid splitter orientation");
        splitter_pixel(position); splitter_pixel(minimum, true);
        require(std::isfinite(gravity) && gravity >= 0 && gravity <= 1, "Splitter gravity must be finite in [0,1]");
        auto splitter = std::make_unique<ManagedWindow<FixedSplitter>>(window(parent), wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
        splitter->SetSplitMode(orientation == 0 ? wxSPLIT_VERTICAL : wxSPLIT_HORIZONTAL);
        splitter->SetMinimumPaneSize(minimum); splitter->SetSashGravity(gravity); splitter->SetSashPosition(position); splitter->requested_position = position;
        const auto id = track(splitter.get()); splitter_owners.insert(splitter.get()); splitter.release(); return id;
    });
}
int wxl_splitter_attach(wxl_handle id, wxl_handle first_id, wxl_handle second_id) {
    return status([&] {
        ready(); require_dispatch_entry(true);
        auto* splitter = as<FixedSplitter>(id); auto* first = as<wxPanel>(first_id); auto* second = as<wxPanel>(second_id);
        require(!splitter->GetWindow1() && !splitter->GetWindow2(), "Splitter panes are already attached");
        require(first != second && first->GetParent() == splitter && second->GetParent() == splitter &&
                !first->GetContainingSizer() && !second->GetContainingSizer(), "Splitter needs distinct direct unsizered panel children");
        SplitterOperation operation(splitter, first, second);
        const auto position = splitter->requested_position;
        const bool success = splitter->GetSplitMode() == wxSPLIT_VERTICAL ? splitter->SplitVertically(first, second, position)
                                                                         : splitter->SplitHorizontally(first, second, position);
        require(success, "Unable to attach splitter panes");
    });
}
int wxl_splitter_pane(wxl_handle id, int index, wxl_handle* pane) {
    return status([&] {
        require(pane && (index == 0 || index == 1), "Invalid splitter pane query"); *pane = 0;
        auto* splitter = as<FixedSplitter>(id); auto* window = index == 0 ? splitter->GetWindow1() : splitter->GetWindow2();
        if (!window) return;
        require(!window_destruction_pending(window), "Pane destruction is pending");
        for (const auto& entry : windows) if (entry.second.get() == window) { *pane = entry.first; break; }
        require(*pane, "Unmanaged native splitter pane");
    });
}
int wxl_splitter_int(wxl_handle id, int field, int* result) {
    return status([&] {
        require(result && field >= 0 && field <= 3, "Invalid splitter integer query"); auto* splitter = as<FixedSplitter>(id);
        *result = field == 0 ? splitter->GetSashPosition() : field == 1 ? splitter->GetMinimumPaneSize() : field == 2 ?
                  (splitter->GetSplitMode() == wxSPLIT_VERTICAL ? 0 : 1) : (splitter->IsSplit() ? 1 : 0);
    });
}
int wxl_set_splitter_int(wxl_handle id, int field, int value) {
    return status([&] {
        require(field == 0 || field == 1, "Invalid mutable splitter integer property"); splitter_pixel(value, field == 1);
        auto* splitter = as<FixedSplitter>(id); SplitterOperation operation(splitter);
        if (field == 0) { splitter->requested_position = value; splitter->SetSashPosition(value); } else splitter->SetMinimumPaneSize(value);
    });
}
int wxl_splitter_gravity(wxl_handle id, double* gravity) {
    return status([&] { require(gravity, "Null splitter gravity output"); *gravity = as<FixedSplitter>(id)->GetSashGravity(); });
}
int wxl_set_splitter_gravity(wxl_handle id, double gravity) {
    return status([&] { require(std::isfinite(gravity) && gravity >= 0 && gravity <= 1, "Splitter gravity must be finite in [0,1]");
        auto* splitter = as<FixedSplitter>(id); SplitterOperation operation(splitter); splitter->SetSashGravity(gravity); });
}
int wxl_event_sash(uint64_t token, int* position) {
    return status([&] { require(position, "Null sash position output"); auto& event = current_event(token);
        require(dynamic_cast<wxSplitterEvent*>(&event) && sash_event(event), "Sash metadata requires a native sash event"); *position = event_sash_value(event); });
}
int wxl_set_event_sash(uint64_t token, int position) {
    return status([&] { require(position == -1 || (position >= 0 && position <= splitter_pixel_limit), "Event sash position out of supported range");
        auto& event = current_event(token); auto* splitter = dynamic_cast<wxSplitterEvent*>(&event);
        require(splitter && event.GetEventType() == wxEVT_SPLITTER_SASH_POS_CHANGING, "Only sash-changing contexts can edit proposed position");
        splitter->SetSashPosition(position); });
}
wxl_handle wxl_tree(wxl_handle parent) {
    return create([&]() -> wxl_handle {
#if defined(__WXQT__)
        (void)parent;
        throw std::runtime_error("Tree controls are unavailable on wxQt: asynchronous item events need generation-safe translation");
#else
        auto tree = std::make_unique<ManagedWindow<wxTreeCtrl>>(window(parent), wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                                             wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_SINGLE);
        const auto id = track(tree.get());
        tree_owners.emplace(tree.get(), id);
        tree->Bind(wxEVT_TREE_DELETE_ITEM, [id](wxTreeEvent& event) {
            const auto found = windows.find(id);
            if (found != windows.end()) invalidate_tree_subtree(tree_token(found->second.get(), event.GetItem()));
            event.Skip();
        });
        tree.release(); return id;
#endif
    });
}
int wxl_tree_add(wxl_handle owner, wxl_handle parent, const char* value, size_t length, wxl_handle* result) {
    return status([&] {
        require(result != nullptr, "Null tree item output"); *result = 0;
        auto* tree = as<wxTreeCtrl>(owner);
        TreeOperation operation(tree);
        const auto text = text_value(value, length);
        wxTreeItemId native_parent;
        if (parent) { const auto& node = tree_node(parent); require(node.owner == owner, "Item belongs to another tree"); native_parent = node.native; }
        else require(!tree->GetRootItem().IsOk(), "Tree already has a root");
        wxEventBlocker changing(tree, wxEVT_TREE_SEL_CHANGING), changed(tree, wxEVT_TREE_SEL_CHANGED);
        const auto selected = tree->GetSelection();
        const auto native = parent ? tree->AppendItem(native_parent, text) : tree->AddRoot(text);
        require(native.IsOk(), "Unable to create tree item");
        const auto id = next_handle++;
        try {
            tree_nodes.emplace(id, TreeNode{owner, parent, tree, native, {}});
            if (parent) tree_nodes.at(parent).children.insert(id);
            tree->SelectItem(parent && selected.IsOk() ? selected : tree->GetRootItem());
        } catch (...) { if (parent && tree_nodes.count(parent)) tree_nodes.at(parent).children.erase(id); tree_nodes.erase(id); tree->Delete(native); throw; }
        *result = id;
    });
}
int wxl_tree_query(wxl_handle owner, int field, wxl_handle* result) {
    return status([&] {
        require(result && (field == 0 || field == 1), "Invalid tree query");
        auto* tree = as<wxTreeCtrl>(owner);
        const auto native = field == 0 ? tree->GetRootItem() : tree->GetSelection();
        *result = tree_token(tree, native);
        require(!native.IsOk() || *result, "Unmanaged native tree item");
    });
}
int wxl_tree_relative(wxl_handle id, int field, wxl_handle* result) {
    return status([&] {
        require(result && field >= 0 && field <= 2, "Invalid tree relative query");
        const auto& node = tree_node(id);
        auto* tree = as<wxTreeCtrl>(node.owner);
        wxTreeItemIdValue cookie;
        const auto native = field == 0 ? tree->GetItemParent(node.native) : field == 1 ? tree->GetFirstChild(node.native, cookie) : node.parent ? tree->GetNextSibling(node.native) : wxTreeItemId();
        *result = tree_token(tree, native);
        require(!native.IsOk() || *result, "Unmanaged native tree item");
    });
}
const char* wxl_tree_text(wxl_handle id, size_t* length) {
    return metadata_string([&] { const auto& node = tree_node(id); return as<wxTreeCtrl>(node.owner)->GetItemText(node.native); }, length);
}
int wxl_set_tree_text(wxl_handle id, const char* value, size_t length) {
    return status([&] {
        const auto& node = tree_node(id); auto* tree = as<wxTreeCtrl>(node.owner); TreeOperation operation(tree);
        const auto text = text_value(value, length);
        wxEventBlocker changing(tree, wxEVT_TREE_SEL_CHANGING), changed(tree, wxEVT_TREE_SEL_CHANGED);
        tree->SetItemText(node.native, text);
    });
}
int wxl_tree_expanded(wxl_handle id) { return boolean_result([&] { const auto& node = tree_node(id); return as<wxTreeCtrl>(node.owner)->IsExpanded(node.native); }); }
int wxl_set_tree_expanded(wxl_handle id, int expanded) {
    return status([&] {
        require(expanded == 0 || expanded == 1, "Invalid tree expansion flag");
        const auto& node = tree_node(id); auto* tree = as<wxTreeCtrl>(node.owner); TreeOperation operation(tree);
        wxEventBlocker changing(tree, wxEVT_TREE_SEL_CHANGING), changed(tree, wxEVT_TREE_SEL_CHANGED);
        auto selected = tree->GetSelection();
        auto cursor = tree_token(tree, selected);
        while (!expanded && cursor && tree_nodes.count(cursor)) {
            if (cursor == id) { selected = node.native; break; }
            cursor = tree_nodes.at(cursor).parent;
        }
        if (expanded) tree->Expand(node.native); else tree->Collapse(node.native);
        tree->SelectItem(selected.IsOk() ? selected : tree->GetRootItem());
    });
}
int wxl_tree_delete(wxl_handle id) {
    return status([&] {
        const auto& node = tree_node(id); auto* tree = as<wxTreeCtrl>(node.owner); const auto native = node.native;
        TreeOperation operation(tree);
        wxEventBlocker changing(tree, wxEVT_TREE_SEL_CHANGING), changed(tree, wxEVT_TREE_SEL_CHANGED);
        const auto selected = tree->GetSelection();
        const auto selected_token = tree_token(tree, selected);
        const auto parent = node.parent;
        invalidate_tree_subtree(id);
        tree->Delete(native);
        if (selected_token && tree_nodes.count(selected_token)) tree->SelectItem(selected);
        else if (parent && tree_nodes.count(parent)) tree->SelectItem(tree_nodes.at(parent).native);
        else if (tree->GetRootItem().IsOk()) tree->SelectItem(tree->GetRootItem());
        else tree->Unselect();
    });
}
int wxl_tree_select(wxl_handle owner, wxl_handle item, int notify) {
    return status([&] {
        require(notify == 0 || notify == 1, "Invalid tree notification flag");
        auto* tree = as<wxTreeCtrl>(owner); TreeOperation operation(tree);
        require(item || !tree->GetRootItem().IsOk(), "Nonempty trees require a selected item");
        if (!item) return;
        const auto& node = tree_node(item); require(node.owner == owner, "Item belongs to another tree");
        const auto native = node.native;
        auto change = [&] { tree->SelectItem(native); };
        if (notify) change(); else { wxEventBlocker changing(tree, wxEVT_TREE_SEL_CHANGING), changed(tree, wxEVT_TREE_SEL_CHANGED); change(); }
    });
}
int wxl_tree_item_alive(wxl_handle id) {
    return boolean_result([&] {
        ready(); const auto node = tree_nodes.find(id); if (node == tree_nodes.end()) return false;
        const auto owner = windows.find(node->second.owner);
        return owner != windows.end() && owner->second.get() && !window_destruction_pending(owner->second.get());
    });
}
int wxl_tree_revision(uint64_t* result) { return status([&] { ready(); require(result != nullptr, "Null tree revision output"); *result = tree_revision; }); }
int wxl_event_tree_item(uint64_t source, int contextual, wxl_handle* result) {
    return status([&] {
        require(result, "Null event tree item output");
        auto& event = contextual ? current_event(source) : owned_event(source);
        require(dynamic_cast<wxTreeEvent*>(&event) != nullptr, "Tree item metadata requires a native tree event");
        *result = event_tree_token(event);
    });
}
wxl_handle wxl_list_box(wxl_handle parent, const char** items, const size_t* lengths, size_t count, int selection) {
    return create([&] {
        require(count <= static_cast<size_t>(std::numeric_limits<int>::max()), "Too many list items");
        require(!count || (items && lengths), "Null list item arrays");
        require(selection >= -1 && (selection == -1 || static_cast<size_t>(selection) < count), "List selection out of bounds");
        wxArrayString strings;
        for (size_t i = 0; i < count; ++i) strings.Add(text_value(items[i], lengths[i]));
        auto control = std::make_unique<ManagedWindow<wxListBox>>(window(parent), wxID_ANY, wxDefaultPosition, wxDefaultSize, strings, wxLB_SINGLE);
        { wxEventBlocker blocker(control.get(), wxEVT_LISTBOX); control->SetSelection(selection); }
        const auto id = track(control.get());
        control->Bind(wxEVT_LISTBOX, [id](wxCommandEvent& event) {
            ListCallbackOperation list_guard(event);
            if (!list_guard.allow) { event.Skip(); return; }
            const auto text = event.GetString();
            const auto bytes = text.ToUTF8();
            emit_event(id, {WXL_EVENT_LISTBOX_SELECTION, bytes.data() ? bytes.data() : "", 0, event.GetSelection(), event.GetId(), bytes.length(), 0,
                           event.GetEventType(), 0, 0, WXL_FAMILY_COMMAND, event.GetInt(), 0, event.GetTimestamp(), event.IsCommandEvent() ? 1 : 0,
                           origin_id(event), static_cast<int>(event.GetEventCategory()), event.GetExtraLong(), event.IsSelection() ? 1 : 0, 0, 0});
            event.Skip();
        });
        control.release(); return id;
    });
}
int wxl_list_count(wxl_handle id) {
    int count = -1;
    status([&] { const auto n = as<wxListBox>(id)->GetCount(); require(n <= static_cast<unsigned>(std::numeric_limits<int>::max()), "Too many list items"); count = static_cast<int>(n); });
    return count;
}
const char* wxl_list_item(wxl_handle id, int index, size_t* length) {
    return metadata_string([&] {
        auto* list = as<wxListBox>(id);
        require(index >= 0 && static_cast<unsigned>(index) < list->GetCount(), "List item index out of bounds");
        return list->GetString(static_cast<unsigned>(index));
    }, length);
}
int wxl_set_list_item(wxl_handle id, int index, const char* value, size_t length) {
    return status([&] {
        auto* list = as<wxListBox>(id);
        require(index >= 0 && static_cast<unsigned>(index) < list->GetCount(), "List item index out of bounds");
        const auto text = text_value(value, length);
        const auto selected = list->GetSelection();
        wxEventBlocker blocker(list, wxEVT_LISTBOX);
        list->SetString(static_cast<unsigned>(index), text);
        list->SetSelection(selected);
    });
}
int wxl_list_append(wxl_handle id, const char* value, size_t length) {
    int index = -1;
    const int result = status([&] {
        auto* list = as<wxListBox>(id);
        require(list->GetCount() < static_cast<unsigned>(std::numeric_limits<int>::max()), "Too many list items");
        const auto text = text_value(value, length);
        const auto selected = list->GetSelection();
        wxEventBlocker blocker(list, wxEVT_LISTBOX);
        index = list->Append(text);
        require(index != wxNOT_FOUND, "Unable to append list item");
        list->SetSelection(selected);
    });
    return result == 0 ? index : -1;
}
int wxl_list_delete(wxl_handle id, int index) {
    return status([&] {
        auto* list = as<wxListBox>(id);
        require(index >= 0 && static_cast<unsigned>(index) < list->GetCount(), "List item index out of bounds");
        const auto selected = list->GetSelection();
        wxEventBlocker blocker(list, wxEVT_LISTBOX);
        list->Delete(static_cast<unsigned>(index));
        list->SetSelection(selected == index ? wxNOT_FOUND : selected > index ? selected - 1 : selected);
    });
}
int wxl_list_clear(wxl_handle id) {
    return status([&] { auto* list = as<wxListBox>(id); wxEventBlocker blocker(list, wxEVT_LISTBOX); list->Clear(); });
}
int wxl_list_selection(wxl_handle id) {
    int selection = -2; status([&] { selection = as<wxListBox>(id)->GetSelection(); }); return selection;
}
int wxl_set_list_selection(wxl_handle id, int selection, int notify) {
    return status([&] {
        auto* list = as<wxListBox>(id);
        require(notify == 0 || notify == 1, "Invalid list notification flag");
        require(selection >= -1 && (selection == -1 || static_cast<unsigned>(selection) < list->GetCount()), "List selection out of bounds");
        { wxEventBlocker blocker(list, wxEVT_LISTBOX); list->SetSelection(selection); }
        if (notify) {
            wxCommandEvent event(wxEVT_LISTBOX, list->GetId());
            event.SetEventObject(list); event.SetInt(selection); event.SetExtraLong(selection >= 0 ? 1 : 0);
            if (selection >= 0) event.SetString(list->GetString(static_cast<unsigned>(selection)));
            list->GetEventHandler()->ProcessEvent(event);
        }
    });
}
int wxl_on_list_selection(wxl_handle id, uint64_t token) { return status([&] { as<wxListBox>(id); handlers[id] = token; }); }
int wxl_off_list_selection(wxl_handle id) { return status([&] { as<wxListBox>(id); handlers.erase(id); }); }
wxl_handle wxl_notebook(wxl_handle parent) {
    return create([&] { return track(new ManagedWindow<wxNotebook>(window(parent), wxID_ANY)); });
}
int wxl_notebook_add(wxl_handle id, wxl_handle page_id, const char* label, size_t length, int select) {
    return status([&] {
        ready();
        require_dispatch_entry(true);
        auto* book = as<wxNotebook>(id);
        auto* page = as<wxPanel>(page_id);
        require(select == 0 || select == 1, "Invalid notebook select flag");
        require(page->GetParent() == book && page->GetContainingSizer() == nullptr, "Page must be a direct unsizered panel child of the notebook");
        require(book->FindPage(page) == wxNOT_FOUND, "Page is already attached");
        require(book->GetPageCount() < static_cast<size_t>(std::numeric_limits<int>::max()), "Too many notebook pages");
        const auto caption = menu_string(label, length, true);
        NotebookOperation operation(book);
        require(book->AddPage(page, caption, select != 0), "Unable to add notebook page");
    });
}
int wxl_notebook_count(wxl_handle id) {
    int count = -1;
    status([&] { const auto n = as<wxNotebook>(id)->GetPageCount(); require(n <= static_cast<size_t>(std::numeric_limits<int>::max()), "Too many notebook pages"); count = static_cast<int>(n); });
    return count;
}
wxl_handle wxl_notebook_page(wxl_handle id, int index) {
    wxl_handle result = 0;
    status([&] {
        auto* book = as<wxNotebook>(id);
        require(index >= 0 && static_cast<size_t>(index) < book->GetPageCount(), "Notebook page index out of bounds");
        auto* page = book->GetPage(static_cast<size_t>(index));
        require(!window_destruction_pending(page), "Page destruction is pending");
        for (const auto& item : windows) if (item.second.get() == page) { result = item.first; break; }
        require(result != 0, "Unmanaged notebook page");
    });
    return result;
}
const char* wxl_notebook_label(wxl_handle id, int index, size_t* length) {
    return metadata_string([&] {
        auto* book = as<wxNotebook>(id);
        require(index >= 0 && static_cast<size_t>(index) < book->GetPageCount(), "Notebook page index out of bounds");
        return book->GetPageText(static_cast<size_t>(index));
    }, length);
}
int wxl_set_notebook_label(wxl_handle id, int index, const char* label, size_t length) {
    return status([&] {
        auto* book = as<wxNotebook>(id);
        require(index >= 0 && static_cast<size_t>(index) < book->GetPageCount(), "Notebook page index out of bounds");
        require(book->SetPageText(static_cast<size_t>(index), menu_string(label, length, true)), "Unable to set notebook page label");
    });
}
int wxl_notebook_selection(wxl_handle id) {
    int result = -2;
    status([&] { result = as<wxNotebook>(id)->GetSelection(); });
    return result;
}
int wxl_set_notebook_selection(wxl_handle id, int index, int notify) {
    return status([&] {
        auto* book = as<wxNotebook>(id);
        require(notify == 0 || notify == 1, "Invalid notebook notification flag");
        require(index >= 0 && static_cast<size_t>(index) < book->GetPageCount(), "Notebook selection out of bounds");
        NotebookOperation operation(book);
        if (notify) book->SetSelection(static_cast<size_t>(index));
        else book->ChangeSelection(static_cast<size_t>(index));
    });
}
wxl_handle wxl_text(wxl_handle parent, const char* value, size_t length, int multiline, int editable) {
    return create([&] {
        require((multiline == 0 || multiline == 1) && (editable == 0 || editable == 1), "Invalid text control flags");
        const auto text = text_value(value, length);
        auto* control = new ManagedWindow<wxTextCtrl>(window(parent), wxID_ANY, text, wxDefaultPosition,
                                                    wxDefaultSize, (multiline ? wxTE_MULTILINE : 0) |
                                                    (editable ? 0 : wxTE_READONLY));
        const auto id = track(control);
        control->Bind(wxEVT_TEXT, [id](wxCommandEvent& event) {
            const auto it = handlers.find(id);
            if (it != handlers.end() && dispatch && !pending_destruction.count(id)) {
                const auto token = it->second;
                // Both objects must survive reentrant callbacks and conversion.
                const auto text = event.GetString();
                const auto bytes = text.ToUTF8();
                const wxl_event payload{WXL_EVENT_TEXT_CHANGE, bytes.data() ? bytes.data() : "", 0, -1, event.GetId(), bytes.length(), 0, event.GetEventType(), 0, 0, WXL_FAMILY_COMMAND, event.GetInt(), 0, event.GetTimestamp(), event.IsCommandEvent() ? 1 : 0, origin_id(event), static_cast<int>(event.GetEventCategory()), event.GetExtraLong(), event.IsSelection() ? 1 : 0, 0, 0};
                invoke_callback(token, payload);
            }
            event.Skip();
        });
        return id;
    });
}
wxl_handle wxl_label(wxl_handle parent, const char* label) {
    return create([&] { return track(new ManagedWindow<wxStaticText>(window(parent), wxID_ANY, utf8(label))); });
}
int wxl_set_label(wxl_handle id, const char* value) {
    return status([&] { as<wxStaticText>(id)->SetLabel(utf8(value)); });
}
const char* wxl_get_label(wxl_handle id) {
    const int result = status([&] {
        const auto text = as<wxStaticText>(id)->GetLabel();
        const auto bytes = text.ToUTF8();
        label_result.assign(bytes.data() ? bytes.data() : "", bytes.length());
    });
    return result == 0 ? label_result.c_str() : nullptr;
}
wxString search_value(const char* value, size_t length) {
    const auto text=text_value(value,length);
    require(text.find('\r')==wxString::npos && text.find('\n')==wxString::npos, "Search text/hint must be single-line");
    return text;
}
wxl_handle wxl_search_control(wxl_handle parent_id, const char* value, size_t length, const char* hint, size_t hint_length) {
    return create([&] {
        auto* parent=window(parent_id); const auto initial=search_value(value,length), placeholder=search_value(hint,hint_length);
        struct CreationScope { CreationScope() { ++input_creation_depth; } ~CreationScope() { --input_creation_depth; } } creation;
        auto object=std::make_unique<ManagedWindow<wxSearchCtrl>>();
        { wxEventBlocker blocker(object.get());
          require(object->Create(parent,wxID_ANY,wxEmptyString,wxDefaultPosition,wxDefaultSize,wxTE_PROCESS_ENTER),"Native search control creation failed");
          object->ShowSearchButton(true); object->ShowCancelButton(true);
          object->SetDescriptiveText(placeholder); object->ChangeValue(initial); }
        const auto id=track(object.get()); input_owners.emplace(object.get(),false); object.release(); return id;
    });
}
const char* wxl_search_field(wxl_handle id, int field, size_t* length) {
    return metadata_string([&] {
        auto* object=as<wxSearchCtrl>(id); require(field==0 || field==1,"Invalid search field");
        return field==0 ? object->GetValue() : object->GetDescriptiveText();
    },length);
}
int wxl_set_search_field(wxl_handle id, int field, const char* value, size_t length, int notify) { return status([&] {
    auto* object=as<wxSearchCtrl>(id); const auto text=search_value(value,length);
    require((field==0 || field==1) && (notify==0 || notify==1) && (field==0 || !notify),"Invalid search field/notification");
    InputOperation operation(object);
    { wxEventBlocker blocker(object); if (field==0) object->ChangeValue(text); else object->SetDescriptiveText(text); }
    if (notify) { wxCommandEvent event(wxEVT_TEXT,object->GetId()); event.SetEventObject(object); event.SetString(object->GetValue()); object->HandleWindowEvent(event); }
}); }
wxl_handle wxl_time_picker(wxl_handle parent_id, int hour, int minute, int second) {
    return create([&] {
        auto* parent=window(parent_id); const auto value=clock_value(hour,minute,second);
        struct CreationScope { CreationScope() { ++input_creation_depth; } ~CreationScope() { --input_creation_depth; } } creation;
        auto object=std::make_unique<ManagedWindow<wxTimePickerCtrl>>();
        { wxEventBlocker blocker(object.get());
          require(object->Create(parent,wxID_ANY,value,wxDefaultPosition,wxDefaultSize,wxTP_DEFAULT), "Native time picker creation failed");
          object->SetValue(value);
          require(clock_code(object->GetValue())==clock_code(value), "Native time picker did not accept the initial time"); }
        const auto id=track(object.get()); input_owners.emplace(object.get(),false); object.release(); return id;
    });
}
int wxl_clock_time(wxl_handle id, int* code) { return status([&] {
    require(code,"Missing clock time output");
    const auto result=clock_code(as<wxTimePickerCtrl>(id)->GetValue());
    require(result!=0,"Native time picker has no supported committed time"); *code=result;
}); }
int wxl_set_clock_time(wxl_handle id, int hour, int minute, int second, int notify) { return status([&] {
    auto* object=as<wxTimePickerCtrl>(id); const auto value=clock_value(hour,minute,second);
    require(notify==0 || notify==1,"Invalid time notification flag"); InputOperation operation(object);
    { wxEventBlocker blocker(object); object->SetValue(value); }
    const auto accepted=object->GetValue();
    require(clock_code(accepted)==clock_code(value),"Native time picker did not accept the time");
    if (notify) { wxDateEvent event(object,accepted,wxEVT_TIME_CHANGED); object->HandleWindowEvent(event); }
}); }
wxl_handle wxl_date_picker(wxl_handle parent_id, int year, int month, int day) {
    return create([&] {
        auto* parent=window(parent_id); const auto value=calendar_value(year,month,day);
        const auto lower=calendar_value(1900,1,1), upper=calendar_value(2100,12,31);
        struct CreationScope { CreationScope() { ++input_creation_depth; } ~CreationScope() { --input_creation_depth; } } creation;
        auto object=std::make_unique<ManagedWindow<wxDatePickerCtrl>>();
        { wxEventBlocker blocker(object.get());
          require(object->Create(parent,wxID_ANY,value,wxDefaultPosition,wxDefaultSize,wxDP_DEFAULT|wxDP_SHOWCENTURY), "Native date picker creation failed");
          object->SetRange(lower,upper);
          wxDateTime actual_lower,actual_upper;
          require(object->GetRange(&actual_lower,&actual_upper) && calendar_code(actual_lower)==19000101 && calendar_code(actual_upper)==21001231,
                  "Native date picker did not accept the supported range");
          object->SetValue(value);
          require(calendar_code(object->GetValue())==calendar_code(value), "Native date picker did not accept the initial date"); }
        const auto id=track(object.get()); input_owners.emplace(object.get(),false); object.release(); return id;
    });
}
int wxl_calendar_date(wxl_handle id, int* code) { return status([&] {
    require(code, "Missing calendar date output");
    const auto result=calendar_code(as<wxDatePickerCtrl>(id)->GetValue());
    require(result!=0,"Native date picker has no supported committed date"); *code=result;
}); }
int wxl_set_calendar_date(wxl_handle id, int year, int month, int day, int notify) { return status([&] {
    auto* object=as<wxDatePickerCtrl>(id); const auto value=calendar_value(year,month,day);
    require(notify==0 || notify==1,"Invalid date notification flag"); InputOperation operation(object);
    { wxEventBlocker blocker(object); object->SetValue(value); }
    const auto accepted=object->GetValue();
    require(calendar_code(accepted)==calendar_code(value),"Native date picker did not accept the date");
    if (notify) { wxDateEvent event(object,accepted,wxEVT_DATE_CHANGED); object->HandleWindowEvent(event); }
}); }
// Tooltip storage belongs to wxWindow; only copied UTF-8 text crosses the ABI.
const char* wxl_tooltip_text(wxl_handle id, size_t* length, int* present) {
    const int result = status([&] {
        require(length && present, "Missing tooltip output");
        auto* object = window(id);
        *present = object->GetToolTip() ? 1 : 0;
        const auto text = object->GetToolTipText();
        const auto bytes = text.ToUTF8();
        metadata_result.assign(bytes.data() ? bytes.data() : "", bytes.length());
        *length = metadata_result.size();
    });
    return result == 0 ? metadata_result.c_str() : nullptr;
}
int wxl_set_tooltip_text(wxl_handle id, const char* value, size_t length, int present) {
    return status([&] {
        auto* object = window(id);
        require(present == 0 || present == 1, "Invalid tooltip presence flag");
        require(length <= 4096, "Tooltip text supports at most4096 UTF-8 bytes");
        require(present || length == 0, "Removed tooltip cannot carry text");
        const auto text = text_value(value, length);
        require(callback_depth == 0, "Defer tooltip mutation until callbacks return");
        InputOperation operation(object);
        if (present) object->SetToolTip(text); else object->UnsetToolTip();
    });
}
// Focus can synchronously deliver native focus/navigation events. Keep the entire
// session's window graph alive, and share the input native-entry gates.
struct FocusOperation {
    InputOperation input;
    explicit FocusOperation(wxWindow* object) : input(object) {
        require(callback_depth == 0 && focus_operation_depth == 0,
                "Defer focus navigation until callbacks/operations return");
        require(object->IsShownOnScreen() && object->IsEnabled(),
                "Focus navigation requires a shown, enabled window");
        ++focus_operation_depth;
    }
    ~FocusOperation() { --focus_operation_depth; }
};
int wxl_focus_window(wxl_handle id, int keyboard) { return status([&] {
    require(keyboard == 0 || keyboard == 1, "Invalid keyboard focus flag");
    auto* object = window(id); FocusOperation operation(object);
    if (keyboard) object->SetFocusFromKbd(); else object->SetFocus();
}); }
int wxl_navigate_focus(wxl_handle id, int forward) { return boolean_result([&] {
    require(forward == 0 || forward == 1, "Invalid focus direction");
    auto* object = window(id); FocusOperation operation(object);
    return object->Navigate(forward ? wxNavigationKeyEvent::IsForward : wxNavigationKeyEvent::IsBackward);
}); }
constexpr int scroll_extent_limit = 1000000;
void scroll_bounds(wxScrolledWindow* object) {
    int x=0,y=0; object->GetVirtualSize(&x,&y);
    const auto client=object->GetClientSize();
    require(x>=0 && y>=0 && x<=scroll_extent_limit && y<=scroll_extent_limit &&
            client.x<=scroll_extent_limit && client.y<=scroll_extent_limit,"Scroll extent exceeds supported pixel bounds");
    if (auto* sizer=object->GetSizer()) {
        const auto minimum=sizer->GetMinSize();
        require(minimum.x>=0 && minimum.y>=0 && minimum.x<=scroll_extent_limit && minimum.y<=scroll_extent_limit,"Scroll content exceeds supported pixel bounds");
    }
}
void scroll_mutation_ready(wxScrolledWindow* object) {
    require(callback_depth==0,"Defer scroll mutation until callbacks return"); scroll_bounds(object);
}
wxl_handle wxl_scrolled_panel(wxl_handle parent_id,int xrate,int yrate) {
    return create([&] {
        auto* parent=window(parent_id); require(xrate>=0 && xrate<=256 && yrate>=0 && yrate<=256,"Scroll rate requires 0..256 pixels per unit");
        auto object=std::make_unique<ManagedWindow<wxScrolledWindow>>();
        { wxEventBlocker blocker(object.get());
          require(object->Create(parent,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxHSCROLL|wxVSCROLL|wxTAB_TRAVERSAL),"Native scrolled panel creation failed");
          object->SetScrollRate(xrate,yrate); }
        const auto id=track(object.get()); object.release(); return id;
    });
}
int wxl_scroll_pair(wxl_handle id,int field,int* x,int* y) { return status([&] {
    auto* object=as<wxScrolledWindow>(id); require(x && y,"Missing scroll pair output");
    if (field==0) object->GetScrollPixelsPerUnit(x,y);
    else if (field==1) object->GetViewStart(x,y);
    else { require(field==2,"Invalid scroll field"); object->GetVirtualSize(x,y); }
}); }
int wxl_set_scroll_pair(wxl_handle id,int field,int x,int y) { return status([&] {
    auto* object=as<wxScrolledWindow>(id); require(field==0 || field==1,"Invalid scroll property");
    const int limit=field==0 ? 256 : 1000000;
    require(x>=0 && y>=0 && x<=limit && y<=limit,"Scroll property outside supported bounds");
    scroll_mutation_ready(object); InputOperation operation(object);
    if (field==0) object->SetScrollRate(x,y); else object->Scroll(x,y);
}); }
int wxl_fit_scroll_content(wxl_handle id) { return status([&] {
    auto* object=as<wxScrolledWindow>(id); require(object->GetSizer(),"Attach a layout before fitting scroll content");
    scroll_mutation_ready(object); InputOperation operation(object);
    object->Layout(); object->FitInside(); object->Layout();
}); }
wxString combo_value(const char* value, size_t length) {
    const auto text=text_value(value,length);
    require(text.find('\r')==wxString::npos && text.find('\n')==wxString::npos, "Combo text must be single-line"); return text;
}
wxl_handle wxl_combo_box(wxl_handle parent_id, const char** items, const size_t* lengths, size_t count,
                         const char* value, size_t value_length, int selection) {
    return create([&] {
        auto* parent = window(parent_id);
        require(count <= 256 && (!count || (items && lengths)), "Combo box supports 0..256 choices");
        require(selection >= -1 && (selection == -1 || static_cast<size_t>(selection) < count), "Combo selection out of bounds");
        const auto initial = combo_value(value, value_length);
        require(selection == -1 || initial.empty(), "Use initial text or selection, not both");
        wxArrayString strings;
        for (size_t i=0; i<count; ++i) {
            const auto item = combo_value(items[i], lengths[i]); require(!item.empty(), "Combo choices must be nonempty");
            for (const auto& old : strings) require(old.CmpNoCase(item) != 0, "Combo choices must be case-insensitively unique");
            strings.Add(item);
        }
        struct CreationScope { CreationScope() { ++input_creation_depth; } ~CreationScope() { --input_creation_depth; } } creation;
        auto object = std::make_unique<ManagedWindow<wxComboBox>>();
        { wxEventBlocker blocker(object.get());
          require(object->Create(parent,wxID_ANY,wxEmptyString,wxDefaultPosition,wxDefaultSize,strings,wxCB_DROPDOWN), "Native combo creation failed");
          object->SetSelection(wxNOT_FOUND);
          object->ChangeValue(initial);
          if (selection >= 0) { object->ChangeValue(strings[selection]); object->SetSelection(selection); } }
        const auto id=track(object.get()); input_owners.emplace(object.get(),false); object.release(); return id;
    });
}
int wxl_combo_field(wxl_handle id, int field) {
    int result=-2; status([&] { auto* object=as<wxComboBox>(id); require(field==0 || field==1,"Invalid combo field");
        result=field==0 ? object->GetSelection() : static_cast<int>(object->GetCount()); }); return result;
}
const char* wxl_combo_text(wxl_handle id, size_t* length) {
    return metadata_string([&] { return as<wxComboBox>(id)->GetValue(); },length);
}
const char* wxl_combo_item(wxl_handle id, int index, size_t* length) {
    return metadata_string([&] { auto* object=as<wxComboBox>(id);
        require(index>=0 && static_cast<unsigned>(index)<object->GetCount(),"Combo item index out of bounds");
        return object->GetString(index); },length);
}
int wxl_set_combo_text(wxl_handle id, const char* value, size_t length, int notify) { return status([&] {
    auto* object=as<wxComboBox>(id); const auto text=combo_value(value,length); require(notify==0 || notify==1,"Invalid combo notification");
    InputOperation operation(object);
    { wxEventBlocker blocker(object); object->SetSelection(wxNOT_FOUND); object->ChangeValue(text); }
    if (notify) { wxCommandEvent event(wxEVT_TEXT,object->GetId()); event.SetEventObject(object); event.SetString(object->GetValue()); object->HandleWindowEvent(event); }
}); }
int wxl_set_combo_selection(wxl_handle id, int index, int notify) { return status([&] {
    auto* object=as<wxComboBox>(id);
    require(index>=-1 && (index==-1 || static_cast<unsigned>(index)<object->GetCount()) && (notify==0 || notify==1),"Invalid combo selection/notification");
    InputOperation operation(object);
    { wxEventBlocker blocker(object);
      if (index>=0) object->ChangeValue(object->GetString(index));
      object->SetSelection(index); if (index==-1) object->ChangeValue(wxEmptyString); }
    if (notify) { wxCommandEvent event(wxEVT_COMBOBOX,object->GetId()); event.SetEventObject(object); event.SetInt(index);
        event.SetString(object->GetValue()); object->HandleWindowEvent(event); }
}); }
wxl_handle wxl_toggle_button(wxl_handle parent_id, const char* label, int checked) {
    return create([&] {
        auto* parent = window(parent_id); require(checked == 0 || checked == 1, "Invalid toggle state");
        struct CreationScope { CreationScope() { ++input_creation_depth; } ~CreationScope() { --input_creation_depth; } } creation;
        auto object = std::make_unique<ManagedWindow<wxToggleButton>>();
        { wxEventBlocker blocker(object.get());
          require(object->Create(parent, wxID_ANY, utf8(label)), "Native toggle button creation failed");
          object->SetValue(checked != 0); }
        const auto id = track(object.get()); input_owners.emplace(object.get(), false); object.release(); return id;
    });
}
int wxl_toggle_button_checked(wxl_handle id) { return boolean_result([&] { return as<wxToggleButton>(id)->GetValue(); }); }
int wxl_set_toggle_button_checked(wxl_handle id, int checked, int notify) { return status([&] {
    auto* object = as<wxToggleButton>(id);
    require((checked == 0 || checked == 1) && (notify == 0 || notify == 1), "Invalid toggle state/notification");
    InputOperation operation(object);
    { wxEventBlocker blocker(object); object->SetValue(checked != 0); }
    if (notify) { wxCommandEvent event(wxEVT_TOGGLEBUTTON, object->GetId()); event.SetEventObject(object);
        event.SetInt(checked); object->HandleWindowEvent(event); }
}); }
const char* wxl_toggle_button_label(wxl_handle id) {
    const int result = status([&] { const auto text = as<wxToggleButton>(id)->GetLabel();
        const auto bytes = text.ToUTF8(); label_result.assign(bytes.data() ? bytes.data() : "", bytes.length()); });
    return result == 0 ? label_result.c_str() : nullptr;
}
int wxl_set_toggle_button_label(wxl_handle id, const char* label) { return status([&] {
    auto* object = as<wxToggleButton>(id); InputOperation operation(object); wxEventBlocker blocker(object);
    object->SetLabel(utf8(label));
}); }
wxl_handle wxl_radio_box(wxl_handle parent_id, const char* label, const char** items, int count, int selection, int horizontal) {
    return create([&] {
        auto* parent = window(parent_id);
        require(items && count >= 2 && count <= 64, "Radio box requires 2..64 choices");
        require(selection >= 0 && selection < count && (horizontal == 0 || horizontal == 1), "Invalid radio selection/orientation");
        wxArrayString strings;
        for (int i = 0; i < count; ++i) { require(items[i], "Missing radio choice"); strings.Add(utf8(items[i])); }
        struct CreationScope { CreationScope() { ++input_creation_depth; } ~CreationScope() { --input_creation_depth; } } creation;
        auto object = std::make_unique<ManagedWindow<wxRadioBox>>();
        { wxEventBlocker blocker(object.get());
          require(object->Create(parent, wxID_ANY, utf8(label), wxDefaultPosition, wxDefaultSize, strings, 1,
                                 horizontal ? wxRA_SPECIFY_ROWS : wxRA_SPECIFY_COLS), "Native radio box creation failed");
          object->SetSelection(selection); }
        const auto id = track(object.get()); input_owners.emplace(object.get(), false); object.release(); return id;
    });
}
int wxl_radio_field(wxl_handle id, int field) {
    int result = -1; status([&] { auto* object = as<wxRadioBox>(id);
        require(field == 0 || field == 1, "Invalid radio field");
        result = field == 0 ? object->GetSelection() : static_cast<int>(object->GetCount()); }); return result;
}
const char* wxl_radio_item(wxl_handle id, int index) {
    const int result = status([&] { auto* object = as<wxRadioBox>(id);
        require(index >= 0 && static_cast<unsigned>(index) < object->GetCount(), "Radio index out of range");
        const auto text = object->GetString(index); const auto bytes = text.ToUTF8(); label_result.assign(bytes.data() ? bytes.data() : "", bytes.length()); });
    return result == 0 ? label_result.c_str() : nullptr;
}
int wxl_set_radio_selection(wxl_handle id, int index, int notify) { return status([&] {
    auto* object = as<wxRadioBox>(id);
    require(index >= 0 && static_cast<unsigned>(index) < object->GetCount() && (notify == 0 || notify == 1), "Invalid radio selection/notification");
    InputOperation operation(object);
    { wxEventBlocker blocker(object); object->SetSelection(index); }
    if (notify) { wxCommandEvent event(wxEVT_RADIOBOX, object->GetId()); event.SetEventObject(object);
        event.SetInt(index); event.SetString(object->GetString(index)); object->HandleWindowEvent(event); }
}); }
wxl_handle wxl_numeric(wxl_handle parent_id, int kind, int minimum, int maximum, int value, int vertical) {
    return create([&] {
        auto* parent = window(parent_id); numeric_bounds(minimum, maximum);
        require(value >= minimum && value <= maximum && (kind == 0 || kind == 1) && (vertical == 0 || vertical == 1) && (kind == 0 || vertical == 0), "Invalid numeric constructor arguments");
        struct CreationScope { CreationScope() { ++input_creation_depth; } ~CreationScope() { --input_creation_depth; } } creation;
        std::unique_ptr<wxWindow> object;
        if (kind == 0) {
            auto slider = std::make_unique<ManagedWindow<wxSlider>>();
            { wxEventBlocker blocker(slider.get());
              require(slider->Create(parent, wxID_ANY, value, minimum, maximum, wxDefaultPosition, wxDefaultSize, vertical ? wxSL_VERTICAL : wxSL_HORIZONTAL), "Native slider creation failed"); }
            object = std::move(slider);
        } else {
            auto spin = std::make_unique<ManagedWindow<wxSpinCtrl>>();
            { wxEventBlocker blocker(spin.get());
              require(spin->Create(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, minimum, maximum, value), "Native spin creation failed");
              require(spin->SetBase(10), "Native decimal spin base unavailable"); spin->SetValue(value); }
            object = std::move(spin);
        }
        const auto id = track(object.get()); input_owners.emplace(object.get(), false); object.release(); return id;
    });
}
int wxl_numeric_field(wxl_handle id, int field, int* value) { return status([&] {
    auto* object = numeric_control(id); require(value, "Missing numeric output");
    if (field == 0) *value = numeric_read(object);
    else if (field == 1) *value = numeric_min(object);
    else if (field == 2) *value = numeric_max(object);
    else { require(field == 3 && dynamic_cast<wxSlider*>(object), "Invalid numeric field/receiver"); *value = object->HasFlag(wxSL_VERTICAL) ? 1 : 0; }
}); }
int wxl_set_numeric_value(wxl_handle id, int value, int notify) { return status([&] {
    auto* object = numeric_control(id); require(notify == 0 || notify == 1, "Invalid numeric notification flag");
    require(value >= numeric_min(object) && value <= numeric_max(object), "Numeric value out of range");
    InputOperation operation(object);
    { wxEventBlocker blocker(object);
      if (auto* slider = dynamic_cast<wxSlider*>(object)) slider->SetValue(value); else static_cast<wxSpinCtrl*>(object)->SetValue(value); }
    if (notify) {
        if (dynamic_cast<wxSlider*>(object)) { wxCommandEvent event(wxEVT_SLIDER, object->GetId()); event.SetEventObject(object); event.SetInt(value); object->HandleWindowEvent(event); }
        else { wxSpinEvent event(wxEVT_SPINCTRL, object->GetId()); event.SetEventObject(object); event.SetPosition(value); object->HandleWindowEvent(event); }
    }
}); }
int wxl_set_numeric_range(wxl_handle id, int minimum, int maximum) { return status([&] {
    auto* object = numeric_control(id); numeric_bounds(minimum, maximum);
    const auto value = std::clamp(numeric_read(object), minimum, maximum);
    InputOperation operation(object); wxEventBlocker blocker(object);
    if (auto* slider = dynamic_cast<wxSlider*>(object)) { slider->SetRange(minimum, maximum); slider->SetValue(value); }
    else { auto* spin = static_cast<wxSpinCtrl*>(object); spin->SetRange(minimum, maximum); spin->SetValue(value); }
}); }
wxl_handle wxl_gauge(wxl_handle parent, int range, int value, int vertical) {
    return create([&] {
        require(range > 0, "Gauge range must be positive");
        require(value >= 0 && value <= range, "Gauge value out of range");
        require(vertical == 0 || vertical == 1, "Invalid gauge orientation");
        auto* control = new ManagedWindow<wxGauge>(window(parent), wxID_ANY, range,
                                   wxDefaultPosition, wxDefaultSize,
                                   vertical ? wxGA_VERTICAL : wxGA_HORIZONTAL);
        control->SetValue(value);
        return track(control);
    });
}
int wxl_gauge_value(wxl_handle id) {
    int value = 0;
    const int result = status([&] { value = as<wxGauge>(id)->GetValue(); });
    return result == 0 ? value : -1;
}
int wxl_gauge_range(wxl_handle id) {
    int range = 0;
    const int result = status([&] { range = as<wxGauge>(id)->GetRange(); });
    return result == 0 ? range : -1;
}
int wxl_set_gauge_value(wxl_handle id, int value) {
    return status([&] {
        auto* control = as<wxGauge>(id);
        require(value >= 0 && value <= control->GetRange(), "Gauge value out of range");
        control->SetValue(value);
    });
}
int wxl_set_gauge_range(wxl_handle id, int range) {
    return status([&] {
        auto* control = as<wxGauge>(id);
        require(range > 0, "Gauge range must be positive");
        const int value = std::min(control->GetValue(), range);
        control->SetRange(range);
        control->SetValue(value);
    });
}
int wxl_gauge_vertical(wxl_handle id) {
    return boolean_result([&] { return as<wxGauge>(id)->IsVertical(); });
}
int wxl_gauge_pulse(wxl_handle id) {
    return status([&] { as<wxGauge>(id)->Pulse(); });
}
wxl_handle wxl_checkbox(wxl_handle parent, const char* label, int checked) {
    return create([&] {
        auto* control = new ManagedWindow<wxCheckBox>(window(parent), wxID_ANY, utf8(label));
        control->SetValue(checked != 0);
        const auto id = track(control);
        control->Bind(wxEVT_CHECKBOX, [id](wxCommandEvent& event) {
            emit_event(id, {WXL_EVENT_TOGGLE, nullptr, event.IsChecked() ? 1 : 0, -1, event.GetId(), 0, 0, event.GetEventType(), 0, 0, WXL_FAMILY_COMMAND, event.GetInt(), 0, event.GetTimestamp(), event.IsCommandEvent() ? 1 : 0, origin_id(event), static_cast<int>(event.GetEventCategory()), event.GetExtraLong(), event.IsSelection() ? 1 : 0, 0, 0});
            event.Skip();
        });
        return id;
    });
}
int wxl_checked(wxl_handle id) {
    int checked = 0;
    const int result = status([&] { checked = as<wxCheckBox>(id)->GetValue() ? 1 : 0; });
    return result == 0 ? checked : -1;
}
int wxl_set_checked(wxl_handle id, int checked, int notify) {
    return status([&] {
        auto* control = as<wxCheckBox>(id);
        control->SetValue(checked != 0);
        if (notify) {
            wxCommandEvent event(wxEVT_CHECKBOX, control->GetId());
            event.SetEventObject(control);
            event.SetInt(control->GetValue() ? 1 : 0);
            control->GetEventHandler()->ProcessEvent(event);
        }
    });
}
int wxl_on_toggle(wxl_handle id, uint64_t token) {
    return status([&] { as<wxCheckBox>(id); handlers[id] = token; });
}
int wxl_off_toggle(wxl_handle id) {
    return status([&] { as<wxCheckBox>(id); handlers.erase(id); });
}
wxl_handle wxl_choice(wxl_handle parent, const char** items, int count, int selection) {
    return create([&] {
        auto* owner = window(parent);
        require(count >= 0 && (count == 0 || items), "Invalid choice items");
        require(selection >= -1 && selection < count, "Choice selection out of range");
        wxArrayString strings;
        for (int i = 0; i < count; ++i) strings.Add(utf8(items[i]));
        auto* control = new ManagedWindow<wxChoice>(owner, wxID_ANY, wxDefaultPosition, wxDefaultSize, strings);
        control->SetSelection(selection);
        const auto id = track(control);
        control->Bind(wxEVT_CHOICE, [id](wxCommandEvent& event) {
            emit_event(id, {WXL_EVENT_SELECTION, nullptr, 0, event.GetSelection(), event.GetId(), 0, 0, event.GetEventType(), 0, 0, WXL_FAMILY_COMMAND, event.GetInt(), 0, event.GetTimestamp(), event.IsCommandEvent() ? 1 : 0, origin_id(event), static_cast<int>(event.GetEventCategory()), event.GetExtraLong(), event.IsSelection() ? 1 : 0, 0, 0});
            event.Skip();
        });
        return id;
    });
}
int wxl_selection(wxl_handle id) {
    int selection = -1;
    const int result = status([&] { selection = as<wxChoice>(id)->GetSelection(); });
    return result == 0 ? selection : -2;
}
int wxl_set_selection(wxl_handle id, int selection, int notify) {
    return status([&] {
        auto* control = as<wxChoice>(id);
        require(selection >= -1 && (selection == -1 || static_cast<unsigned>(selection) < control->GetCount()),
                "Choice selection out of range");
        control->SetSelection(selection);
        if (notify) {
            wxCommandEvent event(wxEVT_CHOICE, control->GetId());
            event.SetEventObject(control);
            event.SetInt(selection);
            control->GetEventHandler()->ProcessEvent(event);
        }
    });
}
int wxl_on_selection(wxl_handle id, uint64_t token) {
    return status([&] { as<wxChoice>(id); handlers[id] = token; });
}
int wxl_off_selection(wxl_handle id) {
    return status([&] { as<wxChoice>(id); handlers.erase(id); });
}
int wxl_column(wxl_handle parent) {
    return status([&] { make_box(parent, wxVERTICAL); });
}
int wxl_row(wxl_handle parent) {
    return status([&] { make_box(parent, wxHORIZONTAL); });
}
int wxl_add(wxl_handle parent, wxl_handle child, int proportion,
            int border, int expand, int alignment) {
    return status([&] {
        auto* p = window(parent); auto* c = window(child);
        require(proportion >= 0, "Proportion must be nonnegative");
        require(border >= 0, "Border must be nonnegative");
        require(alignment >= WXL_ALIGN_START && alignment <= WXL_ALIGN_END, "Invalid alignment");
        require(!expand || alignment == WXL_ALIGN_START, "Alignment requires expand=false");
        require(!dynamic_cast<wxToolBar*>(c) && !dynamic_cast<wxStatusBar*>(c), "Frame bars cannot be placed in a sizer");
        require(c->GetParent() == p, "Child must belong to the sizer's window");
        auto* sizer = box(p);
        require(c->GetContainingSizer() == nullptr, "Child already belongs to a sizer");
        int flags = wxALL;
        if (expand) flags |= wxEXPAND;
        else if (alignment == WXL_ALIGN_CENTER)
            flags |= sizer->GetOrientation() == wxHORIZONTAL ? wxALIGN_CENTER_VERTICAL : wxALIGN_CENTER_HORIZONTAL;
        else if (alignment == WXL_ALIGN_END)
            flags |= sizer->GetOrientation() == wxHORIZONTAL ? wxALIGN_BOTTOM : wxALIGN_RIGHT;
        sizer->Add(c, proportion, flags, border);
        p->Layout();
    });
}
int wxl_spacer(wxl_handle parent, int size) {
    return status([&] {
        auto* p = window(parent);
        require(size >= 0, "Spacer size must be nonnegative");
        box(p)->AddSpacer(size);
        p->Layout();
    });
}
int wxl_stretch(wxl_handle parent, int proportion) {
    return status([&] {
        auto* p = window(parent);
        require(proportion > 0, "Stretch proportion must be positive");
        box(p)->AddStretchSpacer(proportion);
        p->Layout();
    });
}
int wxl_layout(wxl_handle id) { return status([&] { window(id)->Layout(); }); }
int wxl_geometry(wxl_handle id, int* x, int* y, int* width, int* height) {
    return status([&] {
        auto* control = window(id);
        require(x && y && width && height, "Geometry output pointers must not be null");
        const auto position = control->GetPosition();
        const auto size = control->GetSize();
        *x = position.x; *y = position.y; *width = size.x; *height = size.y;
    });
}
const char* wxl_name(wxl_handle id, size_t* length) {
    return metadata_string([&] { return window(id)->GetName(); }, length);
}
int wxl_set_name(wxl_handle id, const char* value, size_t length) {
    return status([&] {
        auto* control = window(id);
        require(value != nullptr, "Null UTF-8 name");
        control->SetName(wxString::FromUTF8(value, length));
    });
}
const char* wxl_title(wxl_handle id, size_t* length) {
    return metadata_string([&] { return as<wxFrame>(id)->GetTitle(); }, length);
}
int wxl_set_title(wxl_handle id, const char* value, size_t length) {
    return status([&] {
        auto* frame = as<wxFrame>(id);
        require(value != nullptr, "Null UTF-8 title");
        frame->SetTitle(wxString::FromUTF8(value, length));
    });
}
int wxl_show(wxl_handle id) { return status([&] { auto* object = window(id); require(!dynamic_cast<wxDialog*>(object), "Use SHOW-MODAL for dialogs"); object->Show(); }); }
int wxl_show_state(wxl_handle id, int show) {
    return boolean_result([&] { auto* object = window(id); require(!dynamic_cast<wxDialog*>(object), "Use SHOW-MODAL for dialogs"); return object->Show(show != 0); });
}
int wxl_shown(wxl_handle id) {
    return boolean_result([&] { return window(id)->IsShown(); });
}
int wxl_enable(wxl_handle id, int enable) {
    return boolean_result([&] { return window(id)->Enable(enable != 0); });
}
int wxl_enabled(wxl_handle id) {
    return boolean_result([&] { return window(id)->IsEnabled(); });
}
int wxl_destroy(wxl_handle id) {
    return status([&] {
        auto* object = window(id);
        require(!protected_modal_ancestor(object), "Cannot destroy an active modal dialog or its ancestor");
        require(!protected_notebook_ancestor(object) && !protected_list_ancestor(object) && !protected_tree_ancestor(object) && !protected_splitter_ancestor(object) && !protected_toolbar_ancestor(object) && !protected_accelerator_window(object) && !protected_aui_ancestor(object) && !protected_input_ancestor(object), "Cannot destroy a navigation operation receiver or its ancestor");
        require(!attached_aui_page(object) && !attached_notebook_page(object) && !attached_splitter_pane(object), "Attached pages/panes are owned by their native container");
        if (dynamic_cast<wxToolBar*>(object)) {
            // OSX Destroy() uninstalls native chrome and can defer deletion.
            // Delete synchronously while quiescent, before allowing replacement.
            require_dispatch_entry(true); delete object; return;
        }
        if (auto* bar = dynamic_cast<wxStatusBar*>(object)) {
            require_dispatch_entry(true); auto* frame = dynamic_cast<wxFrame*>(bar->GetParent());
            if (frame && frame->GetStatusBar() == bar) frame->SetStatusBar(nullptr);
            delete bar; return;
        }
        if (callback_depth) {
            wxWeakRef<wxWindow> weak(object);
            pending_destruction.insert(id);
            try {
                wxTheApp->CallAfter([weak, id] {
                    struct ClearRequest {
                        wxl_handle id;
                        ~ClearRequest() { pending_destruction.erase(id); }
                    } clear{id};
                    if (auto* live = weak.get()) live->Destroy();
                    // Native scheduled deletion now covers any surviving window.
                });
            } catch (...) { pending_destruction.erase(id); throw; }
        } else object->Destroy();
    });
}
int wxl_set_text(wxl_handle id, const char* value, size_t length, int notify) {
    return status([&] {
        auto* control = as<wxTextCtrl>(id);
        require(notify == 0 || notify == 1, "Invalid text notification flag");
        const auto text = text_value(value, length);
        if (notify) control->SetValue(text);
        else control->ChangeValue(text);
    });
}
const char* wxl_get_text(wxl_handle id, size_t* length) {
    return metadata_string([&] { return as<wxTextCtrl>(id)->GetValue(); }, length);
}
int wxl_text_multiline(wxl_handle id) {
    return boolean_result([&] { return as<wxTextCtrl>(id)->IsMultiLine(); });
}
int wxl_text_editable(wxl_handle id) {
    return boolean_result([&] { return as<wxTextCtrl>(id)->IsEditable(); });
}
int wxl_set_text_editable(wxl_handle id, int editable) {
    return status([&] {
        require(editable == 0 || editable == 1, "Invalid text editable flag");
        as<wxTextCtrl>(id)->SetEditable(editable != 0);
    });
}
int wxl_text_selection(wxl_handle id, int64_t* from, int64_t* to) {
    return status([&] {
        require(from && to, "Null text selection output");
        long first, last;
        as<wxTextCtrl>(id)->GetSelection(&first, &last);
        *from = first; *to = last;
    });
}
int wxl_set_text_selection(wxl_handle id, int64_t from, int64_t to) {
    return status([&] {
        auto* control = as<wxTextCtrl>(id);
        text_range(control, from, to);
        control->SetSelection(static_cast<long>(from), static_cast<long>(to));
    });
}
int wxl_text_position(wxl_handle id, int field, int64_t* position) {
    return status([&] {
        require(position && (field == 0 || field == 1), "Invalid text position query");
        auto* control = as<wxTextCtrl>(id);
        *position = field == 0 ? control->GetInsertionPoint() : control->GetLastPosition();
    });
}
int wxl_set_text_insertion(wxl_handle id, int64_t position) {
    return status([&] {
        auto* control = as<wxTextCtrl>(id);
        text_range(control, position, position);
        control->SetInsertionPoint(static_cast<long>(position));
    });
}
int wxl_replace_text(wxl_handle id, int64_t from, int64_t to, const char* value, size_t length) {
    return status([&] {
        auto* control = as<wxTextCtrl>(id);
        text_range(control, from, to);
        control->Replace(static_cast<long>(from), static_cast<long>(to), text_value(value, length));
    });
}
int wxl_on_text_change(wxl_handle id, uint64_t token) {
    return status([&] { as<wxTextCtrl>(id); handlers[id] = token; });
}
int wxl_off_text_change(wxl_handle id) {
    return status([&] { as<wxTextCtrl>(id); handlers.erase(id); });
}
int wxl_off_click(wxl_handle id) {
    return status([&] { as<wxButton>(id); handlers.erase(id); });
}
int wxl_on_click(wxl_handle id, uint64_t token) { return status([&] { as<wxButton>(id); handlers[id] = token; }); }
int wxl_click(wxl_handle id) {
    return status([&] {
        auto* b = as<wxButton>(id);
        wxCommandEvent event(wxEVT_BUTTON, b->GetId());
        event.SetEventObject(b);
        wxPostEvent(b, event);
    });
}
}

#include "drawing-canvas.inc"

extern "C" {
#include "activity-indicator.inc"
}

extern "C" {
#include "grid.inc"
}
#include "webview.inc"

#include "stc.inc"
#include "aui.inc"
extern "C" {
#define WXL_INPUT_EVENT_EXPORTS
#include "input-events.inc"
#undef WXL_INPUT_EVENT_EXPORTS
}

extern "C" {
#include "splitter-extra.inc"
}
