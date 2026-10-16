cmake_minimum_required(VERSION 3.22)
project(doomfix VERSION 0.4.1 LANGUAGES CXX RC)

if(NOT WIN32)
  message(FATAL_ERROR "doomfix собирается только под Windows")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Нужна x64-сборка: DOOMEternalx64vk.exe — 64-битный процесс")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Статический CRT: слою внутри процесса игры не нужен VC++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# DLL и манифест кладём рядом в build/bin — library_path в манифесте относительный.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(DOOMFIX_LAYER_NAME "VK_LAYER_DOOMFIX" CACHE STRING "Имя Vulkan-слоя")
set(DOOMFIX_VULKAN_HEADERS_TAG "v1.4.341" CACHE STRING "Тег Vulkan-Headers, если SDK не найден")
option(DOOMFIX_USE_SDK_HEADERS "Брать заголовки из установленного Vulkan SDK, если он есть" ON)
option(DOOMFIX_BUILD_TESTS "Собирать тестовую программу doomfix_barrier_test.exe" ON)

# --- Заголовки Vulkan ---------------------------------------------------------
# Слою нужны только заголовки (vulkan.h, vk_layer.h); vulkan-1.lib не линкуется.
if(DOOMFIX_USE_SDK_HEADERS)
  find_package(Vulkan QUIET)
endif()
if(TARGET Vulkan::Headers)
  message(STATUS "doomfix: заголовки Vulkan из SDK: ${Vulkan_INCLUDE_DIRS}")
else()
  message(STATUS "doomfix: Vulkan SDK не найден, скачиваю Vulkan-Headers ${DOOMFIX_VULKAN_HEADERS_TAG}")
  include(FetchContent)
  set(VULKAN_HEADERS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
  set(VULKAN_HEADERS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  set(VULKAN_HEADERS_ENABLE_MODULE OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(VulkanHeaders
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG ${DOOMFIX_VULKAN_HEADERS_TAG}
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(VulkanHeaders)
endif()

# Ресурс версии (Свойства → Подробно): название продукта и версия видны пользователю и нужны для
# подписи кода. Добавляет в цель сгенерированный .rc из src/version.rc.in.
function(doomfix_add_version_resource target description filetype)
  set(DOOMFIX_RC_NAME ${target})
  set(DOOMFIX_RC_DESCRIPTION "${description}")
  set(DOOMFIX_RC_FILETYPE ${filetype})
  get_target_property(type ${target} TYPE)
  if(type STREQUAL "SHARED_LIBRARY")
    set(DOOMFIX_RC_FILENAME ${target}.dll)
  else()
    set(DOOMFIX_RC_FILENAME ${target}.exe)
  endif()
  configure_file(src/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/${target}_version.rc @ONLY)
  target_sources(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${target}_version.rc)
endfunction()

# --- Слой ---------------------------------------------------------------------
add_library(doomfix_layer SHARED
  src/doomfix_layer.def
  src/common/config.cpp
  src/common/log.cpp
  src/common/util.cpp
  src/layer/chain_diag.cpp
  src/layer/device_create_edit.cpp
  src/layer/layer.cpp
  src/layer/vk_util.cpp
  src/features/features.cpp
  src/features/hide_device_extensions.cpp
  src/features/indirect_barrier_fix.cpp
)
target_include_directories(doomfix_layer PRIVATE src)
target_link_libraries(doomfix_layer PRIVATE Vulkan::Headers)
target_compile_definitions(doomfix_layer PRIVATE
  NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
  DOOMFIX_LAYER_NAME="${DOOMFIX_LAYER_NAME}"
  DOOMFIX_VERSION="${PROJECT_VERSION}"
)
target_compile_options(doomfix_layer PRIVATE /W4 /permissive- /utf-8)
doomfix_add_version_resource(doomfix_layer "doomfix Vulkan layer for DOOM Eternal 1.1" VFT_DLL)

# --- Лаунчер для ярлыков установщика (без PowerShell и консоли) ---------------------
add_executable(doomfix_launcher WIN32
  src/launcher/launcher.cpp
  src/launcher/launcher.manifest
  src/common/util.cpp
)
target_include_directories(doomfix_launcher PRIVATE src)
target_compile_definitions(doomfix_launcher PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_compile_options(doomfix_launcher PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(doomfix_launcher PRIVATE advapi32 ole32 shell32 user32)
doomfix_add_version_resource(doomfix_launcher "Starts DOOM Eternal with the doomfix layer" VFT_APP)
# Иконка из installer/assets (её рисует installer/assets/generate.py, см. scripts/build_assets.ps1).
set(DOOMFIX_ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/installer/assets/doomfix.ico)
configure_file(src/launcher/launcher_icon.rc.in ${CMAKE_CURRENT_BINARY_DIR}/launcher_icon.rc @ONLY)
target_sources(doomfix_launcher PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/launcher_icon.rc)
set_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/launcher_icon.rc APPEND PROPERTY OBJECT_DEPENDS ${DOOMFIX_ICON_FILE})

# --- Тест фичи indirect-barrier-fix (запускается вручную, см. README) -------------------
if(DOOMFIX_BUILD_TESTS)
  add_executable(doomfix_barrier_test tests/barrier_test.cpp)
  target_link_libraries(doomfix_barrier_test PRIVATE Vulkan::Headers)
  target_compile_definitions(doomfix_barrier_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_options(doomfix_barrier_test PRIVATE /W4 /permissive- /utf-8)
endif()

# --- Манифест слоя -------------------------------------------------------------
configure_file(layer/VK_LAYER_DOOMFIX.json.in ${CMAKE_CURRENT_BINARY_DIR}/layer_manifest.json.in @ONLY)
file(GENERATE
  OUTPUT $<TARGET_FILE_DIR:doomfix_layer>/${DOOMFIX_LAYER_NAME}.json
  INPUT ${CMAKE_CURRENT_BINARY_DIR}/layer_manifest.json.in)
