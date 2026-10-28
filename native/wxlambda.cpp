#include "wxlambda.h"
#include <wx/wx.h>
#include <wx/thread.h>
#include <wx/choice.h>
#include <wx/gauge.h>
#include <wx/weakref.h>
#include <memory>
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
bool session_type(wxEventType type) {
    return type == wxEVT_QUERY_END_SESSION || type == wxEVT_END_SESSION;
}
// Synthetic session probes must never fall through to wxApp's exit defaults.
class StandaloneHandler final : public wxEvtHandler {
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
            const int family = native_event_family(kind);
            const bool carries_text = kind == WXL_EVENT_TEXT_CHANGE || kind >= 8;
            const auto text = command ? command->GetString() : wxString();
            const auto bytes = text.ToUTF8();
            wxl_event snapshot{kind, carries_text ? (bytes.data() ? bytes.data() : "") : nullptr,
                                     kind == WXL_EVENT_TOGGLE && command && command->IsChecked() ? 1 : 0,
                                     kind == WXL_EVENT_SELECTION && command ? command->GetSelection() : -1,
                                     event.GetId(), carries_text ? bytes.length() : 0,
                                     0, event.GetEventType(), event_can_veto(event) ? 1 : 0, event_vetoed(event) ? 1 : 0,
                                     family, command ? command->GetInt() : 0,
                                     session_type(event.GetEventType()) ? (static_cast<wxCloseEvent&>(event).GetLoggingOff() ? 1 : 0) : 0};
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
int wxl_abi_version() { return 18; }
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
        // Destroy remaining top-level windows before OnExit (also on setup failure).
        while (!wxTopLevelWindows.empty())
            delete wxTopLevelWindows.GetFirst()->GetData();
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
        if (callback_depth) {
            pending_destruction.insert(id);
            try {
                wxTheApp->CallAfter([id] { owned_handlers.erase(id); pending_destruction.erase(id); });
            } catch (...) { pending_destruction.erase(id); throw; }
        } else owned_handlers.erase(it);
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
        require(!session_type(type) || id == application_handle || owned_handlers.count(id),
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
        require(!session_type(native_event_type(kind)) || owned_handlers.count(id),
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
        require(!session_type(native_event_type(kind)) || owned_handlers.count(id),
                "Synthetic session events require an isolated owned standalone handler");
        auto event = synthetic_event(receiver, kind, event_id, text, length, value, can_veto, vetoed, logging_off);
        const bool handled = receiver->ProcessEvent(*event);
        *final_veto = event_vetoed(*event) ? 1 : 0;
        return handled;
    });
}
wxl_handle wxl_frame(const char* title, int width, int height) {
    return create([&] {
        require(width > 0 && height > 0, "Frame dimensions must be positive");
        return track(new wxFrame(nullptr, wxID_ANY, utf8(title), wxDefaultPosition, wxSize(width, height)));
    });
}
wxl_handle wxl_panel(wxl_handle parent) { return create([&] { return track(new wxPanel(window(parent))); }); }
wxl_handle wxl_button(wxl_handle parent, const char* label) {
    return create([&] {
        auto* control = new wxButton(window(parent), wxID_ANY, utf8(label));
        auto id = track(control);
        control->Bind(wxEVT_BUTTON, [id](wxCommandEvent& event) {
            emit_event(id, {WXL_EVENT_CLICK, nullptr, 0, -1, event.GetId(), 0, 0, event.GetEventType(), 0, 0, WXL_FAMILY_COMMAND, event.GetInt(), 0});
            event.Skip();
        });
        return id;
    });
}
wxl_handle wxl_text(wxl_handle parent, const char* value) {
    return create([&] {
        auto* control = new wxTextCtrl(window(parent), wxID_ANY, utf8(value));
        const auto id = track(control);
        control->Bind(wxEVT_TEXT, [id](wxCommandEvent& event) {
            const auto it = handlers.find(id);
            if (it != handlers.end() && dispatch && !pending_destruction.count(id)) {
                const auto token = it->second;
                // Both objects must survive reentrant callbacks and conversion.
                const auto text = event.GetString();
                const auto bytes = text.ToUTF8();
                const wxl_event payload{WXL_EVENT_TEXT_CHANGE, bytes.data() ? bytes.data() : "", 0, -1, event.GetId(), bytes.length(), 0, event.GetEventType(), 0, 0, WXL_FAMILY_COMMAND, event.GetInt(), 0};
                invoke_callback(token, payload);
            }
            event.Skip();
        });
        return id;
    });
}
wxl_handle wxl_label(wxl_handle parent, const char* label) {
    return create([&] { return track(new wxStaticText(window(parent), wxID_ANY, utf8(label))); });
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
        auto* control = new wxGauge(window(parent), wxID_ANY, range,
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
        auto* control = new wxCheckBox(window(parent), wxID_ANY, utf8(label));
        control->SetValue(checked != 0);
        const auto id = track(control);
        control->Bind(wxEVT_CHECKBOX, [id](wxCommandEvent& event) {
            emit_event(id, {WXL_EVENT_TOGGLE, nullptr, event.IsChecked() ? 1 : 0, -1, event.GetId(), 0, 0, event.GetEventType(), 0, 0, WXL_FAMILY_COMMAND, event.GetInt(), 0});
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
        auto* control = new wxChoice(owner, wxID_ANY, wxDefaultPosition, wxDefaultSize, strings);
        control->SetSelection(selection);
        const auto id = track(control);
        control->Bind(wxEVT_CHOICE, [id](wxCommandEvent& event) {
            emit_event(id, {WXL_EVENT_SELECTION, nullptr, 0, event.GetSelection(), event.GetId(), 0, 0, event.GetEventType(), 0, 0, WXL_FAMILY_COMMAND, event.GetInt(), 0});
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
