// Foreign client-data states cannot be manufactured through the public Lisp API.
// Include the bridge in this executable only, without adding injection exports.
// Build with -DWXLAMBDA_BUILD_NATIVE_TESTS=ON; run wxlambda-client-data-native.
#define WXLAMBDA_NATIVE_TESTS
#include "../native/wxlambda.cpp"
#include <atomic>
#include <iostream>
#include <functional>
#include <thread>
#include <chrono>
#include <wx/listbox.h>
#include <fstream>

namespace {
std::atomic<unsigned> assertion_count{0};
void capture_assert(const wxString&, int, const wxString&, const wxString&, const wxString&) {
    ++assertion_count;
}
void expect(bool ok, const char* message);
bool binding_called = false;
unsigned menu_calls = 0;
int menu_checked_payload = -1;
int menu_integer_payload = 0;
wxl_handle menu_origin = 0;
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
    if (token == 903) { ++menu_calls; menu_checked_payload = event->checked; menu_integer_payload = event->integer; menu_origin = event->origin_id; }
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
class DialogTimer final : public wxTimer {
public:
    std::function<void()> action;
    explicit DialogTimer(std::function<void()> fn) : action(std::move(fn)) { }
    void Notify() override { action(); }
};
void exercise_generic_dialogs() {
    const auto frame = wxl_frame("Generic workflow regression", 240, 140);
    for (int buttons = 0; buttons < 4; ++buttons) {
        const auto message = wxl_message_dialog(frame, "message", 7, "title", 5, buttons, 0);
        expect(message && wxl_message_dialog_buttons(message) == buttons && as<wxDialog>(message)->GetParent() == as<wxFrame>(frame),
               "Prepared message configuration/parent failed");
        expect(as<wxDialog>(message)->FindWindow(buttons < 2 ? wxID_OK : wxID_YES), "Message controls must exist before ShowModal");
        int result = -1;
        DialogTimer accept([&] {
            expect(wxl_dialog_depth() == 1, "Message depth must count one actual loop");
            expect(wxl_dialog_end(message, buttons < 2 ? 2 : 0) == -1, "Message unoffered success must reject");
            wxCommandEvent event(wxEVT_BUTTON, buttons < 2 ? wxID_OK : wxID_YES);
            event.SetEventObject(as<wxDialog>(message)->FindWindow(event.GetId()));
            as<wxDialog>(message)->GetEventHandler()->ProcessEvent(event);
        }); accept.StartOnce(60);
        expect(wxl_dialog_show(message, &result) == 0 && result == (buttons < 2 ? 0 : 2), "Offered message button acceptance failed");
        DialogTimer cancel([&] { expect(wxl_dialog_end(message, 1) == 0, "Message abort cancel must always be offered"); }); cancel.StartOnce(60);
        expect(wxl_dialog_show(message, &result) == 0 && result == 1, "Message cancellation failed");
        delete as<wxDialog>(message);
    }
    const auto temporary_file = wxFileName::CreateTempFileName("/private/tmp/wxl-generic-workflow-");
    expect(wxRemoveFile(temporary_file) && wxMkdir(temporary_file), "Cannot prepare isolated file workflow directory");
    const std::string directory = temporary_file.ToStdString();
    const auto existing_path = directory + "/existing.txt";
    std::ofstream(existing_path) << "test";
    const auto cwd = wxGetCwd();
    const char* wildcard = "Text|*.txt|All|*";
    for (int scenario = 0; scenario < 4; ++scenario) {
        const bool save = scenario == 1;
        const auto file = wxl_file_dialog(frame, "Files", 5, directory.data(), directory.size(), save ? "existing.txt" : "missing.txt", save ? 12 : 11, wildcard, 16, save ? 1 : 0, save ? 0 : 1, save ? 1 : 0);
        expect(file && wxl_file_dialog_mode(file) == (save ? 1 : 0), "Generic file mode restoration failed");
        auto* native = as<wxGenericFileDialog>(file);
        expect(!save || native->HasFdFlag(wxFD_SAVE) && native->HasFdFlag(wxFD_OVERWRITE_PROMPT), "Save/overwrite flags were stripped");
        expect(wxl_set_file_dialog_filter_index(file, 1) == 0 && wxl_file_dialog_filter_index(file) == 1 && wxl_set_file_dialog_filter_index(file, 2) == -1,
               "File filter bounds failed");
        expect(wxl_set_file_dialog_path(file, existing_path.data(), existing_path.size()) == 0, "File path setter failed");
        if (!save) { const auto missing = directory + "/missing.txt"; expect(wxl_set_file_dialog_path(file, missing.data(), missing.size()) == 0, "Missing-file path setup failed"); }
        if (scenario == 2) expect(wxMkdir(temporary_file + "/NewName"), "Cannot prepare isolated NewDir error path");
        int result = -1;
        DialogTimer inner([&] {
            expect(wxl_dialog_depth() == 2 && actual_modals.size() == 2, "Internal prompt must have depth two");
            expect(dynamic_cast<wxMessageDialog*>(actual_modals.back().get()) == nullptr && dynamic_cast<wxGenericMessageDialog*>(actual_modals.back().get()),
                   "Internal native message must be redirected to generic GUI");
            expect(wxl_dialog_end(file, 0) == -1, "Outer success while internal prompt active must reject");
            if (scenario == 3) actual_modals.back().get()->EndModal(wxID_CANCEL); // Ordinary user cancellation, not abort-all.
            else expect(wxl_dialog_abort() == 1, "Nested modal workflow abort failed");
        });
        DialogTimer trigger([&] {
            inner.StartOnce(60);
            wxCommandEvent event;
            if (scenario == 2) native->OnNew(event); else native->OnOk(event);
            if (scenario == 3) {
                expect(wxl_dialog_depth() == 1 && wxl_dialog_modal(file) == 1, "Ordinary inner Cancel must preserve outer chooser");
                expect(wxl_dialog_end(file, 1) == 0, "Outer workflow cancellation failed");
            }
        }); trigger.StartOnce(60);
        expect(wxl_dialog_show(file, &result) == 0 && result == 1 && wxl_dialog_depth() == 0,
               "Nested generic workflow must cancel sequentially and restore depth");
        delete native;
        if (scenario == 2) expect(wxRmdir(temporary_file + "/NewName"), "Cannot remove isolated NewDir fixture");
    }
    for (int mode = 0; mode < 2; ++mode) {
        const char* name = mode ? "new-save.txt" : "existing.txt";
        const auto file = wxl_file_dialog(frame, "Accept", 6, directory.data(), directory.size(), name, std::strlen(name), wildcard, 16, mode, mode ? 0 : 1, mode ? 1 : 0);
        expect(file != 0, "File acceptance fixture failed");
        int result = -1;
        DialogTimer accept([&] {
            wxCommandEvent event(wxEVT_BUTTON, wxID_OK);
            event.SetEventObject(as<wxGenericFileDialog>(file)->FindWindow(wxID_OK));
            as<wxGenericFileDialog>(file)->GetEventHandler()->ProcessEvent(event);
        }); accept.StartOnce(60);
        expect(wxl_dialog_show(file, &result) == 0 && result == 0 && wxl_dialog_depth() == 0, "Actual file OK acceptance failed");
        size_t path_length = 0;
        const auto* path = wxl_file_dialog_path(file, &path_length);
        expect(path && std::string(path, path_length) == directory + "/" + name, "File accepted native path mismatch");
        delete as<wxGenericFileDialog>(file);
    }
    {
        expect(wxl_callback_error_pending(1) == 0, "Pending callback error latch setup failed");
        wxMessageDialog native_helper(as<wxFrame>(frame), "must not show", "pending", wxOK);
        expect(native_helper.ShowModal() == wxID_CANCEL && wxl_dialog_depth() == 0,
               "Pending callback error must skip native prompt before OS modal entry");
        wxGenericMessageDialog generic_helper(as<wxFrame>(frame), "must not show", "pending", wxOK);
        expect(generic_helper.ShowModal() == wxID_CANCEL && wxl_dialog_depth() == 0,
               "Pending callback error must skip generic prompt without recording depth");
        expect(wxl_callback_error_pending(0) == 0, "Pending callback error latch clear failed");
    }
    // The hook is session-wide: native helper prompts outside public ShowModal
    // must still redirect and abort without entering an NSAlert loop.
    {
        wxMessageDialog helper(as<wxFrame>(frame), "outside public show", "helper", wxOK);
        DialogTimer cancel([&] { expect(wxl_dialog_depth() == 1 && wxl_dialog_abort() == 1, "Outside workflow helper modal must be tracked/abortable"); });
        cancel.StartOnce(60);
        expect(helper.ShowModal() == wxID_CANCEL && wxl_dialog_depth() == 0, "Session hook must redirect a root native helper prompt");
    }
    // Cancellation during Show(true) occurs before wxDialog creates m_eventLoop.
    const auto early = wxl_message_dialog(frame, "early", 5, "early", 5, 0, 0);
    as<wxDialog>(early)->Bind(wxEVT_SHOW, [](wxShowEvent& event) { if (event.IsShown()) wxl_dialog_abort(); event.Skip(); });
    int result = -1;
    expect(wxl_dialog_show(early, &result) == 0 && result == 1 && wxl_dialog_depth() == 0,
           "Pre-loop cancellation must not be lost");
    delete as<wxDialog>(early);
    expect(wxGetCwd() == cwd, "File workflow must not change working directory");
    expect(wxRemoveFile(wxString::FromUTF8(existing_path)) && wxRmdir(temporary_file), "Cannot remove isolated file fixtures");
    delete as<wxFrame>(frame);
}
void exercise_dialogs() {
    const auto parent = wxl_frame("Dialog native regression", 240, 140);
    const auto text = wxl_text_entry_dialog(parent, "Message", 7, "Title", 5, "initial", 7);
    const char* choices[] = {"first", "second"};
    const auto choice = wxl_single_choice_dialog(parent, "Message", 7, "Choice", 6, choices, 2, 0);
    expect(text && choice && wxl_dialog_selection(choice) == 0, "Dialog construction failed");
    expect(wxl_show(text) == -1 && wxl_show_state(choice, 1) == -1, "Ordinary dialog showing must reject");
    size_t length = 0;
    expect(std::string(wxl_dialog_selection_text(choice, &length)) == "", "Choice string is empty before first accepted transfer");
    int result = -1;
    {
        wxCommandEvent outer_event(wxEVT_BUTTON);
        EventScope outer_scope(outer_event);
        CallbackScope outer_callback;
        expect(wxl_context_active(outer_scope.token) == 1, "Outer context setup failed");
        DialogTimer timer([&] {
            expect(wxl_dialog_modal(text) == 1, "Dialog must be natively modal");
            expect(wxl_context_active(outer_scope.token) == 0, "Outer context must suspend throughout modal scope");
            expect(wxl_dialog_show(choice, &result) == -1, "Nested modal must reject");
            const int destroy_text = wxl_destroy(text), destroy_parent = wxl_destroy(parent);
            expect(destroy_text == -1 && destroy_parent == -1, "Active dialog/ancestor destruction must reject");
            expect(wxl_close(parent, 1) == -1, "Active modal ancestor close must reject");
            wxCloseEvent forced_close(wxEVT_CLOSE_WINDOW, as<wxFrame>(parent)->GetId()); forced_close.SetCanVeto(false);
            as<wxFrame>(parent)->GetEventHandler()->ProcessEvent(forced_close);
            expect(wxl_handler_alive(parent) == 1 && wxl_dialog_modal(text) == 1,
                   "Queued/native forced close guard must preserve modal ancestor");
            expect(wxl_poll() == -1 && wxl_finish() == -1, "Modal pump/finish must reject");
            expect(wxl_dialog_end(text, 0) == 0 && wxl_dialog_modal(text) == 0, "Raw EndModal OK failed");
            expect(wxl_destroy(text) == -1 && wxl_destroy(parent) == -1, "Ending modal remains protected until ShowModal returns");
            expect(wxl_dialog_abort() == 1, "Already ending modal remains handled by abort");
        });
        timer.StartOnce(100);
        const int raw_status = wxl_dialog_show(text, &result);
        expect(raw_status == 0 && result == 0, "Raw modal OK did not return");
        expect(wxl_context_active(outer_scope.token) == 1, "Outer context must restore after modal scope");
    }
    auto* native_text = as<wxTextEntryDialog>(text);
    wxTextCtrl* entry = nullptr;
    for (auto* child : native_text->GetChildren()) if (auto* candidate = dynamic_cast<wxTextCtrl*>(child)) entry = candidate;
    expect(entry, "Text-entry child not found");
    {
        DialogTimer timer([&] { entry->ChangeValue("raw unsaved"); expect(wxl_dialog_end(text, 0) == 0, "Raw OK close failed"); });
        timer.StartOnce(100);
        expect(wxl_dialog_show(text, &result) == 0 && result == 0, "Text raw close failed");
        expect(std::string(wxl_dialog_value(text, &length)) == "initial", "Raw EndModal must not transfer text control data");
    }
    {
        DialogTimer timer([&] {
            entry->ChangeValue("accepted");
            wxCommandEvent button(wxEVT_BUTTON, wxID_OK); button.SetEventObject(native_text->FindWindow(wxID_OK));
            native_text->GetEventHandler()->ProcessEvent(button);
        }); timer.StartOnce(100);
        expect(wxl_dialog_show(text, &result) == 0 && result == 0, "Text accept button failed");
        expect(std::string(wxl_dialog_value(text, &length)) == "accepted", "OK button must transfer text data");
    }
    auto* native_choice = as<wxSingleChoiceDialog>(choice);
    wxListBox* list = nullptr;
    for (auto* child : native_choice->GetChildren()) if (auto* candidate = dynamic_cast<wxListBox*>(child)) list = candidate;
    expect(list, "Choice list child not found");
    {
        DialogTimer timer([&] { list->SetSelection(1); expect(wxl_dialog_end(choice, 0) == 0, "Choice raw OK close failed"); }); timer.StartOnce(100);
        expect(wxl_dialog_show(choice, &result) == 0 && wxl_dialog_selection(choice) == 0,
               "Raw EndModal must not transfer choice data");
    }
    {
        DialogTimer timer([&] {
            list->SetSelection(1); wxCommandEvent button(wxEVT_BUTTON, wxID_OK);
            button.SetEventObject(native_choice->FindWindow(wxID_OK)); native_choice->GetEventHandler()->ProcessEvent(button);
        }); timer.StartOnce(100);
        expect(wxl_dialog_show(choice, &result) == 0 && result == 0 && wxl_dialog_selection(choice) == 1,
               "Choice OK button must transfer selection");
        expect(std::string(wxl_dialog_selection_text(choice, &length)) == "second", "Choice accepted string mismatch");
    }
    {
        DialogTimer timer([&] { expect(wxl_dialog_abort() == 1, "Active abort failed"); }); timer.StartOnce(100);
        expect(wxl_dialog_show(text, &result) == 0 && result == 1, "Abort must return Cancel");
    }
    {
        DialogTimer timer([&] {
            wxCloseEvent close(wxEVT_CLOSE_WINDOW, as<wxTextEntryDialog>(text)->GetId());
            close.SetEventObject(as<wxTextEntryDialog>(text));
            as<wxTextEntryDialog>(text)->GetEventHandler()->ProcessEvent(close);
        }); timer.StartOnce(100);
        expect(wxl_dialog_show(text, &result) == 0 && result == 1, "Dialog close event must cancel modal");
    }
    expect(wxl_dialog_abort() == 0 && wxl_set_dialog_selection(choice, -1) == -1 && wxl_set_dialog_selection(choice, 2) == -1,
           "Dialog inactive/bounds guards failed");
    delete as<wxFrame>(parent);
    expect(wxl_handler_alive(text) == 0 && wxl_handler_alive(choice) == 0 && dialog_choice_counts.empty(),
           "Native parent deletion must clean dialog identities and choice count registry");
    for (const bool direct_schedule : {false, true}) {
        const auto doomed_parent = wxl_frame("Scheduled modal parent", 200, 100);
        const auto doomed_child = wxl_text_entry_dialog(doomed_parent, "", 0, "", 0, "", 0);
        const auto doomed_bar = wxl_menu_bar(doomed_parent);
        auto* native_parent = as<wxFrame>(doomed_parent);
        if (direct_schedule) wxTheApp->ScheduleForDestruction(native_parent);
        else expect(wxl_destroy(doomed_parent) == 0, "Parent destroy request failed");
        expect(wxTheApp->IsScheduledForDestruction(native_parent), "Expected native scheduled-deletion state");
        int untouched_result = 72;
        expect(wxl_dialog_show(doomed_child, &untouched_result) == -1 && untouched_result == 72 && !active_modal.get(),
               "Scheduled ancestor must reject modal entry before entering native loop");
        expect(wxl_handler_alive(doomed_parent) == 0 && wxl_handler_alive(doomed_child) == 0 && wxl_menu_alive(doomed_bar) == 0,
               "Scheduled ancestors must invalidate window and menu liveness");
    }

}
void exercise_menus() {
    const auto frame = wxl_frame("Menu native regression", 240, 140);
    const auto bar = wxl_menu_bar(frame);
    expect(bar && !wxl_menu_bar(frame), "Only one menu bar may attach to a frame");
    expect(!wxl_menu(bar, "", 0), "Empty menu title must reject");
    const char embedded_nul[] = {'a', 0, 'b'};
    expect(!wxl_menu(bar, embedded_nul, 3), "NUL menu title must reject");
    const auto menu = wxl_menu(bar, "&File", 5);
    expect(!wxl_menu_item(menu, 18004, "", 0, nullptr, 0, 0), "Empty item label must reject");
    expect(!wxl_menu_item(menu, 18004, "text", 4, embedded_nul, 3, 0), "NUL help must reject");
    const auto check = wxl_menu_item(menu, 18001, "&Check\tCtrl+K", 13, "help", 4, 1);
    const auto separator = wxl_menu_item(menu, 0, nullptr, 0, nullptr, 0, 2);
    size_t label_length = 999;
    expect(wxl_menu_label(bar, &label_length) == nullptr && label_length == 0 && *wxl_last_error(),
           "Wrong menu label receiver must reset length and set an error");
    const auto* tab_label = wxl_menu_label(menu, &label_length);
    expect(tab_label && std::string(tab_label, label_length) == "&File" && !*wxl_last_error(),
           "Successful menu label read must clear stale error");
    expect(wxl_menu_label(check, nullptr) == nullptr && *wxl_last_error(),
           "Missing menu label length output must reject");
    const auto* item_label = wxl_menu_label(check, &label_length);
    expect(item_label && label_length != 0 && !*wxl_last_error(),
           "Successful item label read must clear stale error");
    expect(wxl_set_menu_label(check, "", 0) == -1 && wxl_set_menu_label(menu, embedded_nul, 3) == -1,
           "Invalid label mutation must reject");
    expect(menu && check && separator && wxl_menu_count(bar) == 1 && wxl_menu_count(menu) == 2,
           "Menu resource construction/count failed");
    expect(!wxl_menu_item(menu, 18001, "dup", 3, nullptr, 0, 0), "Duplicate menu ID must reject");
    int id = 0;
    expect(wxl_menu_item_id(separator, &id) == 0 && id == wxID_SEPARATOR, "Separator ID mismatch");
    expect(wxl_menu_checked(check) == 0 && wxl_menu_item_kind(check) == 1, "Check item metadata mismatch");
    expect(wxl_bind(frame, WXL_EVENT_MENU, 903, 18001, wxID_ANY, 1, 0) == 0, "Menu action bind failed");
    expect(wxl_activate_menu(check) == 1 && menu_calls == 1, "Menu activation not handled");
    expect(wxl_menu_checked(check) == 1 && menu_checked_payload == 1,
           "ProcessCommand should toggle check item before callback");
    expect(menu_origin == frame || menu_origin == menu, "Unexpected menu command origin");
    std::cout << "Menu ProcessCommand origin handle=" << menu_origin << " frame=" << frame << " menu=" << menu << '\n';
    expect(wxl_activate_menu(check) == 1 && wxl_menu_checked(check) == 0 && menu_checked_payload == 0,
           "Second activation should untoggle check item");
    expect(wxl_set_menu_enabled(check, 0) == 1 && wxl_set_menu_enabled(check, 0) == 0,
           "Enable changed-result mismatch");
    expect(wxl_activate_menu(check) == 0 && menu_calls == 2, "Disabled activation should not deliver");
    expect(wxl_activate_menu(separator) == -1, "Separator activation must reject");
    expect(wxl_unbind(903) == 1, "Menu action unbind failed");
    const auto normal = wxl_menu_item(menu, 18005, "Normal", 6, nullptr, 0, 0);
    expect(wxl_bind(frame, WXL_EVENT_MENU, 903, 18005, wxID_ANY, 1, 0) == 0, "Normal action bind failed");
    expect(wxl_activate_menu(normal) == 1 && menu_integer_payload == -1 && menu_checked_payload == 1,
           "Normal menu command preserves native -1 integer/IsChecked interpretation");
    expect(wxl_menu_checked(normal) == -1, "Normal item check-state query must reject");
    expect(wxl_unbind(903) == 1, "Normal action unbind failed");
    expect(wxl_menu_destroy(menu) == 0 && wxl_menu_alive(menu) == 0 && wxl_menu_alive(check) == 0 && wxl_menu_count(bar) == 0,
           "Deleting menu must invalidate owned items");
    const auto another = wxl_menu(bar, "Edit", 4);
    const auto child = wxl_menu_item(another, 18002, "Item", 4, nullptr, 0, 0);
    expect(wxl_menu_destroy(bar) == 0 && wxl_menu_alive(another) == 0 && wxl_menu_alive(child) == 0,
           "Deleting bar must invalidate descendants");
    const auto last_bar = wxl_menu_bar(frame);
    const auto last_menu = wxl_menu(last_bar, "Last", 4);
    const auto last_item = wxl_menu_item(last_menu, 18003, "Last item", 9, nullptr, 0, 0);
    as<wxFrame>(frame)->Bind(wxEVT_DESTROY, [](wxWindowDestroyEvent&) { });
    delete as<wxFrame>(frame);
    expect(wxl_menu_alive(last_bar) == 0 && wxl_menu_alive(last_menu) == 0 && wxl_menu_alive(last_item) == 0,
           "Real frame deletion must invalidate menus despite consumed destroy event");
    expect(menu_resources.empty(), "Menu registry must not retain deleted resources");
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
    const wxl_handle controls[] = {frame, panel, wxl_button(panel, "button"), wxl_text(panel, "text", 4, 0, 1),
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


#if wxUSE_CLIPBOARD
#if defined(__WXMSW__) && wxUSE_OLE_CLIPBOARD
constexpr bool native_owns_failed_clipboard_transfer = false;
#else
constexpr bool native_owns_failed_clipboard_transfer = true;
#endif
class MemoryClipboard final : public wxClipboardBase {
public:
    bool opened = false, fail_open = false, fail_read = false, fail_write = false, persisted = true;
    bool legacy_only = false, owns_failure = native_owns_failed_clipboard_transfer;
    unsigned opens = 0, closes = 0;
    std::unique_ptr<wxDataObject> data;
    std::function<void()> during_read;
    bool Open() override { ++opens; if (fail_open) return false; opened = true; return true; }
    void Close() override { ++closes; opened = false; }
    bool IsOpened() const override { return opened; }
    bool AddData(wxDataObject* value) override { data.reset(value); return !fail_write; }
    bool SetData(wxDataObject* value) override { if (!fail_write || owns_failure) data.reset(value); return !fail_write; }
    bool IsSupported(const wxDataFormat& format) override {
        return data && (format == wxDF_TEXT || (!legacy_only && format == wxDF_UNICODETEXT));
    }
    bool GetData(wxDataObject& value) override {
        if (during_read) during_read();
        if (fail_read || !data) return false;
        dynamic_cast<wxTextDataObject&>(value).SetText(dynamic_cast<wxTextDataObject&>(*data).GetText());
        return true;
    }
    void Clear() override { data.reset(); }
    bool Flush() override { return persisted; }
};
void exercise_clipboard() {
    MemoryClipboard memory;
    test_clipboard = &memory;
    struct Reset { ~Reset() { test_clipboard = nullptr; } } reset;
    memory.UsePrimarySelection(true);
    size_t length = 99;
    int available = -1;
    expect(wxl_clipboard_text_available() == 0, "Empty clipboard reported text");
    expect(wxl_clipboard_text(&length, &available) && !available && length == 0, "No text must differ from empty text");
    for (const auto& text : {std::string(""), std::string("Unicode: λ 日本語 😀\nsecond line"), std::string(65536, 'x')}) {
        expect(wxl_set_clipboard_text(text.data(), text.size()) == 0, "Clipboard write failed");
        expect(wxl_clipboard_text_available() == 1, "Clipboard text unavailable after write");
        const auto result = wxl_clipboard_text(&length, &available);
        expect(result && available && std::string(result, length) == text, "Clipboard round trip changed text");
        expect(memory.IsUsingPrimarySelection() && !memory.IsOpened(), "Clipboard selection/open state leaked");
    }
    memory.legacy_only = true;
    expect(wxl_clipboard_text_available() == 1 && wxl_clipboard_text(&length, &available) && available,
           "Legacy text fallback failed");
    memory.during_read = [&] {
        expect(wxl_clipboard_text_available() == -1, "Reentrant clipboard operation must reject");
    };
    expect(wxl_clipboard_text(&length, &available) != nullptr, "Outer clipboard read failed after nested rejection");
    memory.during_read = nullptr;
    memory.fail_open = true;
    const auto closes = memory.closes;
    expect(wxl_clipboard_text_available() == -1 && memory.closes == closes && memory.IsUsingPrimarySelection(),
           "Failed open must restore selection without closing an unowned lock");
    memory.fail_open = false;
    memory.opened = true;
    expect(wxl_set_clipboard_text("blocked", 7) == -1 && memory.opened && memory.closes == closes,
           "Externally open clipboard must reject without closing");
    memory.opened = false;
    memory.fail_read = true;
    expect(!wxl_clipboard_text(&length, &available) && !memory.opened && memory.IsUsingPrimarySelection(),
           "Read failure leaked clipboard lock/selection");
    memory.fail_read = false;
    memory.fail_write = true;
    expect(wxl_set_clipboard_text("failed", 6) == -1 && !memory.opened, "Failed SetData leaked lock");
    memory.fail_write = false;
    struct CountedText : wxTextDataObject {
        unsigned& count;
        explicit CountedText(unsigned& n) : count(n) { }
        ~CountedText() override { ++count; }
    };
    for (const bool owns_failure : {false, true}) {
        unsigned destroyed = 0;
        memory.data.reset();
        memory.fail_write = true;
        memory.owns_failure = owns_failure;
        expect(!transfer_clipboard_data(memory, std::make_unique<CountedText>(destroyed), owns_failure),
               "Mock transfer must fail");
        memory.data.reset();
        expect(destroyed == 1, "Failed transfer object must be destroyed exactly once for either ownership policy");
    }
    memory.owns_failure = native_owns_failed_clipboard_transfer;
    memory.fail_write = false;
    const auto opens = memory.opens;
    expect(wxl_set_clipboard_text("a\0b", 3) == -1 && wxl_set_clipboard_text("\xff", 1) == -1 &&
           wxl_set_clipboard_text(nullptr, 1) == -1 && memory.opens == opens,
           "Invalid text must reject before touching clipboard");
    expect(!wxl_clipboard_text(nullptr, &available) && !wxl_clipboard_text(&length, nullptr), "Null outputs must reject");
    expect(wxl_flush_clipboard() == 1, "Flush success lost");
    memory.persisted = false;
    expect(wxl_flush_clipboard() == 0, "Unsupported flush must be false, not an error");
    int worker_result = 0;
    std::thread worker([&] { worker_result = wxl_clipboard_text_available(); });
    worker.join();
    expect(worker_result == -1, "Worker clipboard access must reject");
    expect(memory.opens == memory.closes + 1, "Successful opens and closes must balance (one failed open)");
}
#endif


void exercise_text_editing() {
    const auto frame = wxl_frame("Text ranges", 300, 200);
    const auto text = wxl_text(frame, "one\nλ 日本語 😀", std::strlen("one\nλ 日本語 😀"), 1, 0);
    expect(text && wxl_text_multiline(text) == 1 && wxl_text_editable(text) == 0, "Text construction flags lost");
    expect(wxl_text(frame, "a\0b", 3, 1, 1) == 0 && wxl_text(frame, "\xff", 1, 1, 1) == 0 &&
           wxl_text(frame, nullptr, 1, 1, 1) == 0 && wxl_text(frame, "", 0, 2, 1) == 0,
           "Invalid text constructor inputs must reject");
    int64_t end = 0, first = -1, last = -1;
    expect(wxl_text_position(text, 1, &end) == 0 && end > 4, "Missing native end position");
    expect(wxl_set_text_selection(text, 0, end) == 0 && wxl_text_selection(text, &first, &last) == 0 && first == 0 && last == end,
           "Native selection round trip failed");
    expect(wxl_replace_text(text, 0, end, "new\nλ", std::strlen("new\nλ")) == 0, "Read-only programmatic replacement failed");
    size_t length = 0;
    const auto value = wxl_get_text(text, &length);
    expect(value && std::string(value, length) == "new\nλ", "Explicit-length text read failed");
    expect(wxl_text_position(text, 1, &end) == 0 && wxl_set_text_insertion(text, end) == 0 &&
           wxl_text_position(text, 0, &last) == 0 && last == end, "Native insertion point failed");
    expect(wxl_set_text_selection(text, -1, end) == -1 && wxl_set_text_selection(text, end, 0) == -1 &&
           wxl_set_text_insertion(text, end + 1) == -1 && wxl_set_text_selection(text, 0, INT64_MAX) == -1 &&
           wxl_replace_text(text, 0, end + 1, "bad", 3) == -1, "Invalid native positions must reject without clamping");
    expect(wxl_set_text(text, "a\0b", 3, 0) == -1 && wxl_set_text(text, "", 0, 2) == -1 &&
           wxl_replace_text(text, 0, 0, "\xff", 1) == -1, "Invalid native text writes must reject");
    expect(!wxl_get_text(text, nullptr) && wxl_text_selection(text, nullptr, &last) == -1 &&
           wxl_text_position(text, 2, &last) == -1 && wxl_text_position(text, 0, nullptr) == -1,
           "Invalid text outputs/fields must reject");
    expect(wxl_text_multiline(frame) == -1 && wxl_text_editable(frame) == -1 &&
           wxl_set_text_editable(text, 2) == -1 && wxl_set_text_editable(text, 1) == 0,
           "Text type/editable checks failed");
    delete window(frame);
    expect(wxl_text_multiline(text) == -1 && wxl_replace_text(text, 0, 0, "", 0) == -1, "Stale text handle must reject");
}

int main() {
    std::thread([] { std::this_thread::sleep_for(std::chrono::seconds(10)); std::_Exit(124); }).detach();
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
        exercise_menus();
        exercise_dialogs();
        exercise_generic_dialogs();
        exercise_text_editing();
#if wxUSE_CLIPBOARD
        exercise_clipboard();
#endif
        expect(wxl_finish() == 0, "Application cleanup failed");
        active = false;
        expect(wxIdleEvent::GetMode() == wxIDLE_PROCESS_ALL, "Finish did not restore pre-session idle mode");
        expect(wxl_idle_mode() == -1 && wxl_wake_up_idle() == -1, "Inactive idle APIs must reject");
        expect(wxl_clipboard_text_available() == -1 && wxl_flush_clipboard() == -1 &&
               wxl_set_clipboard_text("inactive", 8) == -1, "Inactive clipboard APIs must reject");
        expect(wxl_client_data_active(31) == -1, "Inactive client-data scan must reject");
        expect(foreign_destroyed == 1, "Foreign data was deleted more than once");
        expect(assertion_count.load() == 0, "Client-data operations triggered a wx assertion");
        wxSetAssertHandler(old_assert_handler);
        std::cout << "PASS: handler client data, binding user-data lifetime, idle metadata/mode/style restoration, menu ownership/actions, dialog modal/transfer/parent guards, foreign guards and zero wx assertions\n";
        return 0;
    } catch (const std::exception& failure) {
        if (active) wxl_finish();
        wxSetAssertHandler(old_assert_handler);
        std::cerr << "FAIL: " << failure.what() << "; native: " << wxl_last_error() << '\n';
        return 1;
    }
}
