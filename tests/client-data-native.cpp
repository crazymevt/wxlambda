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
std::function<void(uint64_t, const wxl_event*)> notebook_probe, list_probe, tree_probe, splitter_probe, toolbar_probe, accelerator_probe, deferred_event_probe, numeric_probe, radio_probe, toggle_probe, combo_probe, scroll_probe, tooltip_probe, date_probe, time_probe, search_probe, canvas_probe, grid_probe;
std::function<void(uint64_t,int)> ui_action_probe;
void ignore_ui_action(uint64_t token, int execute) { if (ui_action_probe) ui_action_probe(token, execute); }
void ignore_event(uint64_t token, const wxl_event* event) {
    if ((token == 904 || token == 905) && notebook_probe) notebook_probe(token, event);
    if (token >= 906 && token <= 908 && list_probe) list_probe(token, event);
    if ((token == 909 || token == 910) && tree_probe) tree_probe(token, event);
    if ((token == 911 || token == 912) && splitter_probe) splitter_probe(token, event);
    if ((token == 913 || token == 914) && toolbar_probe) toolbar_probe(token, event);
    if ((token == 915 || token == 916) && accelerator_probe) accelerator_probe(token, event);
    if (token == 917 && deferred_event_probe) deferred_event_probe(token, event);
    if (token >= 918 && token <= 920 && numeric_probe) numeric_probe(token, event);
    if ((token == 921 || token == 922) && radio_probe) radio_probe(token, event);
    if ((token == 923 || token == 924) && toggle_probe) toggle_probe(token, event);
    if (token>=925 && token<=927 && combo_probe) combo_probe(token,event);
    if (token==928 && scroll_probe) scroll_probe(token,event);
    if (token==929 && tooltip_probe) tooltip_probe(token,event);
    if ((token==930 || token==931 || token==932) && date_probe) date_probe(token,event);
    if ((token==933 || token==934 || token==935) && time_probe) time_probe(token,event);
    if ((token==936 || token==937 || token==938) && search_probe) search_probe(token,event);
    if (token==939 && canvas_probe) canvas_probe(token,event);
    if (token==940 && grid_probe) grid_probe(token,event);
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


void exercise_notebook() {
    const auto frame = wxl_frame("Notebook", 320, 240);
    const auto book = wxl_notebook(frame);
    const auto one = wxl_panel(book), two = wxl_panel(book), other = wxl_panel(frame);
    expect(book && wxl_notebook_count(book) == 0 && wxl_notebook_selection(book) == -1, "Empty notebook state failed");
    expect(wxl_notebook_add(book, one, "Editor λ", std::strlen("Editor λ"), 0) == 0 &&
           wxl_notebook_add(book, two, "Settings", 8, 0) == 0, "Notebook page addition failed");
    expect(wxl_notebook_page(book, 0) == one && wxl_notebook_count(book) == 2, "Notebook page identity/count failed");
    expect(wxl_notebook_add(book, one, "duplicate", 9, 0) == -1 && wxl_notebook_add(book, other, "wrong", 5, 0) == -1 &&
           wxl_notebook_add(book, other, "invalid", 7, 2) == -1, "Invalid page ownership/flags must reject");
    expect(wxl_destroy(one) == -1 && wxl_column(book) == -1, "Attached pages/custom notebook layout must reject");
    expect(wxl_set_notebook_selection(book, -1, 0) == -1 && wxl_set_notebook_selection(book, 2, 0) == -1 &&
           wxl_set_notebook_selection(book, 0, 2) == -1 && !wxl_notebook_page(book, 2), "Notebook index/flag checks failed");
    size_t length = 0;
    expect(!wxl_notebook_label(book, -1, &length) && !wxl_notebook_label(book, 0, nullptr) &&
           wxl_set_notebook_label(book, 0, "x\0y", 3) == -1, "Notebook label validation failed");
    const auto dialog = wxl_message_dialog(frame, "Must reject modal entry", 23, "Guard", 5, 0, 0);
    expect(dialog != 0, "Notebook guard probe dialog construction failed");
    unsigned book_calls = 0, parent_calls = 0;
    notebook_probe = [&](uint64_t token, const wxl_event* event) {
        token == 904 ? ++book_calls : ++parent_calls;
        expect(event->kind == WXL_EVENT_NOTEBOOK_CHANGING && event->selection == 1 && event->origin_id == book && event->family == WXL_FAMILY_NOTIFY,
               "Typed notebook snapshot metadata lost");
        int64_t index = -1;
        expect(wxl_event_field(event->context, 1, 16, &index) == 0 && index == 1, "Live book selection metadata lost");
        expect(wxl_set_event_field(event->context, 1, 16, 0) == -1 && !wxl_clone_event(event->context, 1),
               "Notebook selection metadata writes/clones must reject");
        int code = -1;
        expect(wxl_set_notebook_selection(book, 0, 0) == -1 && wxl_destroy(book) == -1 &&
               wxl_destroy(frame) == -1 && wxl_close(frame, 1) == -1 && wxl_dialog_show(dialog, &code) == -1 &&
               wxl_clipboard_text_available() == -1, "Actual native page callback bypassed operation guards");
        expect(wxl_context_set_skipped(event->context, 1) == 0, "Cannot propagate notebook event");
    };
    expect(wxl_bind(book, WXL_EVENT_NOTEBOOK_CHANGING, 904, wxID_ANY, wxID_ANY, 1, 0) == 0 &&
           wxl_bind(frame, WXL_EVENT_NOTEBOOK_CHANGING, 905, wxID_ANY, wxID_ANY, 1, 0) == 0, "Notebook event bindings failed");
    // Actual derived native event, outside bridge selection/AddPage guards.
    wxBookCtrlEvent event(wxEVT_NOTEBOOK_PAGE_CHANGING, window(book)->GetId(), 1, 0);
    event.SetEventObject(window(book));
    window(book)->GetEventHandler()->ProcessEvent(event);
    expect(book_calls == 1 && parent_calls == 1 && notebook_operations.empty(), "Native/propagated event guard did not unwind");
    expect(wxl_unbind(904) == 1 && wxl_unbind(905) == 1, "Notebook binding cleanup failed");
    notebook_probe = nullptr;
    expect(!wxl_owned_event(WXL_EVENT_NOTEBOOK_CHANGED, 1, "", 0, 0, 0, 0, 0), "Synthetic book event must reject");
    expect(wxl_set_notebook_selection(book, 1, 0) == 0 && wxl_notebook_selection(book) == 1, "Notebook navigation did not recover");
    delete window(frame);
    expect(wxl_notebook_count(book) == -1 && wxl_handler_alive(one) == 0 && wxl_handler_alive(two) == 0, "Notebook destruction did not invalidate pages");
}


void exercise_list_box() {
    const auto frame = wxl_frame("List", 320, 260);
    const char* labels[] = {"Alpha", "β"}; const size_t lengths[] = {5, std::strlen("β")};
    const auto list = wxl_list_box(frame, labels, lengths, 2, 1);
    expect(list && wxl_list_count(list) == 2 && wxl_list_selection(list) == 1, "List constructor failed");
    expect(!wxl_list_box(frame, nullptr, nullptr, 1, -1) && !wxl_list_box(frame, labels, nullptr, 2, -1) &&
           !wxl_list_box(frame, labels, lengths, 2, 2), "Invalid list arrays/selection must reject");
    const char* invalid[] = {"a\0b", "\xff"}; const size_t invalid_lengths[] = {3, 1};
    expect(!wxl_list_box(frame, invalid, invalid_lengths, 1, -1) &&
           !wxl_list_box(frame, invalid + 1, invalid_lengths + 1, 1, -1), "Invalid list UTF8/NUL must reject");
    size_t length = 0;
    expect(!wxl_list_item(list, -1, &length) && !wxl_list_item(list, 0, nullptr) && wxl_list_count(frame) == -1 &&
           wxl_list_delete(list, 2) == -1 && wxl_set_list_selection(list, 2, 0) == -1 &&
           wxl_set_list_selection(list, 0, 2) == -1, "List raw type/bounds/output checks failed");
    expect(wxl_list_append(list, "a\0b", 3) == -1 && wxl_set_list_item(list, 0, "\xff", 1) == -1,
           "Invalid list item writes must reject");
    const auto dialog = wxl_message_dialog(frame, "Guard", 5, "Guard", 5, 0, 0);
    expect(dialog != 0, "List guard dialog setup failed");
    unsigned generic_calls = 0, parent_calls = 0, convenience_calls = 0;
    list_probe = [&](uint64_t token, const wxl_event* event) {
        if (token == 906) ++generic_calls; else if (token == 907) ++parent_calls; else ++convenience_calls;
        expect(event->origin_id == list && event->selection >= 0 && event->family == WXL_FAMILY_COMMAND &&
               std::string(event->text, event->text_length) == "β", "List cached metadata lost");
        int code = -1;
        expect(wxl_destroy(list) == -1 && wxl_destroy(frame) == -1 && wxl_close(frame, 1) == -1 &&
               wxl_dialog_show(dialog, &code) == -1 && wxl_clipboard_text_available() == -1,
               "Actual list callback bypassed lifetime/native-loop guards");
        if (token != 908) {
            int64_t index = -1;
            expect(wxl_event_field(event->context, 1, 16, &index) == 0 && index == event->selection, "List live selection metadata lost");
            const auto clone = wxl_clone_event(event->context, 1);
            expect(clone && wxl_event_destroy(clone) == 0, "List event clone failed");
            expect(wxl_context_set_skipped(event->context, 1) == 0, "List propagation failed");
        }
        if (token == 906) {
            if (event->kind == WXL_EVENT_LISTBOX_SELECTION) {
                expect(wxl_list_append(list, "Added", 5) == 2 && wxl_list_delete(list, 0) == 0 && wxl_list_selection(list) == 0,
                       "Callback item mutation/selection preservation failed");
            } else expect(wxl_list_clear(list) == 0, "Activation callback clear failed");
        }
    };
    expect(wxl_on_list_selection(list, 908) == 0, "List convenience binding failed");
    for (const int kind : {WXL_EVENT_LISTBOX_SELECTION, WXL_EVENT_LISTBOX_DOUBLE_CLICK}) {
        expect(wxl_bind(list, kind, 906, wxID_ANY, wxID_ANY, 1, 0) == 0 &&
               wxl_bind(frame, kind, 907, wxID_ANY, wxID_ANY, 1, 0) == 0, "List generic bindings failed");
        wxCommandEvent event(native_event_type(kind), window(list)->GetId());
        event.SetEventObject(window(list)); event.SetInt(kind == WXL_EVENT_LISTBOX_SELECTION ? 1 : 0);
        event.SetString(wxString::FromUTF8("β")); event.SetExtraLong(1);
        // Actual native event dispatch, not the bridge selection setter.
        window(list)->GetEventHandler()->ProcessEvent(event);
        expect(wxl_unbind(906) == 1 && wxl_unbind(907) == 1 && list_callback_operations.empty(), "List callback guard/binding cleanup failed");
    }
    expect(generic_calls == 2 && parent_calls == 2 && convenience_calls == 1, "List callback paths were not all exercised");
    list_probe = nullptr;
    expect(wxl_off_list_selection(list) == 0 && wxl_list_count(list) == 0 && wxl_list_selection(list) == -1,
           "Activation clear did not leave empty/unselected list");
    const auto owned = wxl_owned_event(WXL_EVENT_LISTBOX_DOUBLE_CLICK, 1, "payload", 7, 0, 0, 0, 0);
    expect(owned && wxl_event_destroy(owned) == 0, "Detached list command construction failed");
    delete window(frame);
    expect(wxl_list_count(list) == -1 && wxl_list_append(list, "stale", 5) == -1, "Stale list handle must reject");
}


void exercise_tree() {
    const auto frame = wxl_frame("Tree", 320, 280);
#if defined(__WXQT__)
    expect(!wxl_tree(frame) && std::string(wxl_last_error()).find("unavailable on wxQt") != std::string::npos,
           "Qt tree construction must reject before publication");
    delete window(frame); return;
#endif
    const auto tree = wxl_tree(frame), other = wxl_tree(frame);
    auto add = [&](wxl_handle owner, wxl_handle parent, const char* text) {
        wxl_handle item = 0; expect(wxl_tree_add(owner, parent, text, std::strlen(text), &item) == 0 && item, "Tree add failed"); return item;
    };
    const auto root = add(tree, 0, "Root"), branch = add(tree, root, "Branch"), leaf = add(tree, branch, "Leaf λ");
    const auto other_root = add(other, 0, "Other");
    wxl_handle item = 0;
    expect(wxl_tree_query(tree, 0, &item) == 0 && item == root && wxl_tree_relative(leaf, 0, &item) == 0 && item == branch,
           "Tree root/parent identity failed");
    expect(wxl_tree_query(tree, 1, &item) == 0 && item == root && wxl_tree_select(tree, 0, 0) == -1, "Tree populated selection policy failed");
    expect(wxl_tree_add(other, root, "Wrong", 5, &item) == -1 && wxl_tree_select(other, leaf, 0) == -1 &&
           wxl_tree_add(tree, 0, "Duplicate", 9, &item) == -1, "Tree owner/root checks failed");
    expect(wxl_tree_add(tree, root, "a\0b", 3, &item) == -1 && wxl_tree_add(tree, root, "\xff", 1, &item) == -1 &&
           wxl_tree_add(tree, root, "x", 1, nullptr) == -1 && wxl_tree_relative(root, 3, &item) == -1 &&
           wxl_tree_query(tree, 2, &item) == -1 && wxl_tree_query(tree, 0, nullptr) == -1,
           "Tree raw input validation failed");
    size_t length = 0;
    expect(!wxl_tree_text(root, nullptr) && !wxl_tree_text(frame, &length) && wxl_set_tree_text(root, "\xff", 1) == -1 &&
           wxl_set_tree_expanded(root, 2) == -1 && wxl_tree_select(tree, root, 2) == -1 && wxl_tree_revision(nullptr) == -1,
           "Tree text/flags/output validation failed");
    const auto dialog = wxl_message_dialog(frame, "Guard", 5, "Guard", 5, 0, 0);
    expect(dialog, "Tree guard dialog setup failed");
    unsigned calls = 0;
    tree_probe = [&](uint64_t, const wxl_event* event) {
        ++calls; wxl_handle selected = 0; int code = -1;
        expect(event->tree_item == leaf && event->selection == -1 && event->family == WXL_FAMILY_NOTIFY &&
               wxl_event_tree_item(event->context, 1, &selected) == 0 && selected == leaf, "Tree typed event item metadata failed");
        expect(wxl_tree_delete(branch) == -1 && wxl_tree_add(tree, root, "Unsafe", 6, &selected) == -1 &&
               wxl_set_tree_text(root, "Unsafe", 6) == -1 && wxl_set_tree_expanded(root, 0) == -1 &&
               wxl_tree_select(tree, root, 0) == -1 && wxl_destroy(tree) == -1 && wxl_destroy(frame) == -1 &&
               wxl_close(frame, 1) == -1 && wxl_dialog_show(dialog, &code) == -1 && wxl_clipboard_text_available() == -1,
               "Actual tree callback bypassed mutation/lifetime/native-loop guards");
        expect(!wxl_clone_event(event->context, 1) && wxl_context_set_skipped(event->context, 1) == 0,
               "Tree cloning/propagation contract failed");
    };
    for (const int kind : {WXL_EVENT_TREE_CHANGING, WXL_EVENT_TREE_CHANGED}) {
        expect(wxl_bind(tree, kind, 909, wxID_ANY, wxID_ANY, 1, 0) == 0 && wxl_bind(frame, kind, 910, wxID_ANY, wxID_ANY, 1, 0) == 0,
               "Tree event binding failed");
        wxTreeEvent event(native_event_type(kind), window(tree)->GetId());
        event.SetEventObject(window(tree)); event.SetItem(tree_node(leaf).native);
        window(tree)->GetEventHandler()->ProcessEvent(event); // Outside bridge navigation guard.
        expect(wxl_unbind(909) == 1 && wxl_unbind(910) == 1 && tree_operations.empty(), "Tree callback guard did not unwind");
        expect(!wxl_owned_event(kind, 1, "", 0, 0, 0, 0, 0), "Synthetic derived tree events must reject");
    }
    expect(calls == 4, "Native/propagated tree callback paths incomplete");
    tree_probe = nullptr;
    expect(wxl_set_tree_expanded(root, 1) == 0 && wxl_set_tree_expanded(branch, 1) == 0 && wxl_tree_select(tree, leaf, 0) == 0 &&
           wxl_set_tree_expanded(branch, 0) == 0 && wxl_poll() == 0 && wxl_tree_query(tree, 1, &item) == 0 && item == branch,
           "Tree collapse selection did not normalize across idle");
    const auto old_native = tree_node(leaf).native;
    expect(wxl_tree_delete(branch) == 0 && wxl_poll() == 0 && wxl_tree_item_alive(leaf) == 0 && wxl_tree_item_alive(branch) == 0 &&
           wxl_tree_query(tree, 1, &item) == 0 && item == root && !wxl_tree_text(leaf, &length), "Tree subtree invalidation/selection failed");
    const auto replacement = add(tree, root, "Replacement");
    expect(replacement != leaf && tree_node(replacement).native.IsOk(), "Tree item token was reused");
    (void)old_native; // Native allocators may reuse this address; bridge tokens never do.
    as<wxTreeCtrl>(tree)->Delete(tree_node(replacement).native);
    expect(wxl_tree_item_alive(replacement) == 0, "Native deletion notification did not invalidate item token");
    unsigned teardown_calls = 0;
    tree_probe = [&](uint64_t, const wxl_event*) { ++teardown_calls; };
    expect(wxl_bind(tree, WXL_EVENT_TREE_CHANGED, 909, wxID_ANY, wxID_ANY, 1, 0) == 0 &&
           wxl_bind(frame, WXL_EVENT_TREE_CHANGED, 910, wxID_ANY, wxID_ANY, 1, 0) == 0, "Tree teardown bindings failed");
    delete window(tree); // Includes native base-destructor events after ManagedWindow cleanup.
    expect(!teardown_calls && !wxl_tree_item_alive(root), "Tree teardown exposed a dying origin/item");
    wxl_unbind(909); wxl_unbind(910); tree_probe = nullptr;
    delete window(frame);
    expect(wxl_tree_item_alive(other_root) == 0 && tree_nodes.empty() && tree_owners.empty(), "Tree owner teardown leaked item state");
}


struct SplitterProbe : FixedSplitter {
    using FixedSplitter::FixedSplitter;
    using wxSplitterWindow::SetSashPositionAndNotify;
};
class ScrollProbe : public wxScrolledWindow {
public:
    using wxScrolledWindow::wxScrolledWindow;
    std::function<void()> during_scroll;
    void ScrollWindow(int dx,int dy,const wxRect* rect=nullptr) override {
        if (during_scroll) during_scroll();
        wxScrolledWindow::ScrollWindow(dx,dy,rect);
    }
};
void exercise_scrolling_panels() {
    const auto frame=wxl_frame("Scroll",400,240);
    const auto viewport=wxl_scrolled_panel(frame,0,16);
    int x=-1,y=-1;
    expect(viewport && wxl_scroll_pair(viewport,0,&x,&y)==0 && x==0 && y==16,"Scroll default rate");
    expect(!wxl_scrolled_panel(frame,-1,1) && !wxl_scrolled_panel(frame,1,257) &&
           wxl_scroll_pair(frame,0,&x,&y)==-1 && wxl_scroll_pair(viewport,3,&x,&y)==-1 &&
           wxl_scroll_pair(viewport,0,nullptr,&y)==-1 && wxl_set_scroll_pair(viewport,2,1,1)==-1 &&
           wxl_set_scroll_pair(viewport,0,257,0)==-1 && wxl_set_scroll_pair(viewport,1,-1,0)==-1 &&
           wxl_set_scroll_pair(viewport,1,0,1000001)==-1 && wxl_fit_scroll_content(viewport)==-1,"Scroll raw validation");
    uint64_t handler=0; expect(wxl_window_handler(viewport,&handler)==-1,"Private scroll helper cannot be exposed");
    wxl_column(viewport); wxl_spacer(viewport,1800);
    expect(wxl_fit_scroll_content(viewport)==0 && wxl_scroll_pair(viewport,2,&x,&y)==0 && y>=1800,"Scroll fit extent");
    expect(wxl_set_scroll_pair(viewport,1,1000000,1000000)==0 && wxl_scroll_pair(viewport,1,&x,&y)==0 && x==0 && y>=0 && y<1000000,"Scroll native clamp");
    expect(wxl_set_scroll_pair(viewport,0,0,0)==0 && wxl_set_scroll_pair(viewport,1,20,20)==0 &&
           wxl_scroll_pair(viewport,1,&x,&y)==0 && x==0 && y==0,"Disabled scroll axes");
    // Test-only subclass injects supported native callback during virtual ScrollWindow.
    auto* probe=new ManagedWindow<ScrollProbe>(window(frame),wxID_ANY);
    const auto target=track(probe); probe->SetSize(300,150); probe->SetSizer(new wxBoxSizer(wxVERTICAL));
    probe->GetSizer()->AddSpacer(2000); probe->FitInside(); probe->SetScrollRate(0,16);
    const auto dialog=wxl_message_dialog(frame,"Scroll",6,"Probe",5,0,0);
    unsigned calls=0;
    scroll_probe=[&](uint64_t,const wxl_event*) {
        ++calls;
        expect(wxl_destroy(target)==-1 && wxl_destroy(frame)==-1 && wxl_close(frame,1)==-1,"Scroll native-operation lifetime guard");
        expect(wxl_set_scroll_pair(target,1,0,1)==-1 && wxl_set_scroll_pair(target,0,0,8)==-1 &&
               wxl_fit_scroll_content(target)==-1,"Scroll callback mutation guard");
        int result=-1; expect(wxl_poll()==-1 && wxl_clipboard_text_available()==-1 && wxl_dialog_show(dialog,&result)==-1,"Scroll native-operation entry guard");
    };
    expect(wxl_bind(frame,WXL_EVENT_CLICK,928,wxID_ANY,wxID_ANY,1,0)==0,"Scroll native probe binding");
    probe->during_scroll=[&] { wxCommandEvent event(wxEVT_BUTTON,probe->GetId()); event.SetEventObject(probe); probe->HandleWindowEvent(event); };
    expect(wxl_set_scroll_pair(target,1,0,5)==0 && calls>0 && input_operations.empty(),"Protected synchronous native scroll callback");
    probe->during_scroll={}; scroll_probe={}; expect(wxl_unbind(928)==1,"Scroll native probe unbind");
    // Oversized content is rejected by explicit operations, without claiming to police all native layout.
    wxl_spacer(viewport,1000001);
    expect(wxl_fit_scroll_content(viewport)==-1 && wxl_set_scroll_pair(viewport,1,0,1)==-1,"Oversized scroll content gate");
    expect(wxl_destroy(frame)==0 && wxl_poll()==0 && wxl_scroll_pair(viewport,0,&x,&y)==-1,"Scroll teardown with private helper intact");
}
void exercise_combo_boxes() {
    const auto frame=wxl_frame("Combo",460,250), panel=wxl_panel(frame);
    const char* labels[]={"First λ","Second"}; const size_t lengths[]={8,6};
    unsigned calls=0; bool constructing=true;
    combo_probe=[&](uint64_t,const wxl_event*) { ++calls; expect(!constructing,"Combo constructor callbacks suppressed"); };
    expect(wxl_bind(frame,WXL_EVENT_TEXT_CHANGE,927,wxID_ANY,wxID_ANY,1,0)==0,"Observe combo constructor");
    const auto combo=wxl_combo_box(panel,labels,lengths,2,"",0,1), other=wxl_combo_box(panel,nullptr,nullptr,0,"Custom",6,-1);
    expect(combo && other && !calls,"Combo construction"); constructing=false; expect(wxl_unbind(927)==1,"Constructor unbind");
    size_t size=0;
    expect(wxl_combo_field(combo,0)==1 && wxl_combo_field(combo,1)==2 && std::string(wxl_combo_text(combo,&size))=="Second","Initial combo state");
    const char* first=wxl_combo_item(combo,0,&size); expect(first && std::string(first,size)=="First λ","Combo UTF8 item");
    const char* duplicates[]={"a","A"}; const size_t ones[]={1,1};
    expect(!wxl_combo_box(panel,duplicates,ones,2,"",0,-1) && !wxl_combo_box(panel,labels,lengths,257,"",0,-1) &&
           !wxl_combo_box(panel,nullptr,nullptr,1,"",0,-1) && !wxl_combo_box(panel,labels,lengths,2,"bad",3,0),"Raw combo construction validation");
    expect(!wxl_combo_box(panel,labels,lengths,2,"a\nb",3,-1) && wxl_set_combo_text(combo,"a\rb",3,0)==-1 &&
           wxl_set_combo_text(combo,"a\0b",3,0)==-1 && wxl_set_combo_text(combo,"",0,2)==-1 &&
           wxl_set_combo_selection(combo,2,0)==-1 && wxl_set_combo_selection(combo,0,2)==-1 &&
           wxl_combo_field(frame,0)==-2 && wxl_combo_field(combo,2)==-2 && !wxl_combo_item(combo,2,&size),"Raw combo operations");
    const auto dialog=wxl_message_dialog(frame,"Combo",5,"Probe",5,0,0);
    combo_probe=[&](uint64_t token,const wxl_event* event) {
        ++calls;
        expect(event->origin_id==combo && event->family==WXL_FAMILY_COMMAND,"Combo native origin/family");
        expect(wxl_set_combo_selection(combo,1,0)==-1 && wxl_set_combo_text(combo,"Nested",6,0)==-1 &&
               wxl_destroy(combo)==-1 && wxl_destroy(panel)==-1 && wxl_close(frame,1)==-1,"Combo source/ancestor mutations");
        int result=-1;
        expect(wxl_poll()==-1 && wxl_clipboard_text_available()==-1 && wxl_dialog_show(dialog,&result)==-1,"Combo modal/clipboard/loop gate");
        expect(wxl_set_combo_text(other,"Sync",4,0)==0,"Combo silent cross-control write");
        if (event->kind==WXL_EVENT_COMBO_SELECTION) {
            expect(event->selection==0 && std::string(event->text,event->text_length)=="First λ","Combo selection snapshot");
            int64_t selected=-1;
            expect(wxl_event_field(event->context,1,16,&selected)==0 && selected==0 && !wxl_clone_event(event->context,1) &&
                   wxl_set_event_field(event->context,1,16,1)==-1,"Combo native selection read-only/no clone");
        }
        if (event->kind==WXL_EVENT_TEXT_CHANGE) {
            expect(wxl_set_event_text(event->context,1,"Metadata",8)==-1,"Combo source-bound TEXT metadata cannot be set");
        } else {
            expect(wxl_set_event_text(event->context,1,"Metadata",8)==0,"Combo selection text is metadata");
            size_t length=0; expect(std::string(wxl_combo_text(combo,&length))=="First λ","Metadata must not edit combo");
            expect(wxl_set_event_text(event->context,1,"First λ",8)==0,"Restore metadata before parent probe");
        }
        if (token==925) expect(wxl_context_set_skipped(event->context,1)==0,"Combo propagation");
    };
    auto send=[&](int kind) {
        expect(wxl_bind(combo,kind,925,wxID_ANY,wxID_ANY,1,0)==0 && wxl_bind(frame,kind,926,wxID_ANY,wxID_ANY,1,0)==0,"Combo native bindings");
        wxCommandEvent event(kind==WXL_EVENT_COMBO_SELECTION ? wxEVT_COMBOBOX : wxEVT_TEXT,window(combo)->GetId());
        event.SetEventObject(window(combo)); event.SetInt(0); event.SetString("First λ"); window(combo)->HandleWindowEvent(event);
        expect(wxl_unbind(925)==1 && wxl_unbind(926)==1,"Combo unbind");
    };
    expect(wxl_set_combo_selection(combo,0,0)==0 && !calls,"Silent combo select");
    send(WXL_EVENT_COMBO_SELECTION); send(WXL_EVENT_TEXT_CHANGE);
    expect(calls==4 && input_operations.empty(),"Direct combo/text native propagation independent of setter guards");
    combo_probe={};
    expect(wxl_set_combo_selection(combo,-1,0)==0 && wxl_combo_field(combo,0)==-1 && std::string(wxl_combo_text(combo,&size)).empty(),"Combo clear selection and text");
    expect(wxl_set_combo_text(combo,"Custom λ",9,0)==0 && std::string(wxl_combo_text(combo,&size))=="Custom λ","Combo arbitrary Unicode text");
    expect(wxl_destroy(frame)==0 && wxl_poll()==0 && wxl_combo_field(combo,0)==-2 && !wxl_combo_text(combo,&size),"Combo teardown");
}
void exercise_toggle_buttons() {
    const auto frame=wxl_frame("Toggle",400,240), panel=wxl_panel(frame);
    const auto toggle=wxl_toggle_button(panel,"Preview λ",1), other=wxl_toggle_button(panel,"Other",0);
    expect(toggle && other && wxl_toggle_button_checked(toggle)==1 && wxl_toggle_button_checked(other)==0,"Toggle construction/state");
    expect(std::string(wxl_toggle_button_label(toggle))=="Preview λ" &&
           wxl_set_toggle_button_label(toggle,"Paused λ")==0 && std::string(wxl_toggle_button_label(toggle))=="Paused λ","Toggle UTF8 labels");
    expect(!wxl_toggle_button(panel,"Bad",2) && wxl_toggle_button_checked(frame)==-1 &&
           wxl_set_toggle_button_checked(toggle,2,0)==-1 && wxl_set_toggle_button_checked(toggle,1,2)==-1 &&
           !wxl_toggle_button_label(panel) && wxl_set_toggle_button_label(panel,"Bad")==-1,"Raw toggle validation");
    const auto dialog=wxl_message_dialog(frame,"Toggle",6,"Probe",5,0,0);
    unsigned calls=0; bool expected=false;
    toggle_probe=[&](uint64_t token,const wxl_event* event) {
        ++calls;
        expect(event->checked==expected && event->integer==expected && event->origin_id==toggle &&
               event->family==WXL_FAMILY_COMMAND && event->selection==-1,"Toggle native payload");
        int64_t value=-1;
        expect(wxl_event_field(event->context,1,15,&value)==0 && value==expected,"Toggle current checked metadata");
        expect(wxl_set_toggle_button_checked(toggle,1,0)==-1 && wxl_set_toggle_button_label(toggle,"Nested")==-1 &&
               wxl_destroy(toggle)==-1 && wxl_destroy(panel)==-1 && wxl_close(frame,1)==-1,"Toggle source/ancestor mutation protection");
        int result=-1;
        expect(wxl_poll()==-1 && wxl_clipboard_text_available()==-1 && wxl_dialog_show(dialog,&result)==-1,"Toggle entry/transfer gates");
        expect(wxl_set_toggle_button_checked(other,expected,0)==0 && wxl_set_toggle_button_label(other,"Synced")==0,"Silent other-toggle updates");
        expect(!wxl_clone_event(event->context,1) && wxl_set_event_field(event->context,1,15,1)==-1,"Toggle clone/semantic setter refuse");
        if (token==923) expect(wxl_context_set_skipped(event->context,1)==0,"Toggle native propagation");
    };
    expect(wxl_bind(toggle,WXL_EVENT_TOGGLE_BUTTON_CHANGE,923,wxID_ANY,wxID_ANY,1,0)==0 &&
           wxl_bind(frame,WXL_EVENT_TOGGLE_BUTTON_CHANGE,924,wxID_ANY,wxID_ANY,1,0)==0,"Toggle bindings");
    expect(wxl_set_toggle_button_checked(toggle,0,0)==0 && calls==0,"Silent toggle write");
    wxCommandEvent event(wxEVT_TOGGLEBUTTON,window(toggle)->GetId()); event.SetEventObject(window(toggle)); event.SetInt(0);
    window(toggle)->HandleWindowEvent(event);
    expect(calls==2 && input_operations.empty(),"Actual toggle callback guards independent of setter");
    expect(wxl_set_toggle_button_checked(toggle,0,1)==0 && calls==4,"Unchanged toggle notify");
    expected=true;
    expect(wxl_set_toggle_button_checked(toggle,1,1)==0 && calls==6,"Pressed toggle payload");
    expect(wxl_unbind(923)==1 && wxl_unbind(924)==1,"Toggle unbind"); toggle_probe={};
    expect(wxl_destroy(frame)==0 && wxl_poll()==0 && wxl_toggle_button_checked(toggle)==-1,"Toggle cleanup");
}
void exercise_radio_groups() {
    const auto frame = wxl_frame("Radio groups", 440, 300), panel = wxl_panel(frame);
    const char* labels[] = {"First λ", "Second"};
    const auto group = wxl_radio_box(panel, "Choices", labels, 2, 1, 0);
    const auto other = wxl_radio_box(panel, "Other", labels, 2, 1, 1);
    expect(group && other && wxl_radio_field(group,0)==1 && wxl_radio_field(group,1)==2, "Radio construction");
    expect(std::string(wxl_radio_item(group,0))=="First λ", "Radio UTF8 item copy");
    expect(!wxl_radio_box(panel,"Bad",labels,1,0,0) && !wxl_radio_box(panel,"Bad",labels,2,2,0) &&
           !wxl_radio_box(panel,"Bad",nullptr,2,0,0) && !wxl_radio_box(panel,"Bad",labels,2,0,2), "Raw radio bounds");
    expect(wxl_radio_field(frame,0)==-1 && wxl_radio_field(group,2)==-1 && !wxl_radio_item(group,2) &&
           wxl_set_radio_selection(group,-1,0)==-1 && wxl_set_radio_selection(group,2,0)==-1 && wxl_set_radio_selection(group,0,2)==-1, "Raw radio operations");
    unsigned calls=0;
    radio_probe = [&](uint64_t token, const wxl_event* event) {
        ++calls;
        expect(event->selection==0 && event->origin_id==group && event->family==WXL_FAMILY_COMMAND &&
               std::string(event->text,event->text_length)=="First λ", "Radio native snapshot payload");
        expect(wxl_set_radio_selection(group,1,0)==-1 && wxl_destroy(group)==-1 && wxl_destroy(panel)==-1 &&
               wxl_close(frame,1)==-1 && wxl_poll()==-1 && wxl_clipboard_text_available()==-1, "Radio source lifetime/entry guard");
        expect(wxl_set_radio_selection(other,0,0)==0, "Radio silent cross-group write");
        expect(!wxl_clone_event(event->context,1), "Radio clone rejects");
        if (token==921) expect(wxl_context_set_skipped(event->context,1)==0,"Radio propagation");
    };
    expect(wxl_bind(group,WXL_EVENT_RADIO_SELECTION,921,wxID_ANY,wxID_ANY,1,0)==0 &&
           wxl_bind(frame,WXL_EVENT_RADIO_SELECTION,922,wxID_ANY,wxID_ANY,1,0)==0,"Radio native bindings");
    expect(wxl_set_radio_selection(group,0,0)==0 && calls==0, "Radio silent write");
    // Direct native-class delivery has no setter operation guard to mask missing origin protection.
    wxCommandEvent event(wxEVT_RADIOBOX,window(group)->GetId()); event.SetEventObject(window(group));
    event.SetInt(0); event.SetString("First λ"); window(group)->HandleWindowEvent(event);
    expect(calls==2 && input_operations.empty(),"Radio native propagation/guard release");
    expect(wxl_set_radio_selection(group,0,1)==0 && calls==4,"Explicit unchanged radio notification");
    expect(wxl_unbind(921)==1 && wxl_unbind(922)==1,"Radio unbind"); radio_probe={};
    expect(wxl_destroy(frame)==0 && wxl_poll()==0 && wxl_radio_field(group,0)==-1,"Radio teardown");
}
void exercise_numeric_input() {
    const auto frame=wxl_frame("Numeric",560,360), panel=wxl_panel(frame);
    unsigned calls=0; bool construction=true;
    numeric_probe=[&](uint64_t,const wxl_event*) { ++calls; expect(!construction,"Constructor must not deliver numeric/text callbacks"); };
    expect(wxl_bind(frame,WXL_EVENT_TEXT_CHANGE,920,wxID_ANY,wxID_ANY,1,0)==0,"Observe constructor silence");
    const auto slider=wxl_numeric(panel,0,-1000000,1000000,1000000,1), spin=wxl_numeric(panel,1,-1000000,1000000,-1000000,0);
    expect(slider && spin && calls==0,"Create bounded signed numeric controls silently"); construction=false;
    expect(wxl_unbind(920)==1,"Unbind constructor observer");
    int value=0;
    expect(wxl_numeric_field(slider,0,&value)==0 && value==1000000 && wxl_numeric_field(spin,0,&value)==0 && value==-1000000,"Endpoint round trips");
    expect(wxl_numeric(panel,0,1,1,1,0)==0 && wxl_numeric(panel,1,-1000001,1,0,0)==0 && wxl_numeric(panel,1,0,10,0,1)==0,"Raw constructor range/style validation");
    expect(wxl_set_numeric_value(slider,0,2)==-1 && wxl_set_numeric_range(spin,1,1)==-1 && wxl_numeric_field(frame,0,&value)==-1 && wxl_numeric_field(spin,3,&value)==-1 && wxl_numeric_field(slider,0,nullptr)==-1,"Raw operation validation");
    expect(wxl_set_numeric_range(slider,0,10)==0 && wxl_numeric_field(slider,0,&value)==0 && value==10,"Silent upper clamp");
    expect(wxl_set_numeric_range(spin,0,10)==0 && wxl_numeric_field(spin,0,&value)==0 && value==0,"Silent lower clamp");
    expect(wxl_set_numeric_value(slider,7,0)==0 && wxl_set_numeric_value(spin,7,0)==0,"Set numeric fixtures");
    const auto dialog=wxl_message_dialog(frame,"Numeric",7,"Probe",5,0,0);
    wxl_handle current=slider, other=spin;
    numeric_probe=[&](uint64_t token,const wxl_event* event) {
        ++calls;
        expect(wxl_destroy(current)==-1 && wxl_destroy(panel)==-1 && wxl_close(frame,1)==-1,"Native numeric source/ancestor guard");
        expect(wxl_set_numeric_value(current,3,0)==-1 && wxl_set_numeric_range(current,0,5)==-1,"Same numeric callback write rejection");
        expect(wxl_set_numeric_value(other,7,0)==0,"Silent cross-control synchronization");
        int result=-1;
        expect(wxl_dialog_show(dialog,&result)==-1 && wxl_clipboard_text_available()==-1 && wxl_poll()==-1,"Numeric modal/clipboard/pump gate");
        if (event->kind==WXL_EVENT_SLIDER_CHANGE || event->kind==WXL_EVENT_SPIN_CHANGE) {
            expect(event->integer==7 && event->origin_id==current && !wxl_clone_event(event->context,1),"Native numeric payload/origin/no clone");
            if (event->kind==WXL_EVENT_SPIN_CHANGE) expect(wxl_context_set_vetoed(event->context,1)==0,"Spin notify metadata veto");
        }
        if (token==918) expect(wxl_context_set_skipped(event->context,1)==0,"Numeric propagation");
    };
    auto send=[&](int kind) {
        const auto object=current;
        expect(wxl_bind(current,kind,918,wxID_ANY,wxID_ANY,1,0)==0 && wxl_bind(frame,kind,919,wxID_ANY,wxID_ANY,1,0)==0,"Numeric binding pair");
        if (kind==WXL_EVENT_SPIN_CHANGE) {
            wxSpinEvent event(wxEVT_SPINCTRL,window(object)->GetId()); event.SetEventObject(window(object)); event.SetPosition(7);
            window(object)->HandleWindowEvent(event);
        } else {
            wxCommandEvent event(kind==WXL_EVENT_SLIDER_CHANGE ? wxEVT_SLIDER : wxEVT_TEXT,window(object)->GetId());
            event.SetEventObject(window(object)); event.SetInt(7); event.SetString("7"); window(object)->HandleWindowEvent(event);
        }
        expect(wxl_unbind(918)==1 && wxl_unbind(919)==1,"Unbind numeric pair");
    };
    send(WXL_EVENT_SLIDER_CHANGE);
    current=spin; other=slider; send(WXL_EVENT_SPIN_CHANGE); send(WXL_EVENT_TEXT_CHANGE);
    expect(calls==6 && input_operations.empty(),"Native numeric/forwarded text propagation guards");
    numeric_probe={};
    // Raw native edit injection is test-only; public setters accept integers only.
    for (const auto& text : {"-", "NaN", "999999999999999999999999999999999999"}) {
        { wxEventBlocker blocker(window(spin)); as<wxSpinCtrl>(spin)->SetValue(wxString::FromUTF8(text)); }
        expect(wxl_numeric_field(spin,0,&value)==-1,"Invalid/intermediate spin edit rejected before native numeric parsing");
        expect(wxl_set_numeric_value(spin,7,0)==0 && wxl_numeric_field(spin,0,&value)==0 && value==7,"Integer setter recovers spin edit");
    }
    expect(wxl_event_type(WXL_EVENT_SLIDER_CHANGE)==wxEVT_SLIDER && wxl_event_type(WXL_EVENT_SPIN_CHANGE)==wxEVT_SPINCTRL,"New native event tags");
    delete window(frame); expect(input_owners.empty() && input_operations.empty(),"Numeric native teardown registry cleanup");
    expect(wxl_numeric_field(slider,0,&value)==-1,"Stale numeric query rejects");
}

void exercise_ui_timers() {
    const auto frame=wxl_frame("UI timers",500,300), child=wxl_panel(frame);
    expect(wxl_start_ui_timer(0,1,0,0)==-1 && wxl_start_ui_timer(6000,0,0,0)==-1 && wxl_start_ui_timer(6000,86400001,0,0)==-1 && wxl_start_ui_timer(6000,1,2,0)==-1,"Timer raw validation");
    expect(wxl_start_ui_timer(6000,1000,0,frame)==0 && wxl_start_ui_timer(6000,1,0,0)==-1 && wxl_defer_ui_action(6000,0)==-1,"Timer identity isolation");
    auto* periodic=ui_timers.at(6000).get();
    periodic->Notify(); expect(!periodic->due,"Reused native ID cannot trigger early first tick");
    // Deterministic private notifications exercise coalescing and merged ordering
    // independently of OS timing. Real delivery is covered on both Lisps.
    periodic->not_before=std::chrono::steady_clock::time_point::min();
    expect(wxl_defer_ui_action(6001,frame)==0,"Queue before due timer");
    periodic->Notify(); const auto sequence=periodic->sequence;
    for (int tick=0; tick<20; ++tick) periodic->Notify();
    expect(periodic->due && periodic->sequence==sequence,"Repeated native ticks coalesce to one due item");
    expect(wxl_defer_ui_action(6002,frame)==0,"Queue after due timer");
    std::vector<uint64_t> order; bool dropped=false;
    ui_action_probe=[&](uint64_t token,int kind) {
        expect(!timer_notify_depth && ui_action_running && !callback_depth,"Timer functions enter safe idle phase");
        order.push_back(token);
        if (token==6000) {
            expect(kind==2 && wxl_ui_timer_active(token)==1,"Periodic timer retains active identity");
            expect(wxl_cancel_ui_timer(token)==1 && wxl_cancel_ui_timer(token)==0,"Periodic self cancellation");
        }
        if (token==6003) expect(kind==3 && wxl_ui_timer_active(token)==0 && wxl_cancel_ui_timer(token)==0,"One-shot retires before invocation");
        if (token==6004) { expect(kind==0,"Dead timer owner is root cleanup only"); dropped=true; }
    };
    expect(wxl_process_pending_events()==0 && order.empty(),"Pending-only does not drain due timers");
    for (int attempt=0; order.size()<3 && attempt<30; ++attempt) expect(wxl_poll()==0,"Drain merged timer/action queue");
    expect(order==std::vector<uint64_t>({6001,6000,6002}),"Timer/action due order is fair FIFO");
    expect(wxl_start_ui_timer(6003,1000,1,frame)==0,"One-shot start");
    auto* once=ui_timers.at(6003).get(); once->not_before=std::chrono::steady_clock::time_point::min(); once->Notify();
    expect(!once->IsRunning() && wxl_ui_timer_active(6003)==1,"Stopped one-shot remains active while due");
    for (int attempt=0; order.size()<4 && attempt<30; ++attempt) expect(wxl_poll()==0,"Deliver one-shot");
    expect(order.size()==4 && order.back()==6003,"One-shot delivered once");
    expect(wxl_start_ui_timer(6004,86400000,0,child)==0,"Owner lifetime timer"); delete window(child);
    auto* expired=ui_timers.at(6004).get(); expired->Notify();
    expect(expired->retired && !expired->IsRunning(),"Owner expiry stops native timer without self-deletion in Notify");
    for (int attempt=0; !dropped && attempt<30; ++attempt) expect(wxl_poll()==0,"Retire dead owner before long interval");
    expect(dropped && wxl_ui_timer_active(6004)==0,"Idle owner expiry releases timers before interval");
    for (uint64_t token=7000; token<7256; ++token) expect(wxl_start_ui_timer(token,86400000,0,0)==0,"Timer count bound fill");
    expect(wxl_start_ui_timer(7256,1,0,0)==-1,"Timer capacity bound");
    for (uint64_t token=7000; token<7256; ++token) expect(wxl_cancel_ui_timer(token)==1,"Timer count cleanup");
    std::thread([] { expect(wxl_start_ui_timer(8000,1,0,0)==-1 && wxl_cancel_ui_timer(8000)==-1 && wxl_ui_timer_active(8000)==-1,"Timer API rejects worker thread"); }).join();
    expect(ui_timers.empty(),"No timer records leaked"); ui_action_probe={}; delete window(frame);
}

void exercise_deferred_actions() {
#if wxUSE_CLIPBOARD
    MemoryClipboard memory;
    auto* previous_clipboard=test_clipboard; test_clipboard=&memory;
    struct RestoreDeferredClipboard { wxClipboardBase* previous; ~RestoreDeferredClipboard() { test_clipboard=previous; } } restore_clipboard{previous_clipboard};
#endif
    const auto frame=wxl_frame("Deferred",640,360), child=wxl_panel(frame);
    const int key[]={1001}, mods[]={0}, command[]={32301};
    expect(wxl_set_frame_accelerators(frame,key,mods,command,1)==0,"Deferred test accelerator");
    const auto dialog=wxl_message_dialog(frame,"Deferred modal",14,"Probe",5,0,0);
    const auto gui_entry_depth=native_entry_depth;
    std::vector<std::thread> wrong_threads;
    for (unsigned worker=0; worker<4; ++worker) wrong_threads.emplace_back([] {
        for (unsigned attempt=0; attempt<16; ++attempt)
            expect(wxl_defer_ui_action(9000,0)==-1 && native_entry_depth==0,"Wrong-thread UI queue call rejected without corrupting entry depth");
    });
    for (auto& worker:wrong_threads) worker.join();
    expect(native_entry_depth==gui_entry_depth,"GUI native-entry depth is thread-local");
    unsigned calls=0; bool in_origin=false, in_action=false, modal_seen=false, dropped=false;
    std::vector<uint64_t> order;
    as<wxDialog>(dialog)->Bind(wxEVT_SHOW,[&](wxShowEvent& event) {
        if (event.IsShown()) {
            modal_seen=true; expect(in_action && calls==1,"Modal phase before following action");
            wxTheApp->ProcessIdle();
            expect(calls==1 && wxl_ui_action_pending(2002)==1,"Modal idle must not reenter deferred queue");
            wxl_dialog_abort();
        }
        event.Skip();
    });
    ui_action_probe=[&](uint64_t token,int execute) {
        expect(ui_action_running && !callback_depth && EventScope::stack.empty() && !in_origin && !in_action,"Safe action entry");
        expect(wxl_ui_action_pending(token)==0 && wxl_cancel_ui_action(token)==0,"Remove pending identity before entry");
        if (!execute) { expect(token==2005,"Unexpected dropped token"); dropped=true; return; }
        in_action=true; ++calls; order.push_back(token);
        expect(wxl_poll()==-1 && wxl_process_pending_events()==-1 && wxl_run()==-1 && wxl_finish()==-1,"No action-phase loop/finish recursion");
        if (token==2001) {
            expect(wxl_defer_ui_action(2001,0)==-1 && wxl_defer_ui_action(2003,frame)==0,"Active token rejection and nested enqueue");
            int result=-1;
            expect(wxl_dialog_show(dialog,&result)==0 && result==1,"Deferred modal cancellation");
            expect(wxl_clipboard_text_available()>=0,"Deferred clipboard availability");
#if wxUSE_CLIPBOARD
            const char text[]=u8"Deferred λ 日本語 😀";
            expect(wxl_set_clipboard_text(text,sizeof(text)-1)==0,"Deferred isolated clipboard write");
            size_t length=0; int available=0;
            const auto* copied=wxl_clipboard_text(&length,&available);
            expect(copied && available==1 && length==sizeof(text)-1 && std::memcmp(text,copied,length)==0,"Deferred isolated clipboard read");
#endif
            const auto bar=wxl_toolbar(frame), status=wxl_status_bar(frame,1), book=wxl_notebook(frame), page=wxl_panel(book);
            expect(bar && status && book && page && wxl_notebook_add(book,page,"Page",4,0)==0,"Deferred structural construction");
            expect(wxl_add_tool(bar,1,"Tool",4,6,0,nullptr,0)==0 && wxl_realize_toolbar(bar)==0,"Deferred toolbar realization");
            expect(wxl_destroy(bar)==0 && wxl_destroy(status)==0 && wxl_destroy(book)==0,"Deferred structural deletion");
            expect(wxl_set_frame_accelerators(frame,key,mods,command,1)==0,"Deferred table replacement");
        }
        if (token==2006) expect(wxl_callback_error_pending(1)==0,"Pause deferred queue for error");
        in_action=false;
    };
    deferred_event_probe=[&](uint64_t,const wxl_event*) {
        in_origin=true;
        expect(wxl_defer_ui_action(2001,frame)==0 && wxl_defer_ui_action(2002,frame)==0,"Callback scheduling");
        wxTheApp->ProcessPendingEvents(); wxTheApp->ProcessIdle();
        expect(calls==0,"No callback/nested-pump delivery"); in_origin=false;
    };
    expect(wxl_bind(frame,WXL_EVENT_MENU,917,32301,32301,1,0)==0,"Deferred source binding");
    wxCommandEvent event(wxEVT_MENU,32301); as<wxFrame>(frame)->HandleWindowEvent(event);
    expect(calls==0 && wxl_process_pending_events()==0 && calls==0,"Pending-only never delivers UI actions");
    expect(wxl_unbind(917)==1,"Unbind deferred source"); deferred_event_probe={};
    // Even after callback_depth clears, a still-running synchronous bridge
    // operation may force idle. Driver/entry depths postpone until it returns.
    wxTheApp->CallAfter([&] {
        in_origin=true;
        expect(status([&] { wxTheApp->ProcessIdle(); })==0,"Nested native operation idle");
        expect(calls==0,"No delivery before originating native operation returns"); in_origin=false;
    });
    for (int attempt=0; calls<3 && attempt<20; ++attempt) expect(wxl_poll()==0,"Drain deferred actions");
    expect(calls==3 && modal_seen && order==std::vector<uint64_t>({2001,2002,2003}),"FIFO/nonrecursive delivery");
    expect(wxl_defer_ui_action(0,0)==-1 && wxl_defer_ui_action(2004,frame)==0 && wxl_defer_ui_action(2004,frame)==-1 && wxl_cancel_ui_action(2004)==1 && wxl_cancel_ui_action(2004)==0,"Token/cancel validation");
    const auto dead=wxl_panel(frame);
    expect(wxl_defer_ui_action(2004,dead)==0,"Queue pending-owner test");
    expect(wxl_destroy(dead)==0 && wxl_ui_action_pending(2004)==0,"Pending/destroyed owner pruning");
    const auto expired=wxl_panel(frame);
    expect(wxl_defer_ui_action(2005,expired)==0,"Queue owner-expiry callback"); delete window(expired);
    for (int attempt=0; !dropped && attempt<20; ++attempt) expect(wxl_poll()==0,"Drop expired owner");
    expect(dropped,"Dead owner releases root without executing closure");
    expect(wxl_defer_ui_action(2006,0)==0 && wxl_defer_ui_action(2007,0)==0,"Queue error/following work");
    for (int attempt=0; calls<4 && attempt<20; ++attempt) expect(wxl_poll()==0,"Deliver pending error");
    expect(calls==4 && wxl_ui_action_pending(2007)==1,"Pending callback error pauses later work");
    expect(wxl_callback_error_pending(0)==0,"Recover callback error");
    for (int attempt=0; calls<5 && attempt<20; ++attempt) expect(wxl_poll()==0,"Resume queue");
    expect(calls==5,"Recovered queue resumes once");
    for (uint64_t token=3000; token<4024; ++token) expect(wxl_defer_ui_action(token,0)==0,"Queue capacity fill");
    expect(wxl_defer_ui_action(4024,0)==-1,"Queue capacity bound");
    for (uint64_t token=3000; token<4024; ++token) expect(wxl_cancel_ui_action(token)==1,"Queue capacity cancel");
    delete window(frame); expect(ui_actions.empty(),"Deferred tests leave queue empty");
    ui_action_probe={};
}

void exercise_accelerators() {
    const auto frame=wxl_frame("Accelerators",640,360), child=wxl_panel(frame);
    const int keys[]={'A',1012,'0'}, flags[]={1,0,4}, ids[]={3201,32767,3202};
    expect(wxl_frame_accelerator_count(frame)==0,"Default empty accelerator table");
    expect(wxl_set_frame_accelerators(frame,keys,flags,ids,3)==0 && wxl_frame_accelerator_count(frame)==3,"Install copied table");
    int key=0, mods=0, command=0;
    expect(wxl_frame_accelerator_entry(frame,0,&key,&mods,&command)==0 && key=='A' && mods==1 && command==3201,"Accelerator metadata");
    expect(wxl_frame_accelerator_entry(frame,-1,&key,&mods,&command)==-1 && wxl_frame_accelerator_entry(frame,3,&key,&mods,&command)==-1 && wxl_frame_accelerator_entry(frame,0,nullptr,&mods,&command)==-1,"Index/output validation");
    expect(wxl_set_frame_accelerators(frame,nullptr,flags,ids,1)==-1 && wxl_set_frame_accelerators(frame,keys,flags,ids,257)==-1,"Array/count validation");
    const int invalidkeys[]={1000}, invalidids[]={32768}, invalidflags[]={16}, nomods[]={2};
    expect(wxl_set_frame_accelerators(frame,invalidkeys,flags,ids,1)==-1 && wxl_set_frame_accelerators(frame,keys,flags,invalidids,1)==-1 && wxl_set_frame_accelerators(frame,keys,invalidflags,ids,1)==-1 && wxl_set_frame_accelerators(frame,keys,nomods,ids,1)==-1,"Raw entry validation");
    const int duplicatekeys[]={'A','A'}, duplicateflags[]={1,1}, duplicateids[]={3201,3202}, rawflags[]={1,8};
    expect(wxl_set_frame_accelerators(frame,duplicatekeys,duplicateflags,duplicateids,2)==-1 && wxl_frame_accelerator_count(frame)==3,"Duplicate atomic rejection");
#ifdef __WXMAC__
    expect(wxl_set_frame_accelerators(frame,duplicatekeys,rawflags,duplicateids,2)==0,"Distinct OSX control modifiers");
#else
    expect(wxl_set_frame_accelerators(frame,duplicatekeys,rawflags,duplicateids,2)==-1,"Aliased effective control modifiers");
#endif
    expect(wxl_set_frame_accelerators(frame,keys,flags,ids,3)==0,"Replace table");
    const auto bar=wxl_menu_bar(frame), menu=wxl_menu(bar,"Actions",7), item=wxl_menu_item(menu,3201,"Check",5,nullptr,0,1);
    expect(item && wxl_set_menu_enabled(item,0)>=0,"Disabled matching check menu");
    const auto dialog=wxl_message_dialog(frame,"Probe",5,"Probe",5,0,0);
    as<wxDialog>(dialog)->Bind(wxEVT_SHOW, [](wxShowEvent& event) { if (event.IsShown()) wxl_dialog_abort(); event.Skip(); });
    unsigned calls=0; bool installed=true;
    accelerator_probe=[&](uint64_t token,const wxl_event* event) {
        ++calls; expect(event->kind==WXL_EVENT_MENU,"Key routes MENU command");
        if (installed) {
            expect(wxl_destroy(frame)==-1 && wxl_close(frame,1)==-1 && wxl_destroy(child)==-1,"Accelerator frame subtree guards");
            expect(wxl_set_frame_accelerators(frame,nullptr,nullptr,nullptr,0)==-1,"No callback table replacement");
            int result=-1;
            expect(wxl_dialog_show(dialog,&result)==-1 && wxl_clipboard_text_available()==-1 && wxl_poll()==-1,"Accelerator loop guards");
        } else if (token==915) {
            int result=-1;
            expect(wxl_clipboard_text_available()>=0 && wxl_dialog_show(dialog,&result)==0 && result==1,"Clear restores menu modal/clipboard policy");
        }
        if (token==916) expect(wxl_context_set_skipped(event->context,1)==0,"Stack action propagation");
    };
    const auto handler=wxl_event_handler();
    expect(wxl_push_handler(frame,handler)==0 && wxl_bind(handler,WXL_EVENT_MENU,916,1,32767,1,0)==0 && wxl_bind(frame,WXL_EVENT_MENU,915,1,32767,1,0)==0,"Frame/stack accelerator handlers");
#ifdef __WXOSX__
    wxKeyEvent event(wxEVT_CHAR_HOOK); event.m_keyCode='A'; event.SetControlDown(true);
    expect(as<wxFrame>(frame)->GetAcceleratorTable()->GetCommand(event)==3201,"Native accelerator lookup");
    expect(window(child)->OSXHandleKeyEvent(event),"Actual child-to-frame native key dispatch");
    expect(calls==2 && wxl_menu_checked(item)==0,"OSX direct command ignores disabled/check menu state");
    event.m_keyCode=WXK_F12; event.SetControlDown(false);
    expect(window(child)->OSXHandleKeyEvent(event) && calls==4,"Unmodified function key dispatch");
    event.m_keyCode='0'; event.SetAltDown(true);
    expect(window(child)->OSXHandleKeyEvent(event) && calls==6,"Alt digit dispatch");
#else
    wxCommandEvent event(wxEVT_MENU,3201); as<wxFrame>(frame)->HandleWindowEvent(event);
    expect(calls==2,"Frame command/stack guard probe (physical translation unrun)");
#endif
    expect(wxl_set_frame_accelerators(frame,nullptr,nullptr,nullptr,0)==0 && wxl_frame_accelerator_count(frame)==0 && !as<wxFrame>(frame)->GetAcceleratorTable()->IsOk(),"Clear native table");
    installed=false;
    wxCommandEvent after(wxEVT_MENU,3201); as<wxFrame>(frame)->HandleWindowEvent(after);
    expect(accelerator_action_frames.empty(),"Callback guard stack restored");
    expect(wxl_unbind(915)==1 && wxl_unbind(916)==1,"Unbind accelerator probes"); accelerator_probe={};
    wxl_handle popped=0; expect(wxl_pop_handler(frame,1,&popped)==0,"Pop accelerator stack");
    expect(wxl_set_frame_accelerators(frame,keys,flags,ids,3)==0,"Reinstall before teardown");
    delete as<wxFrame>(frame);
    expect(frame_accelerators.empty() && wxl_frame_accelerator_count(frame)==-1,"Owner teardown clears accelerator registry");
}

void exercise_frame_actions() {
    const auto frame = wxl_frame("Frame actions",640,360), bar = wxl_toolbar(frame), status_bar = wxl_status_bar(frame,2);
    expect(bar && status_bar, "Frame bar construction");
    expect(!wxl_toolbar(frame) && !wxl_status_bar(frame,1), "Duplicate bars rejected");
    expect(wxl_add_tool(bar,1001,"Check",5,6,1,"Help",4)==0 && wxl_add_tool(bar,1002,"Open",4,1,0,nullptr,0)==0, "Tool add");
    expect(wxl_add_tool(bar,1001,"Dup",3,0,0,nullptr,0)==-1 && wxl_add_tool(bar,0,nullptr,0,-1,2,nullptr,0)==0, "Tool IDs/separators");
    expect(wxl_add_tool(bar,1003,"Bad",3,7,0,nullptr,0)==-1 && wxl_add_tool(bar,1003,"Bad",3,0,3,nullptr,0)==-1, "Invalid tool options");
    expect(wxl_set_tool_state(bar,1001,1,2)==-1 && wxl_set_tool_state(bar,1002,1,1)==-1 && wxl_tool_state(bar,999,0)==-1, "Raw state validation");
    expect(wxl_set_status_text(status_bar,2,"Bad",3)==-1 && wxl_status_text(status_bar,-1,nullptr)==nullptr, "Field validation");
    expect(wxl_realize_toolbar(bar)==0 && wxl_realize_toolbar(bar)==-1 && wxl_tool_count(bar)==3, "Realize once/count");
    expect(as<wxFrame>(frame)->GetStatusBarPane()==-1, "App-owned status text");
    const auto dialog = wxl_message_dialog(frame,"Probe",5,"Probe",5,0,0);
    unsigned calls=0;
    toolbar_probe = [&](uint64_t token, const wxl_event* event) {
        ++calls;
        expect(event->kind==WXL_EVENT_MENU && event->origin_id==bar && event->id==1001 && event->checked==1 && event->integer==1 && event->extra_long==1, "Toolbar native payload");
        expect(wxl_destroy(bar)==-1 && wxl_destroy(frame)==-1 && wxl_close(frame,1)==-1, "Native tool ancestor guard");
        expect(wxl_set_tool_state(bar,1001,1,0)==-1 && wxl_set_tool_state(bar,1002,0,0)==-1 && wxl_activate_tool(bar,1002)==-1, "Same toolbar guard");
        int result=-1;
        expect(wxl_dialog_show(dialog,&result)==-1 && wxl_clipboard_text_available()==-1 && wxl_poll()==-1, "Native tool loop guard");
        expect(wxl_set_status_text(status_bar,1,"Action",6)==0, "Status write during action");
        if (token==913) expect(wxl_context_set_skipped(event->context,1)==0,"Propagate tool action");
    };
    expect(wxl_bind(bar,WXL_EVENT_MENU,913,1001,1001,1,0)==0 && wxl_bind(frame,WXL_EVENT_MENU,914,1001,1001,1,0)==0, "Native tool bindings");
    auto* native = as<wxToolBar>(bar);
    // Enter the actual native click path without the bridge activation guard.
    native->ToggleTool(1001,true); native->OnLeftClick(1001,true);
    expect(calls==2 && toolbar_operations.empty(),"Actual/propagated toolbar callbacks");
    expect(wxl_unbind(913)==1 && wxl_unbind(914)==1,"Unbind toolbar probes"); toolbar_probe={};
    expect(wxl_activate_tool(bar,1001)==1 && wxl_tool_state(bar,1001,1)==0,"Accepted activation independent of handled result");
    expect(wxl_set_tool_state(bar,1002,0,0)==0 && wxl_activate_tool(bar,1002)==0,"Disabled tool no dispatch");
    expect(wxl_destroy(bar)==0 && wxl_destroy(status_bar)==0 && wxl_poll()==0,"Quiescent bar destruction");
    expect(as<wxFrame>(frame)->GetToolBar()==nullptr && as<wxFrame>(frame)->GetStatusBar()==nullptr,"Detached frame slots");
    wxl_handle replacement_status=0;
    for (const auto& option : {"", "0", "1"}) {
        const auto previous=wxSystemOptions::GetOption("mac.toolbar.no-native");
        wxSystemOptions::SetOption("mac.toolbar.no-native",option);
        for (int repetition=0; repetition<8; ++repetition) {
            const auto replacement=wxl_toolbar(frame);
            expect(replacement && wxSystemOptions::GetOption("mac.toolbar.no-native")==option,"Backend option effective value restored");
#if defined(__WXOSX__) && wxOSX_USE_NATIVE_TOOLBAR
            expect(!as<wxToolBar>(replacement)->MacWantsNativeToolbar(),"Explicit safe in-window toolbar backend");
#endif
            expect(wxl_add_tool(replacement,1,"Unrealized",10,0,0,nullptr,0)==0,"Replacement tool");
            if (repetition%2) expect(wxl_realize_toolbar(replacement)==0,"Replacement realization");
            expect(wxl_destroy(replacement)==0,"Synchronous replacement deletion");
        }
        wxSystemOptions::SetOption("mac.toolbar.no-native",previous);
    }
    expect(wxl_toolbar(frame)!=0,"Replace unrealized toolbar");
    replacement_status=wxl_status_bar(frame,1);
    delete as<wxFrame>(frame);
    expect(wxl_handler_alive(bar)==0 && wxl_handler_alive(status_bar)==0 && wxl_handler_alive(replacement_status)==0 && realized_toolbars.empty(),"Frame-owned bar invalidation");
}

void exercise_splitter() {
    const auto frame = wxl_frame("Splitter", 640, 480);
    const auto regular = wxl_splitter(frame, 0, 0, 20, 0.5);
    expect(regular && !wxl_splitter(frame, 2, 0, 20, 0.5) && !wxl_splitter(frame, 0, -1, 20, 0.5) &&
           !wxl_splitter(frame, 0, 0, 0, 0.5) && !wxl_splitter(frame, 0, 0, 20, std::numeric_limits<double>::quiet_NaN()),
           "Splitter constructor flags/pixels/gravity validation failed");
    const auto first = wxl_panel(regular), second = wxl_panel(regular), wrong = wxl_panel(frame);
    window(regular)->SetSize(600, 400);
    expect(wxl_splitter_attach(regular, first, first) == -1 && wxl_splitter_attach(regular, first, wrong) == -1 &&
           wxl_splitter_attach(regular, first, second) == 0, "Splitter pane ownership checks failed");
    int position = -1; wxl_handle pane = 0;
    expect(wxl_splitter_int(regular, 0, &position) == 0 && std::abs(position - window(regular)->GetClientSize().x / 2) <= 2,
           "Pre-attach zero request lost centering after resize");
    expect(wxl_splitter_pane(regular, 0, &pane) == 0 && pane == first && wxl_splitter_pane(regular, 1, &pane) == 0 && pane == second &&
           wxl_destroy(first) == -1 && wxl_column(regular) == -1 && wxl_splitter_attach(regular, first, second) == -1,
           "Attached pane identity/lifetime guards failed");
    expect(wxl_set_splitter_int(regular, 1, 0) == -1 && wxl_set_splitter_int(regular, 0, INT_MAX) == -1 &&
           wxl_set_splitter_int(regular, 3, 1) == -1 && wxl_splitter_int(regular, 4, &position) == -1 &&
           wxl_splitter_int(regular, 0, nullptr) == -1 && wxl_splitter_pane(regular, 2, &pane) == -1 &&
           wxl_splitter_pane(regular, 0, nullptr) == -1 && wxl_splitter_gravity(regular, nullptr) == -1 &&
           wxl_set_splitter_gravity(regular, 2) == -1 && wxl_set_splitter_gravity(regular, std::numeric_limits<double>::infinity()) == -1,
           "Splitter raw field/output checks failed");
    auto* probe = new ManagedWindow<SplitterProbe>(window(frame), wxID_ANY, wxDefaultPosition, wxSize(600,400), wxSP_3D | wxSP_LIVE_UPDATE);
    probe->SetMinimumPaneSize(20); probe->SetSplitMode(wxSPLIT_VERTICAL);
    const auto splitter = track(probe); splitter_owners.insert(probe);
    const auto left = wxl_panel(splitter), right = wxl_panel(splitter);
    expect(wxl_splitter_attach(splitter, left, right) == 0, "Native splitter protocol probe setup failed");
    const auto dialog = wxl_message_dialog(frame, "Guard", 5, "Guard", 5, 0, 0);
    expect(dialog, "Splitter guard dialog setup failed");
    unsigned calls = 0; int action = 0; int frozen = 0;
    splitter_probe = [&](uint64_t token, const wxl_event* event) {
        ++calls; int live = -2, code = -1;
        expect(event->family == WXL_FAMILY_NOTIFY && event->origin_id == splitter && wxl_event_sash(event->context, &live) == 0 && live == event->sash_position,
               "Splitter typed snapshot/live metadata mismatch");
        expect(wxl_destroy(splitter) == -1 && wxl_destroy(frame) == -1 && wxl_destroy(left) == -1 && wxl_destroy(right) == -1 &&
               wxl_close(frame, 1) == -1 && wxl_set_splitter_int(splitter, 0, 90) == -1 && wxl_set_splitter_gravity(splitter, 0.2) == -1 &&
               wxl_splitter_attach(splitter, left, right) == -1 && wxl_dialog_show(dialog, &code) == -1 &&
               wxl_clipboard_text_available() == -1 && !wxl_clone_event(event->context, 1), "Actual sash callback bypassed lifetime/mutation/native-loop guards");
        if (event->kind == WXL_EVENT_SPLITTER_CHANGING) {
            frozen = event->sash_position;
            expect(wxl_set_event_sash(event->context, token == 911 ? 150 : action == 2 ? -1 : 175) == 0 && event->sash_position == frozen,
                   "Sash proposal editing changed immutable snapshot");
            expect(wxl_set_event_sash(event->context, -2) == -1 && wxl_set_event_sash(event->context, INT_MAX) == -1,
                   "Invalid proposed sash should reject");
            if (token == 912 && action == 1) expect(wxl_context_set_vetoed(event->context, 1) == 0, "Sash Veto failed");
        } else {
            expect(wxl_set_event_sash(event->context, 175) == -1, "CHANGED position must be read-only");
            expect(wxl_context_set_vetoed(event->context, 1) == 0, "CHANGED veto metadata unavailable");
        }
        // Origin callback propagates; parent consumes so native proposal changes take effect.
        if (token == 911) expect(wxl_context_set_skipped(event->context, 1) == 0, "Sash propagation failed");
    };
    for (const int kind : {WXL_EVENT_SPLITTER_CHANGING, WXL_EVENT_SPLITTER_CHANGED}) {
        expect(wxl_bind(splitter, kind, 911, wxID_ANY, wxID_ANY, 1, 0) == 0 && wxl_bind(frame, kind, 912, wxID_ANY, wxID_ANY, 1, 0) == 0,
               "Sash event binding failed");
        if (kind == WXL_EVENT_SPLITTER_CHANGING) {
            expect(probe->OnSashPositionChanging(100) == 175, "Native CHANGING ignored consumed adjusted proposal");
            action = 1; expect(probe->OnSashPositionChanging(100) == -1, "Native CHANGING veto did not cancel");
            action = 2; expect(probe->OnSashPositionChanging(100) == -1, "Native CHANGING -1 did not cancel");
        } else {
            probe->SetSashPositionAndNotify(220);
            expect(probe->GetSashPosition() == 220, "CHANGED veto incorrectly undid committed position");
        }
        expect(wxl_unbind(911) == 1 && wxl_unbind(912) == 1 && splitter_operations.empty(), "Sash event guard did not unwind");
        expect(!wxl_owned_event(kind, 1, "", 0, 0, 0, 0, 0), "Derived sash event construction must reject");
    }
    expect(calls == 8, "Actual/propagated sash paths incomplete"); splitter_probe = nullptr;
    probe->SetMinimumPaneSize(0); // Private native bypass proves override independently of public positive minimum.
    probe->OnDoubleClickSash(100, 10);
    expect(probe->IsSplit() && probe->GetWindow1() == window(left) && probe->GetWindow2() == window(right), "Double-click detached fixed panes");
    probe->SetMinimumPaneSize(20);
    delete window(frame);
    expect(!splitter_owners.size() && wxl_handler_alive(left) == 0 && wxl_handler_alive(right) == 0 &&
           wxl_splitter_pane(regular, 0, &pane) == -1, "Splitter teardown left stale pane state");
}

class FocusProbe : public wxPanel {
public:
    using wxPanel::wxPanel;
    std::function<void()> during_focus;
    unsigned keyboard_calls=0;
    void SetFocus() override { if (during_focus) during_focus(); }
    void SetFocusFromKbd() override { ++keyboard_calls; SetFocus(); }
};
void exercise_focus_navigation() {
    const auto frame=wxl_frame("Focus",400,240);
    auto* probe=new ManagedWindow<FocusProbe>(window(frame),wxID_ANY);
    const auto target=track(probe);
    const auto sibling=wxl_button(frame,"Sibling");
    expect(wxl_focus_window(target,0)==-1 && wxl_navigate_focus(target,1)==-1,"Hidden focus rejects");
    expect(wxl_show(frame)==0,"Show focus frame");
    unsigned calls=0;
    probe->during_focus=[&] {
        ++calls;
        expect(wxl_destroy(target)==-1 && wxl_destroy(sibling)==-1 && wxl_destroy(frame)==-1 &&
               wxl_close(frame,1)==-1,"Protect focus graph during native operation");
        expect(wxl_focus_window(sibling,0)==-1 && wxl_navigate_focus(sibling,1)==-1,
               "Nested focus rejects even outside Lisp callbacks");
        expect(wxl_poll()==-1 && wxl_run()==-1 && wxl_finish()==-1 && wxl_clipboard_text_available()==-1,
               "Focus operation native-entry protection");
    };
    expect(wxl_focus_window(target,0)==0 && calls==1,"Direct native focus dispatch");
    expect(wxl_focus_window(target,1)==0 && calls==2 && probe->keyboard_calls==1,"Keyboard-origin native focus dispatch");
    expect(input_operations.empty() && focus_operation_depth==0,"Focus guards unwind");
    expect(wxl_focus_window(target,2)==-1 && wxl_navigate_focus(target,-1)==-1,"Raw focus flags reject");
    probe->Enable(false);
    expect(wxl_focus_window(target,0)==-1 && input_operations.empty(),"Disabled focus and guard rollback");
    probe->Enable(true);
    int navigation_calls=0;
    window(frame)->Bind(wxEVT_NAVIGATION_KEY,[&](wxNavigationKeyEvent& event) {
        ++navigation_calls;
        expect(event.GetDirection()==(navigation_calls==1),"Native traversal direction");
        expect(wxl_destroy(sibling)==-1 && wxl_navigate_focus(target,1)==-1,"Traversal graph/reentry protection");
        // Consuming the event makes the native result deterministic without OS focus.
    });
    expect(wxl_navigate_focus(target,1)==1 && wxl_navigate_focus(target,0)==1 && navigation_calls==2,"Native forward/backward traversal");
    delete window(frame);
    expect(wxl_focus_window(target,0)==-1 && wxl_navigate_focus(target,1)==-1,"Stale focus rejects");
}

class TooltipProbe : public wxPanel {
public:
    using wxPanel::wxPanel;
    std::function<void()> during_tooltip;
    void DoSetToolTipText(const wxString& tip) override {
        if (during_tooltip) during_tooltip();
        wxPanel::DoSetToolTipText(tip);
    }
    void DoSetToolTip(wxToolTip* tip) override {
        if (during_tooltip) during_tooltip();
        wxPanel::DoSetToolTip(tip);
    }
};
class CountedToolTip : public wxToolTip {
public:
    explicit CountedToolTip(unsigned& count) : wxToolTip("Owned"), destroyed(count) {}
    ~CountedToolTip() override { ++destroyed; }
    unsigned& destroyed;
};
void exercise_tooltips() {
    const auto frame=wxl_frame("Tooltips",400,240);
    auto* probe=new ManagedWindow<TooltipProbe>(window(frame),wxID_ANY);
    const auto target=track(probe);
    size_t length=99; int present=-1;
    const auto initial=wxl_tooltip_text(target,&length,&present);
    expect(initial && length==0 && present==0,"Absent native tooltip");
    expect(wxl_set_tooltip_text(target,"Tip \xce\xbb",6,1)==0,"Unicode tooltip setter");
    const auto text=wxl_tooltip_text(target,&length,&present);
    expect(text && present==1 && length==6 && std::string(text,length)=="Tip \xce\xbb","Copied native tooltip read");
    expect(wxl_set_tooltip_text(target,"",0,1)==0 && probe->GetToolTip() &&
           wxl_tooltip_text(target,&length,&present) && present==1 && length==0,"Empty retains attached tooltip");
    expect(wxl_set_tooltip_text(target,nullptr,0,0)==0 && !probe->GetToolTip(),"Explicit native removal");
    expect(wxl_set_tooltip_text(target,"tip",3,2)==-1 && wxl_set_tooltip_text(target,"tip",3,0)==-1 &&
           wxl_set_tooltip_text(target,nullptr,1,1)==-1 && wxl_set_tooltip_text(target,"a\0b",3,1)==-1 &&
           wxl_set_tooltip_text(target,"\xff",1,1)==-1 && wxl_set_tooltip_text(target,"",4097,1)==-1 &&
           !wxl_tooltip_text(target,nullptr,&present) && !wxl_tooltip_text(target,&length,nullptr),"Raw tooltip validation");
    const std::string limit(4096,'x');
    expect(wxl_set_tooltip_text(target,limit.data(),limit.size(),1)==0,"Tooltip byte boundary");
    const auto dialog=wxl_message_dialog(frame,"Tooltip",7,"Probe",5,0,0);
    unsigned native_calls=0, callback_calls=0;
    auto check_guards=[&] {
        expect(wxl_destroy(target)==-1 && wxl_destroy(frame)==-1 && wxl_close(frame,1)==-1,"Tooltip receiver/ancestor guard");
        expect(wxl_set_tooltip_text(target,"nested",6,1)==-1 && wxl_set_tooltip_text(target,nullptr,0,0)==-1,"Tooltip recursive mutation guard");
        int result=-1;
        expect(wxl_poll()==-1 && wxl_run()==-1 && wxl_finish()==-1 && wxl_clipboard_text_available()==-1 &&
               wxl_dialog_show(dialog,&result)==-1,"Tooltip native-entry guards");
    };
    tooltip_probe=[&](uint64_t,const wxl_event*) { ++callback_calls; check_guards(); };
    expect(wxl_bind(frame,WXL_EVENT_CLICK,929,wxID_ANY,wxID_ANY,1,0)==0,"Tooltip propagated probe binding");
    probe->during_tooltip=[&] {
        ++native_calls; check_guards();
        wxCommandEvent event(wxEVT_BUTTON,probe->GetId()); event.SetEventObject(probe); probe->HandleWindowEvent(event);
    };
    expect(wxl_set_tooltip_text(target,"updated",7,1)==0 && wxl_set_tooltip_text(target,nullptr,0,0)==0 &&
           native_calls>=2 && callback_calls==native_calls && input_operations.empty(),"Protected synchronous tooltip update/removal");
    probe->during_tooltip={}; tooltip_probe={}; expect(wxl_unbind(929)==1,"Tooltip probe unbind");
    unsigned destroyed=0;
    probe->SetToolTip(new CountedToolTip(destroyed));
    expect(wxl_set_tooltip_text(target,nullptr,0,0)==0 && destroyed==1,"Removal deletes native tooltip exactly once");
    probe->SetToolTip(new CountedToolTip(destroyed));
    delete window(frame);
    expect(destroyed==2 && !wxl_tooltip_text(target,&length,&present) &&
           wxl_set_tooltip_text(target,"stale",5,1)==-1,"Window teardown owns tooltip and invalidates handles");
}

class EarlyDateDestroyProbe : public wxDatePickerCtrl {
public:
    using wxDatePickerCtrl::wxDatePickerCtrl;
    std::function<void()> during_destroy;
    bool refuse=false;
    bool Destroy() override {
        if (during_destroy) during_destroy();
        return refuse ? false : wxDatePickerCtrl::Destroy();
    }
};
void exercise_date_picker() {
    const auto frame=wxl_frame("Dates",460,240), panel=wxl_panel(frame);
    unsigned calls=0; bool constructing=true;
    date_probe=[&](uint64_t,const wxl_event*) { ++calls; expect(!constructing,"Date constructor callbacks suppressed"); };
    expect(wxl_bind(frame,WXL_EVENT_DATE_CHANGE,930,wxID_ANY,wxID_ANY,1,0)==0 &&
           wxl_bind(frame,WXL_EVENT_TEXT_CHANGE,932,wxID_ANY,wxID_ANY,1,0)==0,"Observe date creation");
    const auto picker=wxl_date_picker(panel,2000,2,29), other=wxl_date_picker(panel,2024,1,1);
    expect(picker && other && !calls,"Date construction silence"); constructing=false;
    expect(wxl_unbind(930)==1 && wxl_unbind(932)==1,"Date construction probe unbind");
    int code=0;
    expect(wxl_calendar_date(picker,&code)==0 && code==20000229,"Initial native date");
    wxDateTime lower,upper;
    expect(as<wxDatePickerCtrl>(picker)->GetRange(&lower,&upper) && calendar_code(lower)==19000101 && calendar_code(upper)==21001231,"Inclusive fixed native date range");
    expect(!wxl_date_picker(panel,1900,2,29) && !wxl_date_picker(panel,2100,2,29) &&
           !wxl_date_picker(panel,2000,2,30) && !wxl_date_picker(panel,1899,1,1) &&
           !wxl_date_picker(panel,2000,0,1) && wxl_set_calendar_date(picker,2101,1,1,0)==-1 &&
           wxl_set_calendar_date(picker,2000,2,29,2)==-1 && wxl_calendar_date(frame,&code)==-1 &&
           wxl_calendar_date(picker,nullptr)==-1,"Raw Gregorian/type/output validation");
    expect(wxl_set_calendar_date(picker,1900,1,1,0)==0 && wxl_calendar_date(picker,&code)==0 && code==19000101 &&
           wxl_set_calendar_date(picker,2100,12,31,0)==0 && wxl_calendar_date(picker,&code)==0 && code==21001231,"Native date boundaries");
    const auto dialog=wxl_message_dialog(frame,"Date",4,"Probe",5,0,0);
    uint64_t expired=0;
    int expected=20240229;
    date_probe=[&](uint64_t token,const wxl_event* event) {
        ++calls; expired=event->context;
        expect(event->origin_id==picker && event->family==WXL_FAMILY_COMMAND,"Date native origin/family");
        expect(event->date_code==expected && event->integer==0 && !event->can_veto && !event->text,"Independent date payload");
        expect(wxl_set_calendar_date(picker,2024,1,1,0)==-1 && wxl_destroy(picker)==-1 &&
               wxl_destroy(panel)==-1 && wxl_close(frame,1)==-1,"Actual date source/ancestor guard");
        int result=-1;
        expect(wxl_poll()==-1 && wxl_clipboard_text_available()==-1 && wxl_dialog_show(dialog,&result)==-1,"Date native-entry protection");
        expect(wxl_set_calendar_date(other,2025,1,1,0)==0,"Silent cross-date synchronization");
        int64_t captured=-1;
        expect(wxl_event_field(event->context,1,19,&captured)==0 && captured==expected &&
               wxl_set_event_field(event->context,1,19,20240101)==-1 && !wxl_clone_event(event->context,1),"Scoped read-only date/no detached clone");
        if (token==930) expect(wxl_context_set_skipped(event->context,1)==0,"Date propagation");
    };
    expect(wxl_bind(picker,WXL_EVENT_DATE_CHANGE,930,wxID_ANY,wxID_ANY,1,0)==0 &&
           wxl_bind(frame,WXL_EVENT_DATE_CHANGE,931,wxID_ANY,wxID_ANY,1,0)==0,"Date native bindings");
    // Actual native date event outside bridge setters: payload may differ from current control.
    wxDateEvent native(window(picker),calendar_value(2024,2,29),wxEVT_DATE_CHANGED);
    window(picker)->HandleWindowEvent(native);
    expect(calls==2 && input_operations.empty(),"Actual date callback and propagation guards");
    int64_t captured=-1;
    expect(wxl_event_field(expired,1,19,&captured)==-1,"Date context expires");
    expect(wxl_set_calendar_date(picker,2024,2,29,0)==0 && calls==2,"Silent native date write");
    expect(wxl_set_calendar_date(picker,2024,2,29,1)==0 && calls==4,"Single explicit date event even unchanged");
    expected=0;
    wxDateEvent invalid(window(picker),wxInvalidDateTime,wxEVT_DATE_CHANGED);
    window(picker)->HandleWindowEvent(invalid);
    wxDateEvent outside(window(picker),wxDateTime(1,wxDateTime::Jan,1800),wxEVT_DATE_CHANGED);
    window(picker)->HandleWindowEvent(outside);
    expect(calls==8,"Invalid/out-of-scope native event dates produce absent date metadata");
    expect(!wxl_owned_event(WXL_EVENT_DATE_CHANGE,1,nullptr,0,0,0,0,0),"Owned date events reject");
    expect(wxl_unbind(930)==1 && wxl_unbind(931)==1,"Date binding cleanup");
    // Child-origin native TEXT callbacks must protect their owning composite date picker.
    const auto child=wxl_text(picker,"child",5,0,1);
    date_probe=[&](uint64_t,const wxl_event* event) {
        ++calls;
        expect(wxl_set_calendar_date(picker,2024,1,1,0)==-1 && wxl_destroy(picker)==-1,"Date composite-child input guard");
        int64_t date=-1; expect(wxl_event_field(expired,1,19,&date)==-1,"Old date context remains expired");
        expect(wxl_event_field(event->context,1,19,&date)==-1,"Non-date context rejects date metadata");
    };
    expect(wxl_bind(frame,WXL_EVENT_TEXT_CHANGE,932,wxID_ANY,wxID_ANY,1,0)==0,"Date composite TEXT bind");
    wxCommandEvent text(wxEVT_TEXT,window(child)->GetId()); text.SetEventObject(window(child)); text.SetString("child"); window(child)->HandleWindowEvent(text);
    expect(calls==9 && wxl_unbind(932)==1,"Date child-origin text guard");
    // Equivalent early native generic Destroy phase, injected before its base.
    auto* teardown=new ManagedWindow<EarlyDateDestroyProbe>(window(panel),wxID_ANY,calendar_value(2024,1,1));
    const auto teardown_id=track(teardown); input_owners.emplace(teardown,false);
    unsigned teardown_callbacks=0;
    date_probe=[&](uint64_t,const wxl_event*) { ++teardown_callbacks; };
    expect(wxl_bind(frame,WXL_EVENT_DATE_CHANGE,930,wxID_ANY,wxID_ANY,1,0)==0,"Early date teardown binding");
    teardown->during_destroy=[&] { wxDateEvent event(teardown,calendar_value(2024,1,1),wxEVT_DATE_CHANGED); teardown->HandleWindowEvent(event); };
    teardown->refuse=true;
    expect(!teardown->Destroy() && !input_owners.at(teardown) && !teardown_callbacks,"Failed Destroy restores date input guard");
    teardown->refuse=false;
    expect(wxl_destroy(teardown_id)==0 && !teardown_callbacks && wxl_unbind(930)==1,"Early composite Destroy suppresses date callbacks");
    date_probe={}; delete window(frame);
    expect(wxl_calendar_date(picker,&code)==-1 && wxl_set_calendar_date(picker,2024,1,1,0)==-1,"Date teardown stale handles");
}

class EarlyTimeDestroyProbe : public wxTimePickerCtrl {
public:
    using wxTimePickerCtrl::wxTimePickerCtrl;
    std::function<void()> during_destroy;
    bool refuse=false;
    bool Destroy() override {
        if (during_destroy) during_destroy();
        return refuse ? false : wxTimePickerCtrl::Destroy();
    }
};
void exercise_time_picker() {
    const auto frame=wxl_frame("Times",460,240), panel=wxl_panel(frame);
    unsigned calls=0; bool constructing=true;
    time_probe=[&](uint64_t,const wxl_event*) { ++calls; expect(!constructing,"Time constructor callbacks suppressed"); };
    expect(wxl_bind(frame,WXL_EVENT_TIME_CHANGE,933,wxID_ANY,wxID_ANY,1,0)==0 &&
           wxl_bind(frame,WXL_EVENT_TEXT_CHANGE,935,wxID_ANY,wxID_ANY,1,0)==0,"Observe time creation");
    const auto picker=wxl_time_picker(panel,0,0,0), other=wxl_time_picker(panel,12,34,56);
    expect(picker && other && !calls,"Time construction silence"); constructing=false;
    expect(wxl_unbind(933)==1 && wxl_unbind(935)==1,"Time construction probe unbind");
    int code=0;
    expect(wxl_clock_time(picker,&code)==0 && code==1,"Midnight is valid, not absent");
    const auto anchor=clock_value(2,30,45);
    expect(anchor.GetYear()==2012 && anchor.GetMonth()==wxDateTime::Jan && anchor.GetDay()==1 &&
           clock_code(anchor)==9046,"Fixed anchor preserves early-morning time without today's DST date");
    expect(!wxl_time_picker(panel,24,0,0) && !wxl_time_picker(panel,0,60,0) &&
           !wxl_time_picker(panel,0,0,60) && !wxl_time_picker(panel,-1,0,0) &&
           wxl_set_clock_time(picker,0,-1,0,0)==-1 && wxl_set_clock_time(picker,0,0,-1,0)==-1 &&
           wxl_set_clock_time(picker,0,0,0,2)==-1 && wxl_clock_time(frame,&code)==-1 &&
           wxl_clock_time(picker,nullptr)==-1,"Raw time/type/output validation");
    expect(wxl_set_clock_time(picker,23,59,59,0)==0 && wxl_clock_time(picker,&code)==0 && code==86400,"Last time-of-day second");
    const auto dialog=wxl_message_dialog(frame,"Time",4,"Probe",5,0,0);
    uint64_t expired=0;
    int expected=9046;
    time_probe=[&](uint64_t token,const wxl_event* event) {
        ++calls; expired=event->context;
        expect(event->origin_id==picker && event->family==WXL_FAMILY_COMMAND,"Time native origin/family");
        expect(event->time_code==expected && event->date_code==0 && event->integer==0 && !event->can_veto && !event->text,"Time payload separate from date/raw metadata");
        expect(wxl_set_clock_time(picker,1,2,3,0)==-1 && wxl_destroy(picker)==-1 &&
               wxl_destroy(panel)==-1 && wxl_close(frame,1)==-1,"Actual time source/ancestor guard");
        int result=-1;
        expect(wxl_poll()==-1 && wxl_clipboard_text_available()==-1 && wxl_dialog_show(dialog,&result)==-1,"Time native-entry protection");
        expect(wxl_set_clock_time(other,4,5,6,0)==0,"Silent cross-time synchronization");
        int64_t captured=-1;
        expect(wxl_event_field(event->context,1,20,&captured)==0 && captured==expected &&
               wxl_set_event_field(event->context,1,20,1)==-1 && !wxl_clone_event(event->context,1),"Scoped read-only time/no detached clone");
        expect(wxl_event_field(event->context,1,19,&captured)==-1,"Time context has no date payload");
        if (token==933) expect(wxl_context_set_skipped(event->context,1)==0,"Time propagation");
    };
    expect(wxl_bind(picker,WXL_EVENT_TIME_CHANGE,933,wxID_ANY,wxID_ANY,1,0)==0 &&
           wxl_bind(frame,WXL_EVENT_TIME_CHANGE,934,wxID_ANY,wxID_ANY,1,0)==0,"Time native bindings");
    // Payload date is deliberately outside the calendar picker range; only HMS matters.
    wxDateEvent native(window(picker),wxDateTime(1,wxDateTime::Jan,1800,2,30,45),wxEVT_TIME_CHANGED);
    window(picker)->HandleWindowEvent(native);
    expect(calls==2 && input_operations.empty(),"Actual time callback and propagation independent of setters");
    int64_t captured=-1;
    expect(wxl_event_field(expired,1,20,&captured)==-1,"Time context expires");
    expect(wxl_set_clock_time(picker,2,30,45,0)==0 && calls==2,"Silent time write");
    expect(wxl_set_clock_time(picker,2,30,45,1)==0 && calls==4,"Single explicit time event even unchanged");
    expected=1;
    expect(wxl_set_clock_time(picker,0,0,0,1)==0 && calls==6,"Midnight event is present");
    expected=0;
    wxDateEvent invalid(window(picker),wxInvalidDateTime,wxEVT_TIME_CHANGED); window(picker)->HandleWindowEvent(invalid);
    expect(calls==8,"Invalid native event time yields absent metadata");
    expect(!wxl_owned_event(WXL_EVENT_TIME_CHANGE,1,nullptr,0,0,0,0,0),"Owned time events reject");
    expect(wxl_unbind(933)==1 && wxl_unbind(934)==1,"Time binding cleanup");
    const auto child=wxl_text(picker,"child",5,0,1);
    time_probe=[&](uint64_t,const wxl_event* event) {
        ++calls;
        expect(wxl_set_clock_time(picker,1,2,3,0)==-1 && wxl_destroy(picker)==-1,"Time composite-child Input guard");
        int64_t time=-1;
        expect(wxl_event_field(event->context,1,20,&time)==-1 && event->time_code==0,"Non-time context/snapshot metadata");
    };
    expect(wxl_bind(frame,WXL_EVENT_TEXT_CHANGE,935,wxID_ANY,wxID_ANY,1,0)==0,"Time composite TEXT bind");
    wxCommandEvent text(wxEVT_TEXT,window(child)->GetId()); text.SetEventObject(window(child)); text.SetString("child"); window(child)->HandleWindowEvent(text);
    expect(calls==9 && wxl_unbind(935)==1,"Time child-origin text guard");
    auto* teardown=new ManagedWindow<EarlyTimeDestroyProbe>(window(panel),wxID_ANY,clock_value(0,0,0));
    const auto teardown_id=track(teardown); input_owners.emplace(teardown,false);
    unsigned teardown_callbacks=0;
    time_probe=[&](uint64_t,const wxl_event*) { ++teardown_callbacks; };
    expect(wxl_bind(frame,WXL_EVENT_TIME_CHANGE,933,wxID_ANY,wxID_ANY,1,0)==0,"Early time teardown binding");
    teardown->during_destroy=[&] { wxDateEvent event(teardown,clock_value(0,0,0),wxEVT_TIME_CHANGED); teardown->HandleWindowEvent(event); };
    teardown->refuse=true;
    expect(!teardown->Destroy() && !input_owners.at(teardown) && !teardown_callbacks,"Failed time Destroy restores Input guard");
    teardown->refuse=false;
    expect(wxl_destroy(teardown_id)==0 && !teardown_callbacks && wxl_unbind(933)==1,"Early time Destroy suppresses callbacks");
    time_probe={}; delete window(frame);
    expect(wxl_clock_time(picker,&code)==-1 && wxl_set_clock_time(picker,0,0,0,0)==-1,"Time teardown stale handles");
}

class NonEntrySearchProbe : public wxSearchCtrl {
public:
    using wxSearchCtrl::wxSearchCtrl;
    // Generic ports don't necessarily provide WXGetTextEntry().
    const wxTextEntry* WXGetTextEntry() const override { return nullptr; }
};
class EarlySearchDestroyProbe : public wxSearchCtrl {
public:
    using wxSearchCtrl::wxSearchCtrl;
    std::function<void()> during_destroy;
    bool refuse=false;
    bool Destroy() override {
        if (during_destroy) during_destroy();
        return refuse ? false : wxSearchCtrl::Destroy();
    }
};
void exercise_search_controls() {
    const auto frame=wxl_frame("Search",460,240), panel=wxl_panel(frame);
    unsigned calls=0; bool constructing=true;
    search_probe=[&](uint64_t,const wxl_event*) { ++calls; expect(!constructing,"Search constructor callbacks suppressed"); };
    expect(wxl_bind(frame,WXL_EVENT_TEXT_CHANGE,938,wxID_ANY,wxID_ANY,1,0)==0,"Observe search construction");
    const auto search=wxl_search_control(panel,"query λ",8,"Find λ",7), other=wxl_search_control(panel,"",0,"Search",6);
    expect(search && other && !calls,"Search construction silence"); constructing=false; expect(wxl_unbind(938)==1,"Search constructor unbind");
    size_t length=0;
    const auto value=wxl_search_field(search,0,&length); expect(value && std::string(value,length)=="query λ","Native search text");
    const auto hint=wxl_search_field(search,1,&length); expect(hint && std::string(hint,length)=="Find λ","Native search placeholder");
    expect(!wxl_search_control(panel,"a\nb",3,"",0) && !wxl_search_control(panel,"",0,"a\rb",3) &&
           wxl_set_search_field(search,0,"a\0b",3,0)==-1 && wxl_set_search_field(search,0,nullptr,1,0)==-1 &&
           wxl_set_search_field(search,0,"\xff",1,0)==-1 && wxl_set_search_field(search,2,"",0,0)==-1 &&
           wxl_set_search_field(search,1,"",0,1)==-1 && wxl_set_search_field(search,0,"",0,2)==-1 &&
           !wxl_search_field(frame,0,&length) && !wxl_search_field(search,2,&length) && !wxl_search_field(search,0,nullptr),"Raw search field/input validation");
    const auto dialog=wxl_message_dialog(frame,"Search",6,"Probe",5,0,0);
    uint64_t expired=0;
    std::string expected="payload λ";
    auto check_guards=[&] {
        expect(wxl_set_search_field(search,0,"nested",6,0)==-1 && wxl_set_search_field(search,1,"nested",6,0)==-1 &&
               wxl_destroy(search)==-1 && wxl_destroy(panel)==-1 && wxl_close(frame,1)==-1,"Actual search origin/ancestor guards");
        int result=-1;
        expect(wxl_poll()==-1 && wxl_clipboard_text_available()==-1 && wxl_dialog_show(dialog,&result)==-1,"Search native-entry guards");
        expect(wxl_set_search_field(other,0,"Sync",4,0)==0 && wxl_set_search_field(other,1,"Hint",4,0)==0,"Silent cross-search writes");
    };
    search_probe=[&](uint64_t token,const wxl_event* event) {
        ++calls; expired=event->context; check_guards();
        expect(event->origin_id==search && event->family==WXL_FAMILY_COMMAND && !event->can_veto,"Search native origin/family");
        expect(event->text && std::string(event->text,event->text_length)==expected,"Native search/cancel payload copied without inference");
        size_t size=0;
        if (event->kind==WXL_EVENT_TEXT_CHANGE) {
            expect(wxl_set_event_text(event->context,1,"metadata",8)==-1,"Lazy search-origin TEXT metadata rejects writes");
            const auto clone=wxl_clone_event(event->context,1);
            expect(clone!=0,"Existing generic TEXT clone support remains");
            const auto text=wxl_event_text(clone,0,&size); expect(text && std::string(text,size)==expected,"Detached TEXT freezes native search string");
            expect(wxl_event_destroy(clone)==0,"TEXT clone disposal");
        } else {
            expect(!wxl_clone_event(event->context,1),"Native search/cancel clone rejects");
            const auto old_value=wxl_search_field(search,0,&size); const std::string before(old_value,size);
            expect(wxl_set_event_text(event->context,1,"metadata",8)==0,"Search/cancel string metadata remains ordinary command text");
            const auto current=wxl_search_field(search,0,&size); expect(current && std::string(current,size)==before,"Command metadata does not edit query");
            expect(wxl_set_event_text(event->context,1,expected.data(),expected.size())==0,"Restore command string for parent probe");
        }
        if (token==936) expect(wxl_context_set_skipped(event->context,1)==0,"Search propagation");
    };
    auto send=[&](int kind) {
        expect(wxl_bind(search,kind,936,wxID_ANY,wxID_ANY,1,0)==0 && wxl_bind(frame,kind,937,wxID_ANY,wxID_ANY,1,0)==0,"Search native bindings");
        wxCommandEvent event(native_event_type(kind),window(search)->GetId()); event.SetEventObject(window(search)); event.SetString(expected);
        window(search)->HandleWindowEvent(event);
        expect(wxl_unbind(936)==1 && wxl_unbind(937)==1,"Search native unbind");
    };
    // Deliver native commands outside setter operations; prevent assumptions about cancel defaults.
    send(WXL_EVENT_SEARCH);
    expected=""; send(WXL_EVENT_SEARCH_CANCEL);
    expect(calls==4 && input_operations.empty(),"Direct search/cancel propagation and operation cleanup");
    size_t restored=0;
    // Native cancel may clear, so restore query before checking independent TEXT events.
    expect(wxl_set_search_field(search,0,"query λ",8,0)==0,"Restore query silently after native cancel");
    expected="query λ"; send(WXL_EVENT_TEXT_CHANGE);
    int64_t integer=-1;
    expect(calls==6 && wxl_event_field(expired,1,7,&integer)==-1,"Search contexts expire");
    expect(wxl_bind(search,WXL_EVENT_TEXT_CHANGE,936,wxID_ANY,wxID_ANY,1,0)==0 && wxl_bind(frame,WXL_EVENT_TEXT_CHANGE,937,wxID_ANY,wxID_ANY,1,0)==0,"Search TEXT notify bindings");
    expect(wxl_set_search_field(search,0,"query λ",8,0)==0 && wxl_set_search_field(search,1,"",0,0)==0 && calls==6,"Silent search fields");
    expect(wxl_set_search_field(search,0,"query λ",8,1)==0 && calls==8,"Exactly one explicit TEXT event even unchanged");
    expect(wxl_unbind(936)==1 && wxl_unbind(937)==1,"Search notify unbind");
    expect(!wxl_owned_event(WXL_EVENT_SEARCH,1,nullptr,0,0,0,0,0) && !wxl_owned_event(WXL_EVENT_SEARCH_CANCEL,1,nullptr,0,0,0,0,0),"Search/cancel owned events reject");
    // Private/composite child TEXT origin protects the owning search control.
    const auto child=wxl_text(search,"child",5,0,1);
    search_probe=[&](uint64_t,const wxl_event*) { ++calls; check_guards(); };
    expect(wxl_bind(frame,WXL_EVENT_TEXT_CHANGE,938,wxID_ANY,wxID_ANY,1,0)==0,"Search child TEXT bind");
    wxCommandEvent text(wxEVT_TEXT,window(child)->GetId()); text.SetEventObject(window(child)); text.SetString("child"); window(child)->HandleWindowEvent(text);
    expect(calls==9 && wxl_unbind(938)==1,"Search composite child input guard");
    auto* nonentry=new ManagedWindow<NonEntrySearchProbe>(window(panel),wxID_ANY,"query");
    const auto nonentry_id=track(nonentry); input_owners.emplace(nonentry,false);
    unsigned nonentry_calls=0;
    search_probe=[&](uint64_t,const wxl_event* event) {
        ++nonentry_calls;
        expect(std::string(event->text,event->text_length)=="stored λ" &&
               wxl_set_event_text(event->context,1,"metadata",8)==-1,"Search TEXT metadata guard independent of native text-entry hook");
        const auto clone=wxl_clone_event(event->context,1); size_t size=0;
        const auto value=wxl_event_text(clone,0,&size);
        expect(clone && value && std::string(value,size)=="stored λ" && wxl_event_destroy(clone)==0,"Non-lazy TEXT clone retains stored string");
    };
    expect(wxl_bind(nonentry_id,WXL_EVENT_TEXT_CHANGE,938,wxID_ANY,wxID_ANY,1,0)==0,"Non-entry search TEXT binding");
    wxCommandEvent stored(wxEVT_TEXT,nonentry->GetId()); stored.SetEventObject(nonentry); stored.SetString("stored λ"); nonentry->HandleWindowEvent(stored);
    expect(nonentry_calls==1 && wxl_unbind(938)==1 && wxl_destroy(nonentry_id)==0,"Non-entry search probe cleanup");
    auto* teardown=new ManagedWindow<EarlySearchDestroyProbe>(window(panel),wxID_ANY);
    const auto teardown_id=track(teardown); input_owners.emplace(teardown,false);
    unsigned teardown_callbacks=0;
    search_probe=[&](uint64_t,const wxl_event*) { ++teardown_callbacks; };
    expect(wxl_bind(frame,WXL_EVENT_SEARCH,936,wxID_ANY,wxID_ANY,1,0)==0,"Early search teardown binding");
    teardown->during_destroy=[&] { wxCommandEvent event(wxEVT_SEARCH,teardown->GetId()); event.SetEventObject(teardown); teardown->HandleWindowEvent(event); };
    teardown->refuse=true;
    expect(!teardown->Destroy() && !input_owners.at(teardown) && !teardown_callbacks,"Failed search Destroy restores Input guard");
    teardown->refuse=false;
    expect(wxl_destroy(teardown_id)==0 && !teardown_callbacks && wxl_unbind(936)==1,"Early search Destroy suppresses callbacks");
    search_probe={}; delete window(frame);
    expect(!wxl_search_field(search,0,&restored) && wxl_set_search_field(search,1,"stale",5,0)==-1,"Search teardown invalidates handles");
}

#include "activity-native.inc"
#include "canvas-native.inc"
#include "grid-native.inc"
#include "webview-native.inc"

int main() {
    std::thread([] { std::this_thread::sleep_for(std::chrono::seconds(10)); std::_Exit(124); }).detach();
    auto old_assert_handler = wxSetAssertHandler(capture_assert);
    unsigned foreign_destroyed = 0;
    bool active = false;
    try {
        wxIdleEvent::SetMode(wxIDLE_PROCESS_ALL);
        expect(wxl_start(ignore_event, ignore_ui_action) == 0, "Application startup failed");
        active = true;
        exercise_client_data(foreign_destroyed);
        exercise_binding_data();
        exercise_idle();
        exercise_menus();
        exercise_dialogs();
        exercise_generic_dialogs();
        exercise_text_editing();
        exercise_notebook();
        exercise_list_box();
        exercise_tree();
        exercise_splitter();
        exercise_frame_actions();
        exercise_accelerators();
#if wxUSE_CLIPBOARD
        exercise_clipboard();
#endif
        exercise_deferred_actions();
        exercise_ui_timers();
        exercise_numeric_input();
        exercise_radio_groups();
        exercise_toggle_buttons();
        exercise_combo_boxes();
        exercise_scrolling_panels();
        exercise_focus_navigation();
        exercise_tooltips();
        exercise_date_picker();
        exercise_time_picker();
        exercise_search_controls();
        exercise_activity_indicator();
        exercise_drawing_canvas();
        exercise_grid();
        exercise_web_view();
        bool shutdown_ran=false;
        ui_action_probe=[&](uint64_t,int) { shutdown_ran=true; };
        expect(wxl_defer_ui_action(5000,0)==0 && wxl_start_ui_timer(5001,1,0,0)==0,"Queue shutdown action/timer");
        expect(wxl_finish() == 0, "Application cleanup failed");
        active = false;
        expect(!shutdown_ran && ui_actions.empty() && ui_timers.empty() && !ui_dispatch,"Shutdown cancels pending actions before cleanup"); ui_action_probe={};
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
