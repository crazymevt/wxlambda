// Include early for declarations/helpers, and at tail with
// WXL_THREAD_EVENT_IMPLEMENTATION defined for registry/worker export.
#ifndef WXL_THREAD_EVENT_IMPLEMENTATION
#ifndef WXL_THREAD_EVENT_DECLARED
#define WXL_THREAD_EVENT_DECLARED
#if !wxUSE_THREADS
#error "Thread event delivery requires wxUSE_THREADS=1"
#endif
void thread_endpoint_start();
void thread_endpoint_register(wxl_handle receiver,wxEvtHandler* target);
void thread_endpoint_invalidate(wxEvtHandler* target) noexcept;
bool thread_endpoint_retire(wxEvtHandler* target) noexcept;
void thread_endpoint_restore(wxEvtHandler* target,bool previous) noexcept;
void thread_endpoint_shutdown() noexcept;
void thread_endpoint_delete_pending(wxEvtHandler* target);
bool thread_event_kind(int kind) { return kind==WXL_EVENT_THREAD; }
bool thread_payload_readonly(wxEventType type) { return type==wxEVT_THREAD; }
wxString thread_event_text(wxEvent& event);
#endif
#else
namespace {
struct ThreadEndpoint { wxEvtHandler* target=nullptr; bool retiring=false; std::atomic<size_t> pending{0}; };
std::mutex thread_endpoint_mutex;
std::unordered_map<wxl_handle,std::shared_ptr<ThreadEndpoint>> thread_endpoints;
bool thread_ingress=false;
std::atomic<size_t> thread_pending{0};
constexpr size_t thread_payload_limit=1048576;
bool thread_utf8(const char* bytes,size_t length) {
    if ((!bytes && length) || length>thread_payload_limit) return false;
    for (size_t i=0;i<length;) {
        const auto c=static_cast<unsigned char>(bytes[i++]);
        if (c<0x80) continue;
        unsigned needed=0; uint32_t point=0,minimum=0;
        if (c>=0xc2 && c<=0xdf) { needed=1;point=c&0x1f;minimum=0x80; }
        else if (c>=0xe0 && c<=0xef) { needed=2;point=c&0x0f;minimum=0x800; }
        else if (c>=0xf0 && c<=0xf4) { needed=3;point=c&7;minimum=0x10000; }
        else return false;
        if (needed>length-i) return false;
        while (needed--) { const auto next=static_cast<unsigned char>(bytes[i++]); if ((next&0xc0)!=0x80) return false; point=(point<<6)|(next&0x3f); }
        if (point<minimum || point>0x10ffff || (point>=0xd800 && point<=0xdfff)) return false;
    }
    return true;
}
class ThreadPacket final : public wxThreadEvent {
public:
    std::shared_ptr<ThreadEndpoint> endpoint;
    bool counted=false;
    explicit ThreadPacket(const std::string& payload):wxThreadEvent(wxEVT_THREAD,wxID_ANY) {
        // Clone gives the native event its own string buffer. Both temporary
        // wxStrings die before construction returns, before queue publication.
        SetString(wxString::FromUTF8(payload.data(),payload.size()).Clone());
    }
    ~ThreadPacket() override {
        if (!counted) return;
        // wx DeletePendingEvents may delete under its pending-event lock.
        // Never acquire the endpoint mutex here (posting takes it before wx).
        endpoint->pending.fetch_sub(1,std::memory_order_relaxed);
        thread_pending.fetch_sub(1,std::memory_order_relaxed);
    }
    // Inherited wxThreadEvent::Clone deep-copies native string payload and
    // returns a base wxThreadEvent, so it never duplicates our reservation.
};
void thread_endpoint_start() {
    ready(); std::lock_guard<std::mutex> lock(thread_endpoint_mutex);
    require(!thread_ingress && thread_endpoints.empty() && !thread_pending.load(std::memory_order_relaxed),"Previous thread event ingress has not retired");
    thread_ingress=true;
}
void thread_endpoint_register(wxl_handle receiver,wxEvtHandler* target) {
    ready(); require(receiver && target,"Missing thread receiver identity");
    std::lock_guard<std::mutex> lock(thread_endpoint_mutex);
    require(thread_ingress,"Thread event ingress is closed");
    const auto found=thread_endpoints.find(receiver);
    if (found!=thread_endpoints.end()) { require(found->second->target==target,"Thread receiver identity was reused"); return; }
    auto endpoint=std::make_shared<ThreadEndpoint>(); endpoint->target=target;
    thread_endpoints.emplace(receiver,std::move(endpoint));
}
void thread_endpoint_invalidate(wxEvtHandler* target) noexcept {
    std::lock_guard<std::mutex> lock(thread_endpoint_mutex);
    for (auto item=thread_endpoints.begin();item!=thread_endpoints.end();) {
        if (item->second->target==target) { item->second->target=nullptr; item=thread_endpoints.erase(item); }
        else ++item;
    }
}
bool thread_endpoint_retire(wxEvtHandler* target) noexcept {
    std::lock_guard<std::mutex> lock(thread_endpoint_mutex);
    for (const auto& item:thread_endpoints) if (item.second->target==target) {
        const bool previous=item.second->retiring; item.second->retiring=true; return previous;
    }
    return false;
}
void thread_endpoint_restore(wxEvtHandler* target,bool previous) noexcept {
    std::lock_guard<std::mutex> lock(thread_endpoint_mutex);
    for (const auto& item:thread_endpoints) if (item.second->target==target) item.second->retiring=previous;
}
void thread_endpoint_shutdown() noexcept {
    std::lock_guard<std::mutex> lock(thread_endpoint_mutex);
    thread_ingress=false;
    for (const auto& item:thread_endpoints) item.second->target=nullptr;
    thread_endpoints.clear();
}
void thread_endpoint_delete_pending(wxEvtHandler* target) {
    ready();
    std::lock_guard<std::mutex> lock(thread_endpoint_mutex);
    target->DeletePendingEvents();
    // Exclude a concurrent post between deletion and wxApp unregistration.
    wxTheApp->RemovePendingEventHandler(target);
}
wxString thread_event_text(wxEvent& event) {
    require(event.GetEventType()==wxEVT_THREAD,"Expected native thread event");
    auto* thread=dynamic_cast<wxThreadEvent*>(&event); require(thread,"Expected native wxThreadEvent");
    return thread->GetString();
}
}
extern "C" int wxl_queue_thread_event(wxl_handle receiver,const char* text,size_t length) {
    // status() changes only per-thread native-entry depth and error storage.
    // Do not add ready(), wxWeakRef or ordinary GUI registry access here.
    return status([&] {
        require(thread_utf8(text,length),"Thread event requires valid UTF-8 of at most1MiB");
        auto packet=std::make_unique<ThreadPacket>(std::string(text ? text : "",length));
        {
            std::lock_guard<std::mutex> lock(thread_endpoint_mutex);
            require(thread_ingress,"Thread event ingress is closed");
            const auto found=thread_endpoints.find(receiver);
            require(found!=thread_endpoints.end() && found->second->target && !found->second->retiring,"Thread receiver is stale, retiring or has no GUI registration");
            auto endpoint=found->second;
            require(endpoint->pending.load(std::memory_order_relaxed)<256 && thread_pending.load(std::memory_order_relaxed)<4096,"Thread event queue is full");
            packet->endpoint=endpoint; packet->SetEventObject(endpoint->target);
            ++endpoint->pending; ++thread_pending; packet->counted=true;
            // Qualified base call avoids GUI-specific QueueEvent overrides.
            // Ownership transfers to wx and no posting-thread payload is shared.
            endpoint->target->wxEvtHandler::QueueEvent(packet.release());
        }
    });
}
#endif
