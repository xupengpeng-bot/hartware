cmake_minimum_required(VERSION 3.16)
project(embedded_controller_fw C)

set(CMAKE_C_STANDARD 99)
option(FW_SLIM_OTA_BUILD "Enable safe code-size optimizations for OTA release builds" ON)
option(FW_ENABLE_LTO "Enable link-time optimization for smaller ARM firmware images" ON)

# 交叉工具链下为所有目标统一 Cortex-M3（否则静态库可能用错默认 CPU）
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER MATCHES "arm-none-eabi")
    add_compile_options(
        -mcpu=cortex-m3
        -mthumb
        -ffunction-sections
        -fdata-sections
    )
endif()

# 可选：放置自定义 fw_build_config.h 的目录（优先于 firmware/config/ 下的默认文件）
set(FW_PLATFORM_CONFIG_DIR "" CACHE PATH "Override directory for fw_build_config.h (platform host/port/api path)")

add_library(stm32f1_flash STATIC
    platform/stm32f1_flash.c
)

target_include_directories(stm32f1_flash PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER MATCHES "arm-none-eabi")
    target_compile_options(stm32f1_flash PRIVATE
        -Os
        $<$<BOOL:${FW_ENABLE_LTO}>:-flto>
        -fomit-frame-pointer
        -fno-unwind-tables
        -fno-asynchronous-unwind-tables
    )
endif()

add_library(controller_ota STATIC
    protocol/proto_ota.c
    storage/boot_control.c
    storage/storage_upgrade.c
    workflow/workflow_upgrade_guard.c
)

target_include_directories(controller_ota PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/storage
    ${CMAKE_CURRENT_SOURCE_DIR}/workflow
)

target_link_libraries(controller_ota PRIVATE stm32f1_flash)

add_library(controller_firmware STATIC
    third_party/cjson/cJSON.c
    firmware/protocol/proto_envelope.c
    firmware/protocol/proto_json_builder.c
    firmware/protocol/proto_dispatch.c
    firmware/protocol/proto_codec_json.c
    firmware/protocol/proto_command.c
    firmware/protocol/proto_capability.c
    firmware/protocol/proto_register.c
    firmware/protocol/proto_state_snapshot.c
    firmware/protocol/proto_event_report.c
    firmware/protocol/proto_heartbeat.c
    firmware/protocol/proto_sync_config.c
    firmware/protocol/proto_query.c
    firmware/protocol/proto_execute_action.c
    firmware/storage/storage_config.c
    firmware/storage/config_store.c
    firmware/storage/storage_runtime.c
    firmware/storage/storage_recovery.c
    firmware/common/boot_diag.c
    firmware/common/common_status.c
    firmware/common/common_identity.c
    firmware/common/common_alarm.c
    firmware/runtime/runtime_state.c
    firmware/telemetry/telemetry.c
    firmware/safety/safety_flow.c
    firmware/modules/module_registry.c
    firmware/modules/module_pump_vfd.c
    firmware/modules/module_flow.c
    firmware/modules/module_meter.c
    firmware/modules/module_relay_output.c
    firmware/modules/module_single_valve.c
    firmware/workflow/workflow_engine.c
    firmware/workflow/workflow_control.c
    firmware/workflow/workflow_card_reader.c
    firmware/workflow/workflow_local_access.c
    firmware/workflow/workflow_voice.c
    firmware/workflow/workflow_ready.c
    firmware/workflow/workflow_session.c
    firmware/workflow/workflow_recovery.c
    firmware/bsp/bsp_rtc.c
    firmware/bsp/bsp_uart.c
    firmware/bsp/bsp_voice.c
    firmware/bsp/bsp_flash.c
    firmware/bsp/bsp_gpio.c
    firmware/bsp/bsp_status_led.c
    firmware/bsp/bsp_adc.c
    firmware/bsp/bsp_watchdog.c
    firmware/bsp/bsp_system.c
    firmware/net/net_4g_modem.c
    firmware/net/net_socket_client.c
    firmware/net/net_platform_config.c
    firmware/net/net_connectivity.c
    firmware/port/ota_port_board.c
    firmware/app/app_context.c
    firmware/app/app_health.c
    firmware/app/app_main.c
    firmware/app/app_scheduler.c
)

# ARM + Ninja 增量构建下，版本头变更曾出现对象未重编的情况。
# 这里显式把版本头挂到直接使用它的源文件上，避免生成混合版本 BIN。
set_source_files_properties(
    firmware/common/common_identity.c
    firmware/app/main.c
    PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/firmware/config/scan_trial_defs.h"
)

if(FW_PLATFORM_CONFIG_DIR)
    target_include_directories(controller_firmware BEFORE PUBLIC "${FW_PLATFORM_CONFIG_DIR}")
endif()

target_include_directories(controller_firmware PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cjson
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/model
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/storage
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/common
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/telemetry
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/safety
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/workflow
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/bsp
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/net
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/port
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/app
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/config
)

target_include_directories(controller_firmware PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/workflow
)

