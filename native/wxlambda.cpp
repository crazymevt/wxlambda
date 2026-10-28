#include "wxlambda.h"
#include <wx/wx.h>
#include <wx/thread.h>
#include <wx/choice.h>
#include <wx/gauge.h>
#include <wx/weakref.h>
#include <memory>
#include <typeinfo>
#include <unordered_set>
#include <vector>
#include <limits>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>
#ifdef __APPLE__
#include <pthread.h>
#endif

namespace {
static_assert(wxEVT_CATEGORY_UI == 1 && wxEVT_CATEGORY_USER_INPUT == 2 &&
              wxEVT_CATEGORY_SOCKET == 4 && wxEVT_CATEGORY_TIMER == 8 &&
              wxEVT_CATEGORY_THREAD == 16,
              "Review the pinned public event-category baseline for this wxWidgets build");
bool session_type(wxEventType type) {
    return type == wxEVT_QUERY_END_SESSION || type == wxEVT_END_SESSION;
}
// Synthetic session probes must never fall through to wxApp's exit defaults.
class StandaloneHandler final : public wxEvtHandler {
    std::unordered_set<wxEvent*> queued_sessions;
public:
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
class App final : public wxApp {
public:
    bool OnInit() override { return true; }
};
thread_local std::string error;
thread_local std::string text_result;
thread_local std::string label_result;
thread_local std::string metadata_result;
std::unordered_map<wxl_handle, wxWeakRef<wxWindow>> windows;
std::unordered_map<wxl_handle, std::unique_ptr<wxEvtHandler>> owned_handlers;
struct PushedHandler { wxWeakRef<wxWindow> window; wxl_handle window_id; };
std::unordered_map<wxl_handle, PushedHandler> pushed_handlers;
std::unordered_map<wxl_handle, std::unique_ptr<wxEvent>> owned_events;
std::unordered_map<wxl_handle, uint64_t> handlers;
struct Binding;
std::unordered_map<uint64_t, std::shared_ptr<Binding>> bindings;
std::unordered_set<wxl_handle> pending_destruction;
struct RegisteredEvent { wxEventType type; int family; };
std::unordered_map<int, RegisteredEvent> registered_events;
int next_event_kind = 8;
wxl_handle next_handle = 1;
wxl_handle application_handle = 0;
wxl_callback dispatch = nullptr;
bool started = false, used = false, running = false, pumping = false;
unsigned callback_depth = 0;
struct CallbackScope {
    CallbackScope() { ++callback_depth; }
    ~CallbackScope() { --callback_depth; }
};
struct EventScope {
    uint64_t token;
    static uint64_t next;
    struct Borrow { uint64_t token; wxEvent* event; unsigned depth; };
    static std::vector<Borrow> stack;
    explicit EventScope(wxEvent& event) : token(++next) { stack.push_back({token, &event, callback_depth + 1}); }
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
wxWindow* window(wxl_handle id) {
    ready();
    const auto it = windows.find(id);
    require(it != windows.end(), "Invalid or destroyed window handle");
    require(!pending_destruction.count(id), "Window destruction is pending");
    auto* object = it->second.get();
    require(object != nullptr, "Invalid or destroyed window handle");
    require(!object->IsBeingDeleted(), "Window is being destroyed");
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
template<class T> class ManagedWindow final : public T {
public:
    using T::T;
    ~ManagedWindow() override { detach_window_stack(this); }
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
void make_box(wxl_handle id, int orientation) {
    auto* parent = window(id);
    require(parent->GetSizer() == nullptr, "Window already has a sizer");
    parent->SetSizer(new wxBoxSizer(orientation));
}
void invoke_callback(uint64_t token, const wxl_event& event) {
    CallbackScope scope;
    dispatch(token, &event);
}
void require_dispatch_entry() {
    require(!running, "Event loop is already running");
    require(!pumping && callback_depth == 0, "Cannot enter event dispatch from a pump or callback");
}
void emit_event(wxl_handle id, const wxl_event& event) {
    const auto it = handlers.find(id);
    if (it != handlers.end() && dispatch && !pending_destruction.count(id)) {
        const auto token = it->second;
        invoke_callback(token, event);
    }
}
wxEventTypeTag<wxEvent> native_event_type(int kind) {
    switch (kind) {
        case WXL_EVENT_CLICK: return wxEventTypeTag<wxEvent>(wxEVT_BUTTON);
        case WXL_EVENT_TEXT_CHANGE: return wxEventTypeTag<wxEvent>(wxEVT_TEXT);
        case WXL_EVENT_TOGGLE: return wxEventTypeTag<wxEvent>(wxEVT_CHECKBOX);
        case WXL_EVENT_SELECTION: return wxEventTypeTag<wxEvent>(wxEVT_CHOICE);
        case WXL_EVENT_CLOSE: return wxEventTypeTag<wxEvent>(wxEVT_CLOSE_WINDOW);
        case WXL_EVENT_QUERY_END_SESSION: return wxEventTypeTag<wxEvent>(wxEVT_QUERY_END_SESSION);
        case WXL_EVENT_END_SESSION: return wxEventTypeTag<wxEvent>(wxEVT_END_SESSION);
        default: {
            const auto it = registered_events.find(kind);
            require(it != registered_events.end(), "Unsupported event kind");
            return wxEventTypeTag<wxEvent>(it->second.type);
        }
    }
}
int native_event_family(int kind) {
    if (kind >= WXL_EVENT_CLICK && kind <= WXL_EVENT_SELECTION) return WXL_FAMILY_COMMAND;
    if (kind >= WXL_EVENT_CLOSE && kind <= WXL_EVENT_END_SESSION) return WXL_FAMILY_CLOSE;
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
    const auto type = native_event_type(kind);
    const int family = native_event_family(kind);
    require(text || length == 0, "Null event text");
    require(kind != WXL_EVENT_SELECTION || value >= -1, "Invalid selection event index");
    require(session_type(type) || logging_off == 0, "Logging-off applies only to session events");
    require(family == WXL_FAMILY_NOTIFY || vetoed == 0, "Initial veto applies only to notify types");
    std::unique_ptr<wxEvent> event;
    if (family == WXL_FAMILY_CLOSE) {
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
        if (kind == WXL_EVENT_TEXT_CHANGE || kind >= 8)
            command->SetString(wxString::FromUTF8(text ? text : "", length));
        command->SetInt(value);
        event = std::move(command);
    }
    event->SetEventObject(receiver);
    return event;
}
int kind_of(wxEvent& event) {
    for (int kind = 1; kind <= 7; ++kind)
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
            typeid(event) == typeid(wxCloseEvent), "Cloning this native event class is not supported");
    kind_of(event);
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
        case 15: return command && event.GetEventType() == wxEVT_CHECKBOX && command->IsChecked();
        case 16: return command && event.GetEventType() == wxEVT_CHOICE ? command->GetSelection() : -1;
        case 17: return command && command->IsSelection();
        default: throw std::runtime_error("Unknown event field");
    }
}
void write_event_field(wxEvent& event, int field, int64_t value) {
    auto* close = dynamic_cast<wxCloseEvent*>(&event);
    auto* notify = dynamic_cast<wxNotifyEvent*>(&event);
    auto* command = dynamic_cast<wxCommandEvent*>(&event);
    if (field == 1 || field == 3 || field == 7 || field == 11)
        require(value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max(), "Event integer outside signed int range");
    switch (field) {
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
            require(command && event.GetEventType() == wxEVT_CHECKBOX, "Checked metadata requires a toggle event");
            command->SetInt(value != 0 ? 1 : 0); break;
        case 16:
            require(command && event.GetEventType() == wxEVT_CHOICE, "Selection metadata requires a selection event");
            require(value >= -1 && value <= std::numeric_limits<int>::max(), "Selection index outside supported range");
            command->SetInt(static_cast<int>(value)); break;
        default: throw std::runtime_error("This event field is read-only or unknown");
    }
}
struct Binding {
    wxWeakRef<wxEvtHandler> receiver;
    wxl_handle source;
    uint64_t token;
    int kind, id, last_id;
    bool with_context;
    bool live() const {
        auto* object = receiver.get();
        auto* widget = dynamic_cast<wxWindow*>(object);
        return object && !pending_destruction.count(source) && (!widget || !widget->IsBeingDeleted());
    }
    void on_event(wxEvent& event) {
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
            const bool carries_text = snapshot_kind == WXL_EVENT_TEXT_CHANGE || snapshot_kind >= 8;
            const auto text = command ? command->GetString() : wxString();
            const auto bytes = text.ToUTF8();
            wxl_event snapshot{snapshot_kind, carries_text ? (bytes.data() ? bytes.data() : "") : nullptr,
                                     snapshot_kind == WXL_EVENT_TOGGLE && command && command->IsChecked() ? 1 : 0,
                                     snapshot_kind == WXL_EVENT_SELECTION && command ? command->GetSelection() : -1,
                                     event.GetId(), carries_text ? bytes.length() : 0,
                                     0, event.GetEventType(), event_can_veto(event) ? 1 : 0, event_vetoed(event) ? 1 : 0,
                                     family, command ? command->GetInt() : 0,
                                     session_type(event.GetEventType()) ? (static_cast<wxCloseEvent&>(event).GetLoggingOff() ? 1 : 0) : 0,
                                     event.GetTimestamp(), event.IsCommandEvent() ? 1 : 0, origin_id(event), static_cast<int>(event.GetEventCategory()),
                                     command ? command->GetExtraLong() : 0, command && command->IsSelection() ? 1 : 0};
            if (with_context) {
                event.Skip(false);
                EventScope scope(event);
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
                                 binding->id, binding->last_id), "Native event binding not found");
    }
    bindings.erase(token);
    return true;
}
wxl_handle track(wxWindow* object) {
    const auto id = next_handle++;
    windows.emplace(id, object);
    object->Bind(wxEVT_DESTROY, [id, object](wxWindowDestroyEvent& event) {
        if (event.GetEventObject() == object) {
            windows.erase(id);
            handlers.erase(id);
        }
        event.Skip();
    });
    return id;
}
template<class F> int status(F fn) noexcept {
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
}
extern "C" {
int wxl_abi_version() { return 28; }
const char* wxl_last_error() { return error.c_str(); }
int wxl_start(wxl_callback callback) {
    return status([&] {
        main_thread();
        require(!used, "Only one application session is supported per process");
        require(wxApp::GetInstance() == nullptr, "A wxWidgets application already exists");
        require(callback != nullptr, "Missing event dispatcher");
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
        dispatch = callback;
        application_handle = next_handle++;
        started = true;
    });
}
int wxl_run() {
    return status([] {
        ready();
        require_dispatch_entry();
        require(!wxTopLevelWindows.empty(), "Create a frame before running the event loop");
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
        wxEntryCleanup();
        windows.clear(); handlers.clear(); pending_destruction.clear(); registered_events.clear(); dispatch = nullptr; started = false;
    });
}
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
            return object && !object->IsBeingDeleted();
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
            if (pushed_handlers.count(id)) { require_dispatch_entry(); remove_pushed_handler(id); }
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
        require_dispatch_entry();
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
        require_dispatch_entry();
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
    return status([&] { current_event(token).SetId(value); });
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
    return boolean_result([&] { return as<wxFrame>(id)->Close(force != 0); });
}
int wxl_context_resume_propagation(uint64_t token, int level) {
    return status([&] {
        auto& event = current_event(token);
        require(level >= 0, "Propagation level must be nonnegative");
        event.ResumePropagation(level);
    });
}
int wxl_bind(wxl_handle id, int kind, uint64_t token, int first, int last, int with_context) {
    return status([&] {
        auto* receiver = event_handler(id);
        const auto type = native_event_type(kind);
        require(!session_type(type) || id == application_handle || isolated_handler(id),
                "Session bindings require the application view or an owned standalone handler");
        require(with_context == 0 || with_context == 1, "Invalid binding context mode");
        require(token && !bindings.count(token), "Invalid or duplicate binding token");
        require(last == wxID_ANY || (first != wxID_ANY && first <= last), "Invalid event ID range");
        auto binding = std::make_shared<Binding>();
        binding->receiver = receiver;
        binding->source = id; binding->token = token;
        binding->kind = kind; binding->id = first; binding->last_id = last;
        binding->with_context = with_context != 0;
        bindings.emplace(token, binding);
        try { receiver->Bind(type, &Binding::on_event, binding.get(), first, last); }
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
            require(!window || !window->WXGetTextEntry(),
                    "Text-entry-origin text-change payloads are lazy and cannot be mutated");
        }
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
        require_dispatch_entry();
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
        ready(); require_dispatch_entry();
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
        ready(); require_dispatch_entry();
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
        ready(); require_dispatch_entry();
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
            emit_event(id, {WXL_EVENT_CLICK, nullptr, 0, -1, event.GetId(), 0, 0, event.GetEventType(), 0, 0, WXL_FAMILY_COMMAND, event.GetInt(), 0, event.GetTimestamp(), event.IsCommandEvent() ? 1 : 0, origin_id(event), static_cast<int>(event.GetEventCategory()), event.GetExtraLong(), event.IsSelection() ? 1 : 0});
            event.Skip();
        });
        return id;
    });
}
wxl_handle wxl_text(wxl_handle parent, const char* value) {
    return create([&] {
        auto* control = new ManagedWindow<wxTextCtrl>(window(parent), wxID_ANY, utf8(value));
        const auto id = track(control);
        control->Bind(wxEVT_TEXT, [id](wxCommandEvent& event) {
            const auto it = handlers.find(id);
            if (it != handlers.end() && dispatch && !pending_destruction.count(id)) {
                const auto token = it->second;
                // Both objects must survive reentrant callbacks and conversion.
                const auto text = event.GetString();
                const auto bytes = text.ToUTF8();
                const wxl_event payload{WXL_EVENT_TEXT_CHANGE, bytes.data() ? bytes.data() : "", 0, -1, event.GetId(), bytes.length(), 0, event.GetEventType(), 0, 0, WXL_FAMILY_COMMAND, event.GetInt(), 0, event.GetTimestamp(), event.IsCommandEvent() ? 1 : 0, origin_id(event), static_cast<int>(event.GetEventCategory()), event.GetExtraLong(), event.IsSelection() ? 1 : 0};
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
            emit_event(id, {WXL_EVENT_TOGGLE, nullptr, event.IsChecked() ? 1 : 0, -1, event.GetId(), 0, 0, event.GetEventType(), 0, 0, WXL_FAMILY_COMMAND, event.GetInt(), 0, event.GetTimestamp(), event.IsCommandEvent() ? 1 : 0, origin_id(event), static_cast<int>(event.GetEventCategory()), event.GetExtraLong(), event.IsSelection() ? 1 : 0});
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
            emit_event(id, {WXL_EVENT_SELECTION, nullptr, 0, event.GetSelection(), event.GetId(), 0, 0, event.GetEventType(), 0, 0, WXL_FAMILY_COMMAND, event.GetInt(), 0, event.GetTimestamp(), event.IsCommandEvent() ? 1 : 0, origin_id(event), static_cast<int>(event.GetEventCategory()), event.GetExtraLong(), event.IsSelection() ? 1 : 0});
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
int wxl_show(wxl_handle id) { return status([&] { window(id)->Show(); }); }
int wxl_show_state(wxl_handle id, int show) {
    return boolean_result([&] { return window(id)->Show(show != 0); });
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
        if (callback_depth) {
            wxWeakRef<wxWindow> weak(object);
            pending_destruction.insert(id);
            try {
                wxTheApp->CallAfter([weak, id] {
                    pending_destruction.erase(id);
                    if (auto* live = weak.get()) live->Destroy();
                });
            } catch (...) { pending_destruction.erase(id); throw; }
        } else object->Destroy();
    });
}
int wxl_set_text(wxl_handle id, const char* value, int notify) {
    return status([&] {
        auto* control = as<wxTextCtrl>(id);
        const auto text = utf8(value);
        if (notify) control->SetValue(text);
        else control->ChangeValue(text);
    });
}
const char* wxl_get_text(wxl_handle id) {
    const int result = status([&] {
        // ToUTF8 may borrow storage from its wxString; keep the source alive.
        const auto text = as<wxTextCtrl>(id)->GetValue();
        const auto value = text.ToUTF8();
        text_result.assign(value.data() ? value.data() : "", value.length());
    });
    return result == 0 ? text_result.c_str() : nullptr;
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
