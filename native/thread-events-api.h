#pragma once
enum { WXL_EVENT_THREAD = 120 };
/* The only worker-thread-capable public native operation. Copies bytes before
 * return; does not consult GUI state.0 accepted,-1 stale/full/invalid/shutdown.
 * A GUI BIND-EVENT :THREAD registration must first establish this receiver. */
WXL_API int wxl_queue_thread_event(wxl_handle receiver,const char* text,size_t length);
