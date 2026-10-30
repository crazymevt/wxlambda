#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/* Internal backend adapter (historical cocoa_* names are private).
 * WKWebView or opt-in WebKitGTK. GUI-thread only; backend remains wx-owned.
 * Returns 1 on successful setup/enqueue, 0 on invalid input/native exception.
 * Callback byte buffers are borrowed for the duration of the call only. */
int wxl_web_cocoa_configure(void* backend, uint64_t owner);
/* Check loaded backend ABI before creating any browser child. */
int wxl_web_backend_compatible(void);
int wxl_web_cocoa_run_script(void* backend, uint64_t owner, uint64_t ticket,
                             const char* bytes, size_t length);
int wxl_web_cocoa_load(void* backend, const char* bytes, size_t length,
                        const char* base, size_t base_length, int html);
void wxl_web_cocoa_teardown(void* backend);
void wxl_web_script_completed(uint64_t owner, uint64_t ticket, int success,
                              const char* bytes, size_t length);
void wxl_web_message_received(uint64_t owner, const char* bytes, size_t length);
#ifdef __cplusplus
}
#endif
