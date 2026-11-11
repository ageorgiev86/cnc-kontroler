# Операторски интерфейс на CNC контролера (LVGL 9).
#
# Две цели:
#   cnc-ui      — Linux платката (Orange Pi / Raspberry Pi): DRM/KMS + libinput
#   cnc-ui-sim  — симулатор на компютър със SDL2
# Всяка цел се строи, ако библиотеките ѝ са налични. На Orange Pi:
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
cmake_minimum_required(VERSION 3.16)
project(cnc_ui LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(LVGL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lvgl)
if(NOT EXISTS ${LVGL_DIR}/lvgl.h)
  message(FATAL_ERROR "Липсва LVGL. Изпълнете: git submodule update --init ui/lvgl/lvgl")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 IMPORTED_TARGET sdl2)
pkg_check_modules(DRM IMPORTED_TARGET libdrm)
pkg_check_modules(LIBINPUT IMPORTED_TARGET libinput)

option(CNC_UI_BUILD_SIM "Симулатор със SDL2" ${SDL2_FOUND})
option(CNC_UI_BUILD_DEVICE "Цел за платката (DRM + libinput)" OFF)
if(DRM_FOUND AND LIBINPUT_FOUND)
  set(CNC_UI_BUILD_DEVICE ON CACHE BOOL "" FORCE)
endif()
if(NOT CNC_UI_BUILD_SIM AND NOT CNC_UI_BUILD_DEVICE)
  message(FATAL_ERROR "Няма какво да се строи: нужни са libsdl2-dev (симулатор) или libdrm-dev + libinput-dev (платка).")
endif()

# Ядрото (core/): интерпретатор и планер на G-code, без LVGL
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../core ${CMAKE_CURRENT_BINARY_DIR}/core EXCLUDE_FROM_ALL)

# Изходните файлове на LVGL (без ThorVG, който е изключен в lv_conf.h)
file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS ${LVGL_DIR}/src/*.c)
file(GLOB FONT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fonts/gen/*.c)
set(APP_SOURCES
  src/ui.c
  src/ui_style.c
  src/ui_topbar.c
  src/ui_dro.c
  src/machine.c
  src/program.c
  src/ui_traj.c
  src/raster.c
  src/ui_gcode.c
  src/ui_ctrl.c
  src/ui_dlg.c
  src/ui_jog.c
  src/ui_mdi.c
  src/config.c
  src/machine_tc.c
  src/ui_tools.c
  src/machine_prb.c
  src/machine_hmap.c
  src/ui_probe.c
  src/ui_hmap.c
  src/io.c
  src/ui_diag.c
  src/ui_set.c
  src/ui_files.c
  src/config_defs.c
  src/net_apply.c
  src/update.c
)

# Предупрежденията са грешки само за нашия код; LVGL се компилира с настройките си
# версията (ui/lvgl/VERSION); истинската идва от cnc-update (тагът или VERSION-хеш)
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION CNC_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION)

set(APP_WARNINGS -Wall -Wextra -Wshadow -Wno-unused-parameter)

# Прави библиотека LVGL и програма за една от целите
function(cnc_ui_target name sim main_src)
  add_library(${name}_lvgl STATIC ${LVGL_SOURCES})
  target_include_directories(${name}_lvgl SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}      # lv_conf.h
    ${LVGL_DIR}                      # lvgl.h
    ${LVGL_DIR}/include)
  target_compile_definitions(${name}_lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE CNC_UI_SIM=${sim})
  target_compile_options(${name}_lvgl PRIVATE -w)

  add_executable(${name} ${main_src} ${APP_SOURCES} ${FONT_SOURCES})
  target_include_directories(${name} PRIVATE src)
  target_compile_options(${name} PRIVATE ${APP_WARNINGS})
  target_compile_definitions(${name} PRIVATE CNC_VERSION="${CNC_VERSION}")
  target_link_libraries(${name} PRIVATE ${name}_lvgl cnc-core m)
endfunction()

if(CNC_UI_BUILD_SIM)
  cnc_ui_target(cnc-ui-sim 1 src/main_sim.c)
  target_link_libraries(cnc-ui-sim_lvgl PUBLIC PkgConfig::SDL2)
  message(STATUS "cnc-ui-sim: симулатор със SDL2")
endif()

if(CNC_UI_BUILD_DEVICE)
  cnc_ui_target(cnc-ui 0 "src/main_drm.c;src/drm_disp.c;src/kbd_evdev.c;src/mouse_wheel.c")
  target_link_libraries(cnc-ui_lvgl PUBLIC PkgConfig::DRM PkgConfig::LIBINPUT)
  message(STATUS "cnc-ui: DRM + libinput")
endif()
