// Included in the native ABI block after checked window/operation helpers.
wxl_handle wxl_activity_indicator(wxl_handle parent_id, int running) {
    return create([&] {
        auto* parent=window(parent_id);
        require(running==0 || running==1,"Invalid activity running flag");
        auto object=std::make_unique<ManagedWindow<wxActivityIndicator>>();
        { wxEventBlocker blocker(object.get());
          require(object->Create(parent,wxID_ANY),"Native activity indicator creation failed");
          if (running) object->Start();
          require(object->IsRunning()==(running!=0),"Native activity indicator did not accept initial state"); }
        const auto id=track(object.get()); object.release(); return id;
    });
}
int wxl_activity_running(wxl_handle id) {
    return boolean_result([&] { return as<wxActivityIndicator>(id)->IsRunning(); });
}
int wxl_set_activity_running(wxl_handle id, int running) { return status([&] {
    auto* object=as<wxActivityIndicator>(id);
    require(running==0 || running==1,"Invalid activity running flag");
    require(callback_depth==0,"Defer activity state changes until callbacks return");
    InputOperation operation(object);
    if (running) object->Start(); else object->Stop();
    require(object->IsRunning()==(running!=0),"Native activity indicator did not accept state");
}); }
