#include "wxlambda.h"
#include <wx/wx.h>
#include <wx/thread.h>
#include <wx/choice.h>
#include <wx/gauge.h>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>
#ifdef __APPLE__
#include <pthread.h>
#endif

namespace {
class App final : public wxApp {
public:
    bool OnInit() override { return true; }
};
thread_local std::string error;
thread_local std::string text_result;
thread_local std::string label_result;
thread_local std::string metadata_result;
std::unordered_map<wxl_handle, wxWindow*> windows;
std::unordered_map<wxl_handle, uint64_t> handlers;
wxl_handle next_handle = 1;
wxl_callback dispatch = nullptr;
bool started = false, used = false, running = false;

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
wxWindow* window(wxl_handle id) {
    ready();
    const auto it = windows.find(id);
    require(it != windows.end(), "Invalid or destroyed window handle");
    require(!it->second->IsBeingDeleted(), "Window is being destroyed");
    return it->second;
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
void emit_event(wxl_handle id, const wxl_event& event) {
    const auto it = handlers.find(id);
    if (it != handlers.end() && dispatch) {
        const auto token = it->second;
        dispatch(token, &event);
    }
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
int wxl_abi_version() { return 10; }
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
        started = true;
    });
}
int wxl_run() {
    return status([] {
        ready();
        require(!running, "Event loop is already running");
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
        require(!running, "Cannot poll inside the blocking event loop");
        wxTheApp->Yield(true);
        wxTheApp->ProcessIdle();
    });
}
int wxl_stop() { return status([] { ready(); require(running, "Event loop is not running"); wxTheApp->ExitMainLoop(); }); }
int wxl_finish() {
    return status([] {
        ready();
        require(!running, "Cannot clean up inside the event loop");
        // Destroy remaining top-level windows before OnExit (also on setup failure).
        while (!wxTopLevelWindows.empty())
            delete wxTopLevelWindows.GetFirst()->GetData();
        wxTheApp->OnExit();
        wxEntryCleanup();
        windows.clear(); handlers.clear(); dispatch = nullptr; started = false;
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
        control->Bind(wxEVT_BUTTON, [id](wxCommandEvent&) {
            emit_event(id, {WXL_EVENT_CLICK, nullptr, 0});
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
            if (it != handlers.end() && dispatch) {
                const auto token = it->second;
                // Both objects must survive reentrant callbacks and conversion.
                const auto text = event.GetString();
                const auto bytes = text.ToUTF8();
                const wxl_event payload{WXL_EVENT_TEXT_CHANGE, bytes.data() ? bytes.data() : "", 0};
                dispatch(token, &payload);
            }
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
            emit_event(id, {WXL_EVENT_TOGGLE, nullptr, event.IsChecked() ? 1 : 0});
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
            emit_event(id, {WXL_EVENT_SELECTION, nullptr, 0, event.GetSelection()});
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
int wxl_destroy(wxl_handle id) { return status([&] { window(id)->Destroy(); }); }
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
