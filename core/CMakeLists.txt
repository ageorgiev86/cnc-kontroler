# Ядро на контролера: интерпретатор и планер на G-code (docs/interpretator-planer.md).
# Без LVGL и без зависимост от интерфейса. Тестове:
#   cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(cnc_core LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(CNC_CORE_SANITIZE "ASan + UBSan (за тестовете)" OFF)
set(CORE_WARNINGS -Wall -Wextra -Wshadow -Wno-unused-parameter)

add_library(cnc-core STATIC
  src/gc_parse.c
  src/gc_interp.c
  src/gc_dump.c
)
target_include_directories(cnc-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(cnc-core PRIVATE ${CORE_WARNINGS})
target_link_libraries(cnc-core PUBLIC m)

if(CNC_CORE_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
  target_compile_options(cnc-core PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

add_executable(gcsim tools/gcsim.c)
target_include_directories(gcsim PRIVATE tools)
target_compile_options(gcsim PRIVATE ${CORE_WARNINGS})
target_link_libraries(gcsim PRIVATE cnc-core)

enable_testing()
add_executable(test_golden tests/test_golden.c)
target_include_directories(test_golden PRIVATE tools)
target_compile_options(test_golden PRIVATE ${CORE_WARNINGS})
target_link_libraries(test_golden PRIVATE cnc-core)
add_test(NAME golden COMMAND test_golden ${CMAKE_CURRENT_SOURCE_DIR}/tests/gcode)

add_executable(test_parse tests/test_parse.c)
target_compile_options(test_parse PRIVATE ${CORE_WARNINGS})
target_link_libraries(test_parse PRIVATE cnc-core)
add_test(NAME parse COMMAND test_parse)
