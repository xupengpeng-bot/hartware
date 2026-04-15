# STM32F103: bootloader @ 0x08000000, APP vector @ 0x08004000

if(NOT (CMAKE_C_COMPILER_ID STREQUAL "GNU" AND "${CMAKE_C_COMPILER}" MATCHES "arm-none-eabi"))
    message(STATUS "bootloader: skipped (needs arm-none-eabi-gcc)")
else()
    enable_language(ASM)

    add_executable(bootloader
        main.c
        ${CMAKE_SOURCE_DIR}/platform/stm32f1_flash.c
        ${CMAKE_SOURCE_DIR}/storage/boot_control.c
        system_boot.c
        startup_stm32f103rc_bootloader_gcc.S
    )

    target_include_directories(bootloader PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_compile_options(bootloader PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-include stdint.h>
        -mcpu=cortex-m3
        -mthumb
        -std=gnu99
        -Os
        -ffunction-sections
        -fdata-sections
        -Wall
        -Wno-unused-function
    )

    target_link_options(bootloader PRIVATE
        -mcpu=cortex-m3
        -mthumb
        -specs=nano.specs
        -specs=nosys.specs
        "-T${CMAKE_CURRENT_SOURCE_DIR}/stm32f103rc_bootloader.ld"
        -Wl,--gc-sections
        "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/bootloader.map"
    )

    set_target_properties(bootloader PROPERTIES SUFFIX ".elf")

    if(CMAKE_OBJCOPY)
        add_custom_command(TARGET bootloader POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:bootloader> $<TARGET_FILE_DIR:bootloader>/bootloader.hex
            COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:bootloader> $<TARGET_FILE_DIR:bootloader>/bootloader.bin
            COMMENT "objcopy bootloader -> hex/bin")
        add_custom_command(TARGET bootloader POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE_DIR:bootloader>/bootloader.bin
                ${CMAKE_BINARY_DIR}/bootloader.bin
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE_DIR:bootloader>/bootloader.hex
                ${CMAKE_BINARY_DIR}/bootloader.hex
            COMMENT "bootloader: copy .bin/.hex to build root")
    endif()
endif()
