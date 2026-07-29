cmake_minimum_required(VERSION 3.16)
project(FabricFoundation LANGUAGES C CXX)

add_library(FabricRuntime SHARED
  src/Fabric/FabricRuntime.cpp
  src/Fabric/Backends/Amber/AmberDynamicLibrary.cpp
  src/Fabric/Backends/Amber/AmberBackendProvider.cpp)
target_include_directories(FabricRuntime PUBLIC include PRIVATE src/Fabric)
target_include_directories(FabricRuntime PRIVATE src/Fabric/Backends/Amber)
target_compile_definitions(FabricRuntime PRIVATE FABRIC_EXPORTS)
target_compile_features(FabricRuntime PUBLIC cxx_std_17)
set_target_properties(FabricRuntime PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS OFF)
if(UNIX AND NOT APPLE)
  target_link_libraries(FabricRuntime PRIVATE dl)
endif()
if(MSVC)
  target_compile_options(FabricRuntime PRIVATE /W4 /WX)
else()
  target_compile_options(FabricRuntime PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()
add_library(FakeAmberApiV2 SHARED tests/fakes/FakeAmberApiV2.cpp)
target_include_directories(FakeAmberApiV2 PRIVATE include)
target_compile_definitions(FakeAmberApiV2 PRIVATE AMBER_BRIDGE_EXPORTS)
target_compile_features(FakeAmberApiV2 PRIVATE cxx_std_17)
add_executable(FabricRuntimeTests tests/fabric_runtime_tests.cpp tests/fakes/FakeBackend.cpp)
target_include_directories(FabricRuntimeTests PRIVATE include src/Fabric tests/fakes)
target_link_libraries(FabricRuntimeTests PRIVATE FabricRuntime)
add_test(NAME FabricRuntimeTests COMMAND FabricRuntimeTests)
target_compile_definitions(FabricRuntimeTests PRIVATE FAKE_AMBER_PATH="$<TARGET_FILE:FakeAmberApiV2>")
add_dependencies(FabricRuntimeTests FakeAmberApiV2)

add_test(NAME FabricExportInspection
  COMMAND ${CMAKE_COMMAND} -DLIBRARY=$<TARGET_FILE:FabricRuntime>
          -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_fabric_exports.cmake)

add_executable(FabricHeaderC tests/fabric_header_c.c)
target_include_directories(FabricHeaderC PRIVATE include)
set_property(TARGET FabricHeaderC PROPERTY C_STANDARD 11)
add_executable(FabricHeaderCpp tests/fabric_header_cpp.cpp)
target_include_directories(FabricHeaderCpp PRIVATE include)
target_compile_features(FabricHeaderCpp PRIVATE cxx_std_17)
add_test(NAME FabricHeaderC COMMAND FabricHeaderC)
add_test(NAME FabricHeaderCpp COMMAND FabricHeaderCpp)
