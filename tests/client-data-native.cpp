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
        expect(wxl_start(ignore_event) == 0, "Application startup failed");
        active = true;
        exercise_client_data(foreign_destroyed);
        exercise_binding_data();
        expect(wxl_finish() == 0, "Application cleanup failed");
        active = false;
        expect(wxl_client_data_active(31) == -1, "Inactive client-data scan must reject");
        expect(foreign_destroyed == 1, "Foreign data was deleted more than once");
        expect(assertion_count.load() == 0, "Client-data operations triggered a wx assertion");
        wxSetAssertHandler(old_assert_handler);
        std::cout << "PASS: handler client data, binding user-data self-unbind/scoped lifetime, foreign guards and zero wx assertions\n";
        return 0;
    } catch (const std::exception& failure) {
        if (active) wxl_finish();
        wxSetAssertHandler(old_assert_handler);
        std::cerr << "FAIL: " << failure.what() << "; native: " << wxl_last_error() << '\n';
        return 1;
    }
}
