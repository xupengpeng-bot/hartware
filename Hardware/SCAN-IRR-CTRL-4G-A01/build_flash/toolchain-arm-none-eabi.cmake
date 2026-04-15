# 与 build.cmd / env_tools.cmd 配合：优先环境变量 ARM_GCC_BIN
# 查找顺序对齐 3.0jijingoldplatform\tools\build_gcc.ps1

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

set(TOOLCHAIN_PREFIX arm-none-eabi-)

if(DEFINED ENV{ARM_GCC_BIN})
  file(TO_CMAKE_PATH "$ENV{ARM_GCC_BIN}" _bin)
  set(TOOLCHAIN_BIN_DIR "${_bin}")
elseif(EXISTS "D:/Tools/GNUArmEmbeddedToolchain/bin/arm-none-eabi-gcc.exe")
  set(TOOLCHAIN_BIN_DIR "D:/Tools/GNUArmEmbeddedToolchain/bin")
elseif(EXISTS "C:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2021.10/bin/arm-none-eabi-gcc.exe")
  set(TOOLCHAIN_BIN_DIR "C:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2021.10/bin")
elseif(EXISTS "D:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2021.10/bin/arm-none-eabi-gcc.exe")
  set(TOOLCHAIN_BIN_DIR "D:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2021.10/bin")
else()
  message(FATAL_ERROR "arm-none-eabi-gcc not found. Set ARM_GCC_BIN or install GNU Arm Embedded Toolchain.")
endif()

set(CMAKE_C_COMPILER "${TOOLCHAIN_BIN_DIR}/${TOOLCHAIN_PREFIX}gcc.exe")
set(CMAKE_ASM_COMPILER "${CMAKE_C_COMPILER}")
set(CMAKE_OBJCOPY "${TOOLCHAIN_BIN_DIR}/${TOOLCHAIN_PREFIX}objcopy.exe")
set(CMAKE_SIZE "${TOOLCHAIN_BIN_DIR}/${TOOLCHAIN_PREFIX}size.exe")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
