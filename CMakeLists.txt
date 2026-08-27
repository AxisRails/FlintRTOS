# FlintRTOS - top-level build (host simulation / CI build).
# TCB (kernel) built as C11; tests/tools as C++17. See docs/CODING-STANDARD.md.
cmake_minimum_required(VERSION 3.16)
project(FlintRTOS LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Strict warnings-as-errors approximate MISRA required rules in this environment
# (a licensed MISRA checker is the authority in the production pipeline).
set(FLINT_C_WARNINGS
    -Wall -Wextra -Werror -Wconversion -Wsign-conversion -Wshadow
    -Wundef -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes)
set(FLINT_CXX_WARNINGS
    -Wall -Wextra -Werror -Wconversion -Wsign-conversion -Wshadow -Wundef)

# --- Kernel TCB static library (C) -----------------------------------------
add_library(flint_kernel STATIC
    kernel/msgtag.c
    kernel/prio_bitmap.c
    kernel/cap.c)
target_include_directories(flint_kernel PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_compile_options(flint_kernel PRIVATE ${FLINT_C_WARNINGS})

# --- Classic FreeRTOS-style core, host build for unit tests (C) -------------
# Same sources the RPi4 firmware builds, compiled against the Host portmacro so
# the portable logic can be tested natively.
add_library(flint_core_host STATIC
    kernel/list.c
    kernel/tasks.c
    kernel/queue.c
    portable/MemMang/heap_4.c
    tests/host_port_stubs.c)
target_include_directories(flint_core_host PUBLIC
    ${CMAKE_SOURCE_DIR}/include/FlintRTOS
    ${CMAKE_SOURCE_DIR}/portable/Host
    ${CMAKE_SOURCE_DIR}/tests)
target_compile_options(flint_core_host PRIVATE ${FLINT_C_WARNINGS})

# --- Unit tests (C++), link the C libs -------------------------------------
enable_testing()
add_executable(flint_tests
    tests/test_main.cpp
    tests/test_msgtag.cpp
    tests/test_prio_bitmap.cpp
    tests/test_cap.cpp
    tests/test_list.cpp
    tests/test_heap.cpp
    tests/test_queue.cpp
    tests/test_semaphore.cpp)
target_include_directories(flint_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests)
target_compile_options(flint_tests PRIVATE ${FLINT_CXX_WARNINGS})
target_link_libraries(flint_tests PRIVATE flint_kernel flint_core_host)
add_test(NAME unit COMMAND flint_tests)
