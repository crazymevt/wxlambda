cmake_minimum_required(VERSION 3.16)
project(wxlambda VERSION 0.1.0 LANGUAGES CXX)
find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base adv)
include(${wxWidgets_USE_FILE})
include(CheckCXXSourceCompiles)
include(CMakePushCheckState)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_INCLUDES ${wxWidgets_INCLUDE_DIRS})
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
cmake_pop_check_state()
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
add_library(wxlambda SHARED native/wxlambda.cpp)
target_compile_features(wxlambda PRIVATE cxx_std_17)
target_compile_definitions(wxlambda PRIVATE WXLAMBDA_BUILD)
target_link_libraries(wxlambda PRIVATE ${wxWidgets_LIBRARIES})
set_target_properties(wxlambda PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
install(TARGETS wxlambda LIBRARY DESTINATION lib RUNTIME DESTINATION bin ARCHIVE DESTINATION lib)
install(FILES native/wxlambda.h native/drawing-canvas-api.h native/activity-indicator-api.h DESTINATION include)

option(WXLAMBDA_BUILD_NATIVE_TESTS "Build native invariant regressions (GUI session required)" OFF)
if(WXLAMBDA_BUILD_NATIVE_TESTS)
  add_executable(wxlambda-client-data-native tests/client-data-native.cpp)
  target_compile_features(wxlambda-client-data-native PRIVATE cxx_std_17)
  target_compile_definitions(wxlambda-client-data-native PRIVATE WXLAMBDA_BUILD)
  target_link_libraries(wxlambda-client-data-native PRIVATE ${wxWidgets_LIBRARIES})
endif()
