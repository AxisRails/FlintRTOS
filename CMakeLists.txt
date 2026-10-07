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

# --- PTP slave engine: closed-loop simulation against a model master --------
# Real ptp_core + servo + clock, simulated ptp4l master, drift, delay, jitter.
add_executable(ptp_sim
    tests/ptp_sim.c
    port/ptp/ptp_core.c
    port/ptp/ptp_servo.c
    port/ptp/ptp_clock.c
    port/ptp/ptp_msg.c
    port/ptp/ptp_time.c)
target_compile_definitions(ptp_sim PRIVATE PTP_HOST_TEST)
target_include_directories(ptp_sim PRIVATE
    ${CMAKE_SOURCE_DIR}/port/ptp ${CMAKE_SOURCE_DIR}/third_party/ptpd/src)
add_test(NAME ptp_sim_nominal   COMMAND ptp_sim 37000 48000 8000)
add_test(NAME ptp_sim_fast_osc  COMMAND ptp_sim -120000 30000 20000)
add_test(NAME ptp_sim_ideal     COMMAND ptp_sim 0 100000 0)
add_test(NAME ptp_sim_max_slew  COMMAND ptp_sim 450000 50000 5000)

# --- MQTT device app end-to-end against a real broker (needs mosquitto) -----
# mqtt_host_test : flint_mqtt.c + coreMQTT over POSIX sockets.
# mqtt_lwip_test : flint_mqtt.c + transport_raw.c + coreMQTT + lwIP NO_SYS,
#                  frames over a Linux TAP (needs root + /dev/net/tun).
set(MQTT_DIR ${CMAKE_SOURCE_DIR}/third_party/coreMQTT/source)
set(MQTT_CORE ${MQTT_DIR}/core_mqtt.c ${MQTT_DIR}/core_mqtt_serializer.c ${MQTT_DIR}/core_mqtt_state.c)
set(MQTT_INCS ${CMAKE_SOURCE_DIR}/port/mqtt ${MQTT_DIR}/include ${MQTT_DIR}/interface)
find_program(MOSQUITTO_BIN mosquitto PATHS /usr/sbin /usr/local/sbin)
find_program(MOSQUITTO_SUB mosquitto_sub)
if(MOSQUITTO_BIN AND MOSQUITTO_SUB)
    add_executable(mqtt_host_test tests/mqtt_host_test.c
        port/mqtt/flint_mqtt.c port/mqtt/jsonw.c ${MQTT_CORE})
    target_include_directories(mqtt_host_test PRIVATE ${MQTT_INCS})
    add_test(NAME mqtt_e2e_posix
             COMMAND sh ${CMAKE_SOURCE_DIR}/tests/mqtt_host_test.sh $<TARGET_FILE:mqtt_host_test> posix 18830)

    if(EXISTS /dev/net/tun)
        set(LWIPSRC ${CMAKE_SOURCE_DIR}/third_party/lwip/src)
        file(GLOB LWIP_IPV4 ${LWIPSRC}/core/ipv4/*.c)
        add_executable(mqtt_lwip_test tests/mqtt_lwip_test.c
            port/mqtt/transport_raw.c port/mqtt/flint_mqtt.c port/mqtt/jsonw.c ${MQTT_CORE}
            ${LWIPSRC}/core/init.c ${LWIPSRC}/core/def.c ${LWIPSRC}/core/dns.c
            ${LWIPSRC}/core/inet_chksum.c ${LWIPSRC}/core/ip.c ${LWIPSRC}/core/mem.c
            ${LWIPSRC}/core/memp.c ${LWIPSRC}/core/netif.c ${LWIPSRC}/core/pbuf.c
            ${LWIPSRC}/core/raw.c ${LWIPSRC}/core/stats.c ${LWIPSRC}/core/sys.c
            ${LWIPSRC}/core/tcp.c ${LWIPSRC}/core/tcp_in.c ${LWIPSRC}/core/tcp_out.c
            ${LWIPSRC}/core/timeouts.c ${LWIPSRC}/core/udp.c ${LWIP_IPV4}
            ${LWIPSRC}/netif/ethernet.c)
        target_include_directories(mqtt_lwip_test PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/lwip_host ${LWIPSRC}/include ${MQTT_INCS})
        add_test(NAME mqtt_e2e_lwip_tap
                 COMMAND sh ${CMAKE_SOURCE_DIR}/tests/mqtt_host_test.sh $<TARGET_FILE:mqtt_lwip_test> lwip 18832)
    endif()
endif()
