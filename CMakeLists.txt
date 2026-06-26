cmake_minimum_required(VERSION 3.16)
project(npdrive_velox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/W4 /permissive- /EHsc)
endif()

# --- Core library: transport + device + positioner abstraction ---
add_library(npdrive_core STATIC
  src/json_rpc_client.cpp
  src/np_drive.cpp
  src/positioner.cpp
)
target_include_directories(npdrive_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
if(WIN32)
  target_link_libraries(npdrive_core PUBLIC Ws2_32)
endif()

# --- Velox-facing driver DLL (PLACEHOLDER ABI; reshape to FormFactor's ICD) ---
add_library(npdrive_velox SHARED src/velox_adapter.cpp)
target_compile_definitions(npdrive_velox PRIVATE NPX_BUILD_DLL)
target_link_libraries(npdrive_velox PRIVATE npdrive_core)

# --- Smoke test (builds + runs offline; opt-in live test via NPDRIVE_HOST) ---
add_executable(npdrive_smoke test/smoke_test.cpp)
target_link_libraries(npdrive_smoke PRIVATE npdrive_core)

enable_testing()
add_test(NAME smoke COMMAND npdrive_smoke)