target_link_libraries(controller_firmware PUBLIC controller_ota)
target_link_libraries(controller_firmware PRIVATE stm32f1_flash)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER MATCHES "arm-none-eabi")
    target_compile_definitions(controller_firmware PRIVATE BOARD_STM32F103=1)
    if(FW_SLIM_OTA_BUILD)
        target_compile_definitions(controller_firmware PRIVATE
            FW_MODEM_VERBOSE_LOG=0
            FW_OTA_VERBOSE_LOG=0
            FW_PROTO_VERBOSE_LOG=0
        )
    endif()
    target_compile_options(controller_ota PRIVATE
        -Os
        $<$<BOOL:${FW_ENABLE_LTO}>:-flto>
        -fomit-frame-pointer
        -fno-unwind-tables
        -fno-asynchronous-unwind-tables
    )
    target_compile_options(controller_firmware PRIVATE
        -Os
        $<$<BOOL:${FW_ENABLE_LTO}>:-flto>
        -fomit-frame-pointer
        -fno-unwind-tables
        -fno-asynchronous-unwind-tables
    )
    # 未使用 FW_PLATFORM_CONFIG_DIR 时由编译器锁定北向 TCP，与 fw_build_config.h 一致，避免错误头文件覆盖
    if(NOT FW_PLATFORM_CONFIG_DIR)
        target_compile_definitions(controller_firmware PRIVATE
            "FW_PLATFORM_TCP_HOST=\"xupengpeng.top\""
            FW_PLATFORM_TCP_PORT=32563
        )
    endif()
    # 所有 .c 在预处理项目头文件前先包含 stdint.h，避免 uint32_t 未定义
    target_compile_options(controller_ota PRIVATE -include stdint.h)
    target_compile_options(controller_firmware PRIVATE -include stdint.h)
endif()

target_compile_options(controller_ota PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra>
)

target_compile_options(controller_firmware PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra>
)

# 可烧录固件入口：链接生成 ELF；交叉工具链下尽量额外生成 .bin
add_executable(controller_fw firmware/app/main.c)
target_link_libraries(controller_fw PRIVATE controller_firmware)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER MATCHES "arm-none-eabi")
    enable_language(ASM)
    target_sources(controller_fw PRIVATE
        firmware/port/startup_stm32f103rc_app_gcc.S
        firmware/port/system_app.c
        firmware/port/board_clock_stm32f103.c
        firmware/port/fault_handlers.c
    )
    target_compile_definitions(controller_fw PRIVATE BOARD_STM32F103=1)
    target_compile_options(controller_fw PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-include stdint.h>
        -std=gnu99
        -Os
        $<$<BOOL:${FW_ENABLE_LTO}>:-flto>
        -Wall
        -Wno-unused-function
    )
    target_link_options(controller_fw PRIVATE
        -mcpu=cortex-m3
        -mthumb
        -specs=nano.specs
        -specs=nosys.specs
        $<$<BOOL:${FW_ENABLE_LTO}>:-flto>
        "-T${CMAKE_CURRENT_SOURCE_DIR}/firmware/port/stm32f103rc_app.ld"
        -Wl,--gc-sections
        "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/controller_fw.map"
    )
endif()

if(CMAKE_OBJCOPY)
    add_custom_command(TARGET controller_fw POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:controller_fw> $<TARGET_FILE_DIR:controller_fw>/controller_fw.bin
        COMMENT "objcopy -> controller_fw.bin")
endif()

# 独立固件 @ 0x08000000：无需 bootloader，上电即跑 APP（排障 / 未分区烧录用）
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER MATCHES "arm-none-eabi")
    add_executable(controller_fw_standalone firmware/app/main.c)
    target_link_libraries(controller_fw_standalone PRIVATE controller_firmware)
    target_sources(controller_fw_standalone PRIVATE
        firmware/port/startup_stm32f103rc_app_gcc.S
        firmware/port/system_app.c
        firmware/port/board_clock_stm32f103.c
        firmware/port/fault_handlers.c
    )
    target_compile_definitions(controller_fw_standalone PRIVATE APP_FLASH_VECTOR_ADDR=0x08000000U BOARD_STM32F103=1)
    target_compile_options(controller_fw_standalone PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-include stdint.h>
        -std=gnu99
        -Os
        $<$<BOOL:${FW_ENABLE_LTO}>:-flto>
        -Wall
        -Wno-unused-function
    )
    target_link_options(controller_fw_standalone PRIVATE
        -mcpu=cortex-m3
        -mthumb
        -specs=nano.specs
        -specs=nosys.specs
        $<$<BOOL:${FW_ENABLE_LTO}>:-flto>
        "-T${CMAKE_CURRENT_SOURCE_DIR}/firmware/port/stm32f103rc_app_standalone.ld"
        -Wl,--gc-sections
        "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/controller_fw_standalone.map"
    )
    if(CMAKE_OBJCOPY)
        add_custom_command(TARGET controller_fw_standalone POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:controller_fw_standalone>
                $<TARGET_FILE_DIR:controller_fw_standalone>/controller_fw_standalone.bin
            COMMENT "objcopy -> controller_fw_standalone.bin")
    endif()
endif()

add_subdirectory(bootloader)

# 不将 bootloader 设为 controller_fw 的依赖：日常只改 APP 时 `ninja controller_fw` 不会重编 bootloader。
# 需 bootloader 产物时：单独构建 bootloader 目标，或使用 build_flash\build_all.cmd。
