cmake_minimum_required(VERSION 3.16)
project(FabricFoundation LANGUAGES C CXX)

execute_process(COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE FABRIC_SOURCE_REVISION
  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT FABRIC_SOURCE_REVISION)
  set(FABRIC_SOURCE_REVISION unknown)
endif()

add_library(FabricRuntimeCore OBJECT
  src/Fabric/FabricRuntime.cpp
  src/Fabric/Backends/Amber/AmberDynamicLibrary.cpp
  src/Fabric/Backends/Amber/AmberTrace.cpp
  src/Fabric/Backends/Amber/AmberLegacyAdapter.cpp
  src/Fabric/Backends/Amber/AmberBackendProvider.cpp)
target_include_directories(FabricRuntimeCore PRIVATE
  include
  src/Fabric
  src/Fabric/Backends/Amber)
target_compile_definitions(FabricRuntimeCore PRIVATE FABRIC_EXPORTS
  FABRIC_SOURCE_REVISION="${FABRIC_SOURCE_REVISION}")
target_compile_features(FabricRuntimeCore PRIVATE cxx_std_17)
set_target_properties(FabricRuntimeCore PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  target_compile_options(FabricRuntimeCore PRIVATE /W4 /WX)
else()
  target_compile_options(FabricRuntimeCore PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Keep the production DLL as the sole public runtime target.  Tests that need
# internal C++ composition hooks embed the same implementation objects instead
# of attempting to import those hooks from the DLL.
add_library(FabricRuntime SHARED $<TARGET_OBJECTS:FabricRuntimeCore>)
target_include_directories(FabricRuntime PUBLIC include)
target_compile_features(FabricRuntime PUBLIC cxx_std_17)
set_target_properties(FabricRuntime PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS OFF)
if(CMAKE_DL_LIBS)
  target_link_libraries(FabricRuntime PRIVATE ${CMAKE_DL_LIBS})
endif()

enable_testing()
add_library(FakeAmberApiV2 SHARED tests/fakes/FakeAmberApiV2.cpp)
target_include_directories(FakeAmberApiV2 PRIVATE include tests/fakes)
target_compile_definitions(FakeAmberApiV2 PRIVATE AMBER_BRIDGE_EXPORTS)
target_compile_features(FakeAmberApiV2 PRIVATE cxx_std_17)
add_library(FakeAmberNoApi SHARED tests/fakes/FakeAmberNoApi.cpp)
add_library(FakeAmberLegacy SHARED tests/fakes/FakeAmberLegacy.cpp)
target_include_directories(FakeAmberLegacy PRIVATE src/Cores/JPMSystem6)
target_compile_features(FakeAmberLegacy PRIVATE cxx_std_17)
add_library(FakeAmberLegacyMissingConfig SHARED tests/fakes/FakeAmberLegacy.cpp)
target_include_directories(FakeAmberLegacyMissingConfig PRIVATE src/Cores/JPMSystem6)
target_compile_definitions(FakeAmberLegacyMissingConfig PRIVATE FAKE_AMBER_OMIT_SET_STEPS)
target_compile_features(FakeAmberLegacyMissingConfig PRIVATE cxx_std_17)
add_library(FakeAmberLegacy48Stereo SHARED tests/fakes/FakeAmberLegacy.cpp)
target_include_directories(FakeAmberLegacy48Stereo PRIVATE src/Cores/JPMSystem6)
target_compile_definitions(FakeAmberLegacy48Stereo PRIVATE
  FAKE_AMBER_SAMPLE_RATE=48000 FAKE_AMBER_CHANNELS=2)
target_compile_features(FakeAmberLegacy48Stereo PRIVATE cxx_std_17)
add_library(FakeAmberLegacy48Mono SHARED tests/fakes/FakeAmberLegacy.cpp)
target_include_directories(FakeAmberLegacy48Mono PRIVATE src/Cores/JPMSystem6)
target_compile_definitions(FakeAmberLegacy48Mono PRIVATE
  FAKE_AMBER_SAMPLE_RATE=48000 FAKE_AMBER_CHANNELS=1)
target_compile_features(FakeAmberLegacy48Mono PRIVATE cxx_std_17)
add_library(FakeAmberLegacyPartial SHARED tests/fakes/FakeAmberLegacy.cpp)
target_include_directories(FakeAmberLegacyPartial PRIVATE src/Cores/JPMSystem6)
target_compile_definitions(FakeAmberLegacyPartial PRIVATE
  FAKE_AMBER_SAMPLE_RATE=48000 FAKE_AMBER_CHANNELS=2
  FAKE_AMBER_MAX_FRAMES_PER_READ=7)
target_compile_features(FakeAmberLegacyPartial PRIVATE cxx_std_17)
add_executable(FabricRuntimeTests
  tests/fabric_runtime_tests.cpp
  tests/fakes/FakeBackend.cpp
  $<TARGET_OBJECTS:FabricRuntimeCore>)
target_include_directories(FabricRuntimeTests PRIVATE include src/Fabric tests/fakes)
target_compile_definitions(FabricRuntimeTests PRIVATE FABRIC_EXPORTS)
target_link_libraries(FabricRuntimeTests PRIVATE FakeAmberApiV2)
if(CMAKE_DL_LIBS)
  target_link_libraries(FabricRuntimeTests PRIVATE ${CMAKE_DL_LIBS})
endif()
add_test(NAME FabricRuntimeTests COMMAND FabricRuntimeTests)
add_executable(AmberBackendTests tests/amber_backend_tests.cpp)
target_include_directories(AmberBackendTests PRIVATE include tests/fakes)
target_link_libraries(AmberBackendTests PRIVATE FabricRuntime FakeAmberApiV2)
target_compile_definitions(AmberBackendTests PRIVATE FAKE_AMBER_PATH="$<TARGET_FILE:FakeAmberApiV2>")
target_compile_definitions(AmberBackendTests PRIVATE FAKE_AMBER_NO_API_PATH="$<TARGET_FILE:FakeAmberNoApi>")
add_test(NAME AmberBackendTests COMMAND AmberBackendTests)
add_executable(AmberLegacyBackendTests tests/amber_legacy_backend_tests.cpp)
target_include_directories(AmberLegacyBackendTests PRIVATE include)
target_link_libraries(AmberLegacyBackendTests PRIVATE FabricRuntime)
target_compile_definitions(AmberLegacyBackendTests PRIVATE
  FAKE_AMBER_LEGACY_PATH="$<TARGET_FILE:FakeAmberLegacy>"
  FAKE_AMBER_LEGACY_MISSING_CONFIG_PATH="$<TARGET_FILE:FakeAmberLegacyMissingConfig>"
  FAKE_AMBER_NO_API_PATH="$<TARGET_FILE:FakeAmberNoApi>")
add_dependencies(AmberLegacyBackendTests FakeAmberLegacy
  FakeAmberLegacyMissingConfig FakeAmberNoApi)
add_test(NAME AmberLegacyBackendTests COMMAND AmberLegacyBackendTests)
add_executable(AmberLegacyAudioTests tests/amber_legacy_audio_tests.cpp)
target_include_directories(AmberLegacyAudioTests PRIVATE include)
target_link_libraries(AmberLegacyAudioTests PRIVATE FabricRuntime)
target_compile_definitions(AmberLegacyAudioTests PRIVATE
  FAKE_AMBER_441_STEREO_PATH="$<TARGET_FILE:FakeAmberLegacy>"
  FAKE_AMBER_48_STEREO_PATH="$<TARGET_FILE:FakeAmberLegacy48Stereo>"
  FAKE_AMBER_48_MONO_PATH="$<TARGET_FILE:FakeAmberLegacy48Mono>"
  FAKE_AMBER_PARTIAL_PATH="$<TARGET_FILE:FakeAmberLegacyPartial>")
add_dependencies(AmberLegacyAudioTests FakeAmberLegacy FakeAmberLegacy48Stereo
  FakeAmberLegacy48Mono FakeAmberLegacyPartial)
add_test(NAME AmberLegacyAudioTests COMMAND AmberLegacyAudioTests)
add_executable(AmberTraceTests tests/amber_trace_tests.cpp
  $<TARGET_OBJECTS:FabricRuntimeCore>)
target_include_directories(AmberTraceTests PRIVATE include src/Fabric
  src/Fabric/Backends/Amber)
target_compile_definitions(AmberTraceTests PRIVATE FABRIC_EXPORTS)
target_compile_definitions(AmberTraceTests PRIVATE
  FAKE_AMBER_LEGACY_PATH="$<TARGET_FILE:FakeAmberLegacy>")
add_dependencies(AmberTraceTests FakeAmberLegacy)
if(CMAKE_DL_LIBS)
  target_link_libraries(AmberTraceTests PRIVATE ${CMAKE_DL_LIBS})
endif()
target_compile_features(AmberTraceTests PRIVATE cxx_std_17)
foreach(mode explicit default disabled invalid)
  add_test(NAME AmberTraceTests-${mode} COMMAND AmberTraceTests ${mode})
endforeach()
target_compile_definitions(FabricRuntimeTests PRIVATE FAKE_AMBER_PATH="$<TARGET_FILE:FakeAmberApiV2>")
add_dependencies(FabricRuntimeTests FakeAmberApiV2)

add_test(NAME FabricExportInspection
  COMMAND ${CMAKE_COMMAND} -DLIBRARY=$<TARGET_FILE:FabricRuntime>
          -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_fabric_exports.cmake)

add_executable(FabricHeaderC tests/fabric_header_c.c)
target_include_directories(FabricHeaderC PRIVATE include)
target_link_libraries(FabricHeaderC PRIVATE FabricRuntime FakeAmberApiV2)
target_compile_definitions(FabricHeaderC PRIVATE FAKE_AMBER_PATH="$<TARGET_FILE:FakeAmberApiV2>")
set_property(TARGET FabricHeaderC PROPERTY C_STANDARD 11)
add_executable(FabricHeaderCpp tests/fabric_header_cpp.cpp)
target_include_directories(FabricHeaderCpp PRIVATE include)
target_link_libraries(FabricHeaderCpp PRIVATE FabricRuntime FakeAmberApiV2)
target_compile_definitions(FabricHeaderCpp PRIVATE FAKE_AMBER_PATH="$<TARGET_FILE:FakeAmberApiV2>")
target_compile_features(FabricHeaderCpp PRIVATE cxx_std_17)
add_test(NAME FabricHeaderC COMMAND FabricHeaderC)
add_test(NAME FabricHeaderCpp COMMAND FabricHeaderCpp)
