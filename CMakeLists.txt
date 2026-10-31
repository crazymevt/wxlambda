cmake_minimum_required(VERSION 3.16)
project(wxlambda VERSION 0.1.0 LANGUAGES CXX)
find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base adv webview stc aui)
include(${wxWidgets_USE_FILE})
include(CheckCXXSourceCompiles)
include(CMakePushCheckState)
# Directory COMPILE_DEFINITIONS from UsewxWidgets are not inherited by
# try_compile/try_run. In particular GTK needs __WXGTK__ and MSW needs the
# selected DLL/Unicode configuration even in isolated configure probes.
set(WXLAMBDA_WX_PROBE_DEFINITIONS)
foreach(definition IN LISTS wxWidgets_DEFINITIONS)
  list(APPEND WXLAMBDA_WX_PROBE_DEFINITIONS "-D${definition}")
endforeach()
option(WXLAMBDA_VERIFY_EDGE_COMPLETION_PATCH "Require the pinned Windows Edge prerequisite patch (does not enable WebView)" OFF)
if(WXLAMBDA_VERIFY_EDGE_COMPLETION_PATCH)
  if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "The Edge completion prerequisite currently targets Windows/MSVC")
  endif()
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_DEFINITIONS ${WXLAMBDA_WX_PROBE_DEFINITIONS})
  set(CMAKE_REQUIRED_INCLUDES ${wxWidgets_INCLUDE_DIRS})
  set(CMAKE_REQUIRED_LIBRARIES ${wxWidgets_LIBRARIES})
  unset(WXLAMBDA_HAS_EDGE_COMPLETION_PATCH CACHE)
  check_cxx_source_compiles("#include <wx/webview.h>
#include <wx/msw/webview_edge.h>
#include <wx/version.h>
#if !wxUSE_WEBVIEW_EDGE || wxMAJOR_VERSION!=3 || wxMINOR_VERSION!=2 || wxRELEASE_NUMBER!=12
#error Pinned wx3.2.12 with Edge enabled is required
#endif
int main() { return wxWebViewEdge::MSWGetWXLambdaCompletionGuardVersion()!=1; }" WXLAMBDA_HAS_EDGE_COMPLETION_PATCH)
  cmake_pop_check_state()
  if(NOT WXLAMBDA_HAS_EDGE_COMPLETION_PATCH)
    message(FATAL_ERROR "Patched Edge headers/binary not found: apply ci/patch-wx-edge.py and rebuild pinned wxWidgets")
  endif()
endif()
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_DEFINITIONS ${WXLAMBDA_WX_PROBE_DEFINITIONS})
set(CMAKE_REQUIRED_INCLUDES ${wxWidgets_INCLUDE_DIRS})
check_cxx_source_compiles("#include <wx/setup.h>
#if !wxUSE_FILEDLG || !wxUSE_DIRDLG || !wxUSE_FILECTRL || !wxUSE_TREECTRL || !wxUSE_BMPBUTTON || !wxUSE_STATLINE
#error Private generic dialogs require their wx GUI controls
#endif
int main() { return 0; }" WXLAMBDA_HAS_GENERIC_DIALOG_CONTROLS)
check_cxx_source_compiles("#include <wx/setup.h>
#if !wxUSE_ACCEL
#error wxUSE_ACCEL is required
#endif
int main() { return 0; }" WXLAMBDA_HAS_ACCELERATORS)
check_cxx_source_compiles("#include <wx/setup.h>
#if !wxUSE_TIMER
#error wxUSE_TIMER is required
#endif
int main() { return 0; }" WXLAMBDA_HAS_TIMERS)
check_cxx_source_compiles("#include <wx/setup.h>
#if !wxUSE_SCROLLBAR || !wxUSE_COMBOBOX || !wxUSE_TOGGLEBTN || !wxUSE_RADIOBOX || !wxUSE_RADIOBTN || !wxUSE_STATBOX || !wxUSE_SLIDER || !wxUSE_SPINCTRL || !wxUSE_SPINBTN
#error Numeric input controls are required
#endif
int main() { return 0; }" WXLAMBDA_HAS_SCROLLING_CONTROLS)
check_cxx_source_compiles("#include <wx/setup.h>
#if !wxUSE_TOOLTIPS
#error wxUSE_TOOLTIPS is required
#endif
int main() { return 0; }" WXLAMBDA_HAS_TOOLTIPS)
check_cxx_source_compiles("#include <wx/setup.h>
#if !wxUSE_DATEPICKCTRL || !wxUSE_DATETIME
#error Date picker and datetime support are required
#endif
int main() { return 0; }" WXLAMBDA_HAS_DATE_PICKER)
check_cxx_source_compiles("#include <wx/setup.h>
#if !wxUSE_TIMEPICKCTRL || !wxUSE_DATETIME
#error Time picker and datetime support are required
#endif
int main() { return 0; }" WXLAMBDA_HAS_TIME_PICKER)
check_cxx_source_compiles("#include <wx/setup.h>
#if !wxUSE_SEARCHCTRL || !wxUSE_TEXTCTRL
#error Search and text controls are required
#endif
int main() { return 0; }" WXLAMBDA_HAS_SEARCH_CONTROL)
check_cxx_source_compiles("#include <wx/setup.h>
#if !wxUSE_IMAGE || !wxUSE_LIBPNG || !wxUSE_GRAPHICS_CONTEXT || !wxUSE_ACTIVITYINDICATOR || !wxUSE_STREAMS || !wxUSE_FFILE
#error Drawing and activity support are required
#endif
int main() { return 0; }" WXLAMBDA_HAS_DRAWING_WORKFLOWS)
check_cxx_source_compiles("#include <wx/setup.h>
#if !wxUSE_LISTCTRL || !wxUSE_GRID || !wxUSE_WEBVIEW || !wxUSE_STC || !wxUSE_AUI
#error Grid and WebView required
#endif
int main() { return 0; }" WXLAMBDA_HAS_GRID_WEBVIEW)
check_cxx_source_compiles("#include <wx/setup.h>
#if !wxUSE_STC || !wxUSE_AUI
#error Styled text and AUI are required
#endif
int main() { return 0; }" WXLAMBDA_HAS_EDITOR_COMPONENTS)
check_cxx_source_compiles("#include <wx/setup.h>
#if !wxUSE_STATBMP || !wxUSE_DISPLAY || !wxUSE_IMAGE || !wxUSE_LIBPNG || !wxUSE_STREAMS || !wxUSE_FFILE
#error Image static bitmap and display support are required
#endif
int main() { return 0; }" WXLAMBDA_HAS_IMAGE_DISPLAYS)
check_cxx_source_compiles("#include <wx/setup.h>
#if !wxUSE_THREADS
#error Native thread locking is required
#endif
int main() { return 0; }" WXLAMBDA_HAS_THREADS)
cmake_pop_check_state()
if(NOT WXLAMBDA_HAS_IMAGE_DISPLAYS)
  message(FATAL_ERROR "wxlambda requires wxUSE_STATBMP=1, wxUSE_DISPLAY=1, wxUSE_IMAGE=1, wxUSE_LIBPNG=1, wxUSE_STREAMS=1 and wxUSE_FFILE=1 for image/display bindings")
endif()
if(NOT WXLAMBDA_HAS_THREADS)
  message(FATAL_ERROR "wxlambda requires wxUSE_THREADS=1 for worker event delivery")
endif()
if(NOT WXLAMBDA_HAS_EDITOR_COMPONENTS)
  message(FATAL_ERROR "wxlambda requires wxUSE_STC=1, wxUSE_AUI=1 and stc/aui components")
endif()
if(NOT WXLAMBDA_HAS_GRID_WEBVIEW)
  message(FATAL_ERROR "wxlambda requires wxUSE_GRID=1 and wxUSE_WEBVIEW=1 plus webview/stc/aui components and wxUSE_STC/wxUSE_AUI")
endif()
if(NOT WXLAMBDA_HAS_ACCELERATORS)
  message(FATAL_ERROR "wxlambda requires wxWidgets built with wxUSE_ACCEL=1")
endif()
if(NOT WXLAMBDA_HAS_TIMERS)
  message(FATAL_ERROR "wxlambda requires wxWidgets built with wxUSE_TIMER=1")
endif()
if(NOT WXLAMBDA_HAS_SCROLLING_CONTROLS)
  message(FATAL_ERROR "wxlambda requires wxUSE_SCROLLBAR=1, wxUSE_COMBOBOX=1, wxUSE_TOGGLEBTN=1, wxUSE_RADIOBOX=1, wxUSE_RADIOBTN=1, wxUSE_STATBOX=1, wxUSE_SLIDER=1, wxUSE_SPINCTRL=1 and wxUSE_SPINBTN=1")
endif()
if(NOT WXLAMBDA_HAS_TOOLTIPS)
  message(FATAL_ERROR "wxlambda requires wxWidgets built with wxUSE_TOOLTIPS=1")
endif()
if(NOT WXLAMBDA_HAS_DATE_PICKER)
  message(FATAL_ERROR "wxlambda requires wxUSE_DATEPICKCTRL=1 and wxUSE_DATETIME=1 (wxWidgets adv)")
endif()
if(NOT WXLAMBDA_HAS_TIME_PICKER)
  message(FATAL_ERROR "wxlambda requires wxUSE_TIMEPICKCTRL=1 and wxUSE_DATETIME=1 (wxWidgets adv)")
endif()
if(NOT WXLAMBDA_HAS_SEARCH_CONTROL)
  message(FATAL_ERROR "wxlambda requires wxUSE_SEARCHCTRL=1 and wxUSE_TEXTCTRL=1")
endif()
if(NOT WXLAMBDA_HAS_DRAWING_WORKFLOWS)
  message(FATAL_ERROR "wxlambda requires wxUSE_IMAGE/wxUSE_LIBPNG/wxUSE_GRAPHICS_CONTEXT/wxUSE_ACTIVITYINDICATOR/wxUSE_STREAMS/wxUSE_FFILE")
endif()
option(WXLAMBDA_EXPERIMENTAL_GTK_WEBVIEW "Enable experimental WebKitGTK WebView (Linux GTK3 only; runtime unverified)" OFF)
if(WXLAMBDA_EXPERIMENTAL_GTK_WEBVIEW)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Experimental GTK WebView requires Linux")
  endif()
  find_package(PkgConfig REQUIRED)
  # Match the WebKit ABI used by wxWidgets; never link both 4.0 and 4.1.
  set(WXLAMBDA_WEBKITGTK_PACKAGE "webkit2gtk-4.0" CACHE STRING "WebKitGTK pkg-config package matching wxWidgets")
  if(NOT WXLAMBDA_WEBKITGTK_PACKAGE MATCHES "^webkit2gtk-4\\.[01]$")
    message(FATAL_ERROR "Select webkit2gtk-4.0 or webkit2gtk-4.1 matching wxWidgets")
  endif()
  if(WXLAMBDA_WEBKITGTK_PACKAGE STREQUAL "webkit2gtk-4.0")
    set(WXLAMBDA_EXPECTED_WEBKITGTK_MASK 1)
  else()
    set(WXLAMBDA_EXPECTED_WEBKITGTK_MASK 2)
  endif()
  pkg_check_modules(WXLAMBDA_WEBKITGTK REQUIRED IMPORTED_TARGET ${WXLAMBDA_WEBKITGTK_PACKAGE}>=2.22 glib-2.0>=2.66)
  # Mixed libsoup generations can abort at library load, before a runtime guard.
  # Probe in a disposable configure process, not in the user's Lisp process.
  if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Experimental WebView requires a native ABI probe; cross compilation is not supported")
  endif()
  include(CheckCXXSourceRuns)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_DEFINITIONS ${WXLAMBDA_WX_PROBE_DEFINITIONS})
  set(CMAKE_REQUIRED_INCLUDES ${wxWidgets_INCLUDE_DIRS})
  set(CMAKE_REQUIRED_LIBRARIES ${wxWidgets_LIBRARIES} PkgConfig::WXLAMBDA_WEBKITGTK ${CMAKE_DL_LIBS})
  unset(WXLAMBDA_WEBKITGTK_ABI_MATCHES CACHE)
  check_cxx_source_runs("#include <wx/webview.h>
#include <webkit2/webkit2.h>
#include <link.h>
#include <cstring>
int main() {
  wxWebView* (*volatile factory)(const wxString&) = &wxWebView::New;
  if (!factory || !webkit_get_major_version()) return 1;
  unsigned seen=0;
  dl_iterate_phdr([](dl_phdr_info* p, size_t, void* data) {
    auto& mask=*static_cast<unsigned*>(data);
    if (std::strstr(p->dlpi_name, \"libwebkit2gtk-4.0.so\")) mask|=1;
    if (std::strstr(p->dlpi_name, \"libwebkit2gtk-4.1.so\")) mask|=2;
    return 0;
  }, &seen);
  return seen==${WXLAMBDA_EXPECTED_WEBKITGTK_MASK} ? 0 : 1;
}" WXLAMBDA_WEBKITGTK_ABI_MATCHES)
  cmake_pop_check_state()
  if(NOT WXLAMBDA_WEBKITGTK_ABI_MATCHES)
    message(FATAL_ERROR "WebKitGTK ABI probe failed: wxWidgets and bridge must use the same WebKitGTK ABI; see CMake error log")
  endif()
endif()
if(APPLE)
  enable_language(OBJCXX)
  set(WXLAMBDA_WEBVIEW_SOURCE native/webview-cocoa.mm)
elseif(WXLAMBDA_EXPERIMENTAL_GTK_WEBVIEW)
  set(WXLAMBDA_WEBVIEW_SOURCE native/webview-gtk.cpp)
  add_compile_definitions(WXLAMBDA_EXPERIMENTAL_GTK_WEBVIEW=1)
  add_compile_definitions(WXLAMBDA_EXPECTED_WEBKITGTK_MASK=${WXLAMBDA_EXPECTED_WEBKITGTK_MASK})
else()
  set(WXLAMBDA_WEBVIEW_SOURCE native/webview-unavailable.cpp)
endif()
set(WXLAMBDA_GENERIC_DIALOG_SOURCES native/generic-dialogs/filedlgg.cpp native/generic-dialogs/dirdlgg.cpp)
if(NOT WXLAMBDA_HAS_GENERIC_DIALOG_CONTROLS)
  message(FATAL_ERROR "Private generic dialogs require wxUSE_FILEDLG/DIRDLG/FILECTRL/TREECTRL/BMPBUTTON/STATLINE=1")
endif()
add_library(wxlambda SHARED native/wxlambda.cpp ${WXLAMBDA_WEBVIEW_SOURCE} ${WXLAMBDA_GENERIC_DIALOG_SOURCES})
target_compile_features(wxlambda PRIVATE cxx_std_17)
target_compile_definitions(wxlambda PRIVATE WXLAMBDA_BUILD)
target_link_libraries(wxlambda PRIVATE ${wxWidgets_LIBRARIES})
if(WXLAMBDA_EXPERIMENTAL_GTK_WEBVIEW)
  target_link_libraries(wxlambda PRIVATE PkgConfig::WXLAMBDA_WEBKITGTK ${CMAKE_DL_LIBS})
endif()
if(APPLE)
  target_link_libraries(wxlambda PRIVATE "-framework WebKit")
endif()
set_target_properties(wxlambda PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
install(TARGETS wxlambda LIBRARY DESTINATION lib RUNTIME DESTINATION bin ARCHIVE DESTINATION lib)
install(FILES native/generic-dialogs/LICENCE.txt native/generic-dialogs/LGPL.txt DESTINATION share/doc/wxlambda/generic-dialogs)
install(FILES native/wxlambda.h native/drawing-canvas-api.h native/activity-indicator-api.h native/grid-api.h native/grid-extra-api.h native/webview-api.h native/stc-api.h native/input-events-api.h native/aui-api.h native/thread-events-api.h native/views-api.h native/images-api.h DESTINATION include)

option(WXLAMBDA_BUILD_NATIVE_TESTS "Build native invariant regressions (GUI session required)" OFF)
option(WXLAMBDA_BUILD_PORTABILITY_TESTS "Build GUI-independent asynchronous lifetime regression" OFF)
if(WXLAMBDA_BUILD_PORTABILITY_TESTS)
  enable_testing()
  add_executable(wxlambda-async-owner-native tests/async-owner-native.cpp)
  target_compile_features(wxlambda-async-owner-native PRIVATE cxx_std_11)
  add_test(NAME asynchronous-owner-retirement COMMAND wxlambda-async-owner-native)
endif()
if(WXLAMBDA_BUILD_NATIVE_TESTS)
  add_executable(wxlambda-client-data-native tests/client-data-native.cpp ${WXLAMBDA_WEBVIEW_SOURCE} ${WXLAMBDA_GENERIC_DIALOG_SOURCES})
  target_compile_features(wxlambda-client-data-native PRIVATE cxx_std_17)
  target_compile_definitions(wxlambda-client-data-native PRIVATE WXLAMBDA_BUILD)
  target_link_libraries(wxlambda-client-data-native PRIVATE ${wxWidgets_LIBRARIES})
  if(WXLAMBDA_EXPERIMENTAL_GTK_WEBVIEW)
    target_link_libraries(wxlambda-client-data-native PRIVATE PkgConfig::WXLAMBDA_WEBKITGTK ${CMAKE_DL_LIBS})
  endif()
  if(APPLE)
    target_link_libraries(wxlambda-client-data-native PRIVATE "-framework WebKit")
  endif()
endif()
