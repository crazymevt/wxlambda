#include "webview-cocoa.h"
extern "C" int wxl_web_cocoa_configure(void*,uint64_t) { return 0; }
extern "C" int wxl_web_cocoa_run_script(void*,uint64_t,uint64_t,const char*,size_t) { return 0; }
extern "C" void wxl_web_cocoa_teardown(void*) {}

extern "C" int wxl_web_cocoa_load(void*,const char*,size_t,const char*,size_t,int) { return 0; }
