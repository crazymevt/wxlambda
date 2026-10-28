#include "wxlambda.h"
#include <wx/wx.h>
#include <wx/thread.h>
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
int wxl_abi_version() { return 4; }
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
            const auto it = handlers.find(id);
            if (it != handlers.end() && dispatch) dispatch(it->second, WXL_EVENT_CLICK, nullptr);
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
                dispatch(token, WXL_EVENT_TEXT_CHANGE, bytes.data() ? bytes.data() : "");
            }
        });
        return id;
    });
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
int wxl_show(wxl_handle id) { return status([&] { window(id)->Show(); }); }
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
