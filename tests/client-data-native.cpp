// Foreign client-data states cannot be manufactured through the public Lisp API.
// Include the bridge in this executable only, without adding injection exports.
// Build with -DWXLAMBDA_BUILD_NATIVE_TESTS=ON; run wxlambda-client-data-native.
#include "../native/wxlambda.cpp"
#include <atomic>
#include <iostream>

namespace {
std::atomic<unsigned> assertion_count{0};
void capture_assert(const wxString&, int, const wxString&, const wxString&, const wxString&) {
    ++assertion_count;
}
void expect(bool ok, const char* message);
bool binding_called = false;
uint64_t saved_context = 0;
void inspect_binding(uint64_t token, const wxl_event* event) {
    expect(event->user_data == token, "Snapshot binding user data mismatch");
    uint64_t value = 0;
    expect(wxl_context_user_data(event->context, &value) == 0 && value == token,
           "Current context must expose its captured binding token");
    expect(wxl_unbind(token) == 1, "Self-unbind failed");
    value = 0;
    expect(wxl_context_user_data(event->context, &value) == 0 && value == token,
           "Self-unbind must not invalidate captured context user data");
    saved_context = event->context;
    binding_called = true;
}
void ignore_event(uint64_t token, const wxl_event* event) {
    if (token == 901) inspect_binding(token, event);
}
void expect(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
struct ForeignClientData final : wxClientData {
    explicit ForeignClientData(unsigned& count) : destroyed(count) { }
    ~ForeignClientData() override { ++destroyed; }
    unsigned& destroyed;
};
uint64_t read_token(wxl_handle id) {
    uint64_t token = 999;
    expect(wxl_handler_client_data(id, &token) == 0, "Client-data read failed");
    return token;
}
void expect_rejected(wxl_handle id) {
    uint64_t token = 999;
    expect(wxl_handler_client_data(id, &token) == -1, "Foreign storage read must reject");
    expect(token == 999, "Failed read must preserve its output");
    expect(wxl_set_handler_client_data(id, 41) == -1, "Foreign storage replacement must reject");
    expect(wxl_set_handler_client_data(id, 0) == -1, "Foreign storage clearing must reject");
}
void exercise_idle() {
    expect(native_event_type(WXL_EVENT_IDLE) == wxEVT_IDLE && native_event_family(WXL_EVENT_IDLE) == WXL_FAMILY_IDLE,
           "Idle builtin type/family mismatch");
    auto event = synthetic_event(nullptr, WXL_EVENT_IDLE, 37, nullptr, 0, 0, 1, 0, 0);
    expect(typeid(*event) == typeid(wxIdleEvent) && event->GetId() == 37 && kind_of(*event) == WXL_EVENT_IDLE,
           "Synthetic idle class/ID mismatch");
    expect(read_event_field(*event, 18) == 0, "Idle RequestMore must default false");
    write_event_field(*event, 18, 1);
    auto copy = detached_clone(*event);
    expect(read_event_field(*copy, 18) == 1 && copy->GetEventObject() == nullptr,
           "Idle Clone must preserve RequestMore independently");
    write_event_field(*event, 18, 0);
    expect(read_event_field(*copy, 18) == 1, "Idle clone must be independent");
    wxCommandEvent command(wxEVT_BUTTON);
    expect(read_event_field(command, 18) == 0, "Non-idle MoreRequested must be false");
    expect(status([&] { write_event_field(command, 18, 1); }) == -1, "Non-idle RequestMore must reject");
    expect(status([&] { synthetic_event(nullptr, WXL_EVENT_IDLE, 1, "x", 1, 0, 1, 0, 0); }) == -1,
           "Idle command text must reject");
    expect(status([&] { synthetic_event(nullptr, WXL_EVENT_IDLE, 1, nullptr, 0, 1, 1, 0, 0); }) == -1,
           "Idle command integer must reject");
    expect(wxl_set_idle_mode(1) == 0 && wxl_idle_mode() == 1, "Specified idle mode failed");
    expect(wxl_set_idle_mode(2) == -1 && wxl_idle_mode() == 1, "Invalid mode must preserve old mode");
    expect(wxl_set_idle_mode(0) == 0 && wxl_idle_mode() == 0, "All idle mode failed");
    const auto frame = wxl_frame("Idle native regression", 200, 120);
    auto* widget = window(frame);
    const auto original = widget->GetExtraStyle() | wxWS_EX_BLOCK_EVENTS;
    widget->SetExtraStyle(original);
    expect(wxl_set_window_process_idle(frame, 1) == 0 && wxl_window_process_idle(frame) == 1,
           "Window idle opt-in failed");
    expect(widget->GetExtraStyle() == (original | wxWS_EX_PROCESS_IDLE), "Idle opt-in damaged other style bits");
    expect(wxl_set_window_process_idle(frame, 0) == 0 && wxl_window_process_idle(frame) == 0,
           "Window idle opt-out failed");
    expect(widget->GetExtraStyle() == (original & ~wxWS_EX_PROCESS_IDLE), "Idle opt-out damaged other style bits");
    expect(wxl_wake_up_idle() == 0, "WakeUpIdle failed");
    expect(wxl_set_idle_mode(1) == 0, "Restore-check mode setup failed");
}
void exercise_binding_data() {
    const auto id = wxl_event_handler();
    expect(wxl_bind(id, WXL_EVENT_CLICK, 901, wxID_ANY, wxID_ANY, 1, 1) == 0,
           "User-data binding creation failed");
    wxCommandEvent event(wxEVT_BUTTON, 17);
    event_handler(id)->ProcessEvent(event);
    expect(binding_called && wxl_binding_active(901) == 0, "Self-unbinding callback was not delivered");
    uint64_t value = 99;
    expect(wxl_context_user_data(saved_context, &value) == -1 && value == 99,
           "Expired context user data must reject without writing output");
    auto owned = std::make_unique<wxCommandEvent>(wxEVT_BUTTON, 17);
    const auto event_id = next_handle++;
    owned_events.emplace(event_id, std::move(owned));
    expect(wxl_event_user_data(event_id, &value) == 0 && value == 0,
           "Owned events have no binding user data");
    BindingUserData foreign(999);
    owned_event(event_id).m_callbackUserData = &foreign; // Test-only internal-state injection.
    value = 99;
    expect(wxl_event_user_data(event_id, &value) == -1 && value == 99,
           "Owned borrowed user data must reject");
    expect(wxl_clone_event(event_id, 0) == 0, "Cloning borrowed user data must reject");
    owned_event(event_id).m_callbackUserData = nullptr;
    expect(wxl_event_destroy(event_id) == 0, "Owned cleanup failed");
    expect(wxl_bind(id, WXL_EVENT_CLICK, 902, wxID_ANY, wxID_ANY, 0, 1) == 0,
           "Destroy-time binding creation failed");
    expect(wxl_handler_destroy(id) == 0, "Bound handler cleanup failed");
}
void exercise_client_data(unsigned& foreign_destroyed) {
    const auto empty = wxl_event_handler();
    expect(empty != 0 && read_token(empty) == 0, "Fresh storage must be empty");
    expect(wxl_set_handler_client_data(empty, 0) == 0, "Clearing fresh storage failed");
    expect(read_token(empty) == 0, "Typed-null storage must read empty");
    expect(wxl_set_handler_client_data(empty, 17) == 0, "Initial token assignment failed");
    expect(read_token(empty) == 17 && wxl_client_data_active(17) == 1, "Token must be associated");
    expect(wxl_set_handler_client_data(empty, 19) == 0, "Replacement failed");
    expect(wxl_client_data_active(17) == 0 && wxl_client_data_active(19) == 1,
           "Replacing client object must retire its old token");
    expect(wxl_set_handler_client_data(empty, 0) == 0, "Clearing bridge storage failed");
    expect(read_token(empty) == 0 && wxl_client_data_active(19) == 0, "Clear must retire token");

    const auto untyped = wxl_event_handler();
    auto* untyped_object = event_handler(untyped);
    int sentinel = 23;
    untyped_object->SetClientData(&sentinel);
    expect_rejected(untyped);
    expect(untyped_object->GetClientData() == &sentinel, "Foreign raw pointer was mutated");
    untyped_object->SetClientData(nullptr);
    expect_rejected(untyped);
    expect(untyped_object->GetClientData() == nullptr, "Void-null storage was mutated");

    const auto foreign = wxl_event_handler();
    auto* foreign_object = event_handler(foreign);
    auto* foreign_data = new ForeignClientData(foreign_destroyed);
    foreign_object->SetClientObject(foreign_data);
    expect_rejected(foreign);
    expect(foreign_destroyed == 0 && foreign_object->GetClientObject() == foreign_data,
           "Rejected operation must not delete or replace foreign client data");
    expect(wxl_client_data_active(41) == 0, "Foreign storage must not invent an active token");
    expect(wxl_handler_destroy(foreign) == 0 && foreign_destroyed == 1,
           "Native owner must delete its foreign object exactly once");

    // The shared private type discriminator must work for wxApp and every
    // managed widget class, not only owned standalone handlers.
    const auto app = wxl_application_handler();
    expect(wxl_set_handler_client_data(app, 31) == 0 && read_token(app) == 31,
           "Application client data failed");
    const auto frame = wxl_frame("Native client-data regression", 200, 120);
    const auto panel = wxl_panel(frame);
    const char* items[] = {"item"};
    const wxl_handle controls[] = {frame, panel, wxl_button(panel, "button"), wxl_text(panel, "text"),
                                  wxl_label(panel, "label"), wxl_checkbox(panel, "checkbox", 0),
                                  wxl_choice(panel, items, 1, -1), wxl_gauge(panel, 100, 0, 0)};
    uint64_t token = 100;
    for (const auto id : controls) {
        expect(id != 0 && wxl_set_handler_client_data(id, token) == 0, "Widget client data failed");
        expect(read_token(id) == token && wxl_client_data_active(token) == 1, "Widget token mismatch");
        ++token;
    }
    expect(wxl_handler_client_data(empty, nullptr) == -1, "Null output must reject");
    expect(wxl_set_handler_client_data(0, 71) == -1, "Invalid receiver must reject");
    expect(wxl_client_data_active(0) == 0, "Token zero is reserved for clear");
    expect(wxl_handler_destroy(empty) == 0 && wxl_handler_destroy(untyped) == 0,
           "Standalone cleanup failed");
    expect(wxl_handler_client_data(empty, &token) == -1, "Dead receiver must reject");
}
}

int main() {
    auto old_assert_handler = wxSetAssertHandler(capture_assert);
    unsigned foreign_destroyed = 0;
    bool active = false;
    try {
        wxIdleEvent::SetMode(wxIDLE_PROCESS_ALL);
        expect(wxl_start(ignore_event) == 0, "Application startup failed");
        active = true;
        exercise_client_data(foreign_destroyed);
        exercise_binding_data();
        exercise_idle();
        expect(wxl_finish() == 0, "Application cleanup failed");
        active = false;
        expect(wxIdleEvent::GetMode() == wxIDLE_PROCESS_ALL, "Finish did not restore pre-session idle mode");
        expect(wxl_idle_mode() == -1 && wxl_wake_up_idle() == -1, "Inactive idle APIs must reject");
        expect(wxl_client_data_active(31) == -1, "Inactive client-data scan must reject");
        expect(foreign_destroyed == 1, "Foreign data was deleted more than once");
        expect(assertion_count.load() == 0, "Client-data operations triggered a wx assertion");
        wxSetAssertHandler(old_assert_handler);
        std::cout << "PASS: handler client data, binding user-data lifetime, idle metadata/mode/style restoration, foreign guards and zero wx assertions\n";
        return 0;
    } catch (const std::exception& failure) {
        if (active) wxl_finish();
        wxSetAssertHandler(old_assert_handler);
        std::cerr << "FAIL: " << failure.what() << "; native: " << wxl_last_error() << '\n';
        return 1;
    }
}
