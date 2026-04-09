@echo off
rem If you see "'@echo' is not recognized": file may have UTF-8 BOM. Run: python "%~dp0tools\strip_utf8_bom.py"

chcp 65001 >nul

setlocal EnableDelayedExpansion



rem 涓?code 骞崇骇锛氭湰鐩綍 ..\code 涓哄浐浠跺伐绋嬫牴鐩綍

set "SCRIPT_DIR=%~dp0"

set "CODE_DIR=%SCRIPT_DIR%..\code"

rem CMake build dir defaults to ASCII-only path. Override with BUILD_FLASH_OUT if needed.

rem Example: set BUILD_FLASH_OUT=D:\your\ascii\path\out\build

if defined BUILD_FLASH_OUT (

    set "BUILD_DIR=!BUILD_FLASH_OUT!"

) else (

    set "BUILD_DIR=%LOCALAPPDATA%\hw_embedded_build\out\build"

)



if not exist "%CODE_DIR%\CMakeLists.txt" (

    echo [閿欒] 鏈壘鍒?CMakeLists.txt: "%CODE_DIR%\CMakeLists.txt"

    echo 璇风‘璁?build_flash 涓?code 鍦ㄥ悓涓€鐖剁洰褰曚笅銆?

    exit /b 1

)



rem 鏃犲叏灞€ Ninja 鏃惰嚜鍔ㄤ笅杞藉埌 tools\ninja.exe锛堜粎闇€ PowerShell 涓庣綉缁滐級

if not exist "%SCRIPT_DIR%tools\ninja.exe" (

    echo [淇℃伅] 鏈壘鍒?tools\ninja.exe锛屾鍦ㄤ笅杞?Ninja...

    powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%tools\ensure_ninja.ps1"

    if errorlevel 1 (

        echo [閿欒] 鏃犳硶涓嬭浇 Ninja銆傝妫€鏌ョ綉缁滄垨鎵嬪姩灏?ninja.exe 鏀惧叆 build_flash\tools\

        exit /b 1

    )

)



call "%SCRIPT_DIR%env_tools.cmd"



rem 鏈缃?CMAKE_GENERATOR 鏃讹細浼樺厛 Ninja锛岄伩鍏?CMake 榛樿 NMake 鑰屾湰鏈烘棤 nmake

rem 锛坋nv_tools 宸叉妸甯歌 ninja 鐩綍鍔犲叆 PATH锛涙澶勫啀鎸夎矾寰勬帰娴嬩竴娆★級

if not defined CMAKE_GENERATOR (

    if exist "%SCRIPT_DIR%tools\ninja.exe" set "CMAKE_GENERATOR=Ninja"

)

if not defined CMAKE_GENERATOR (

    where ninja >nul 2>&1 && set "CMAKE_GENERATOR=Ninja"

)

if not defined CMAKE_GENERATOR (

    if exist "%LOCALAPPDATA%\Microsoft\WinGet\Links\ninja.exe" set "CMAKE_GENERATOR=Ninja"

)

if not defined CMAKE_GENERATOR (

    if exist "D:\Tools\ninja\ninja.exe" set "CMAKE_GENERATOR=Ninja"

)

if not defined CMAKE_GENERATOR (

    if exist "%ProgramFiles%\Ninja\ninja.exe" set "CMAKE_GENERATOR=Ninja"

)

if not defined CMAKE_GENERATOR (

    if exist "%ProgramData%\chocolatey\bin\ninja.exe" set "CMAKE_GENERATOR=Ninja"

)

if not defined CMAKE_GENERATOR (

    if exist "%USERPROFILE%\scoop\shims\ninja.exe" set "CMAKE_GENERATOR=Ninja"

)

if not defined CMAKE_GENERATOR (

    where nmake >nul 2>&1

    if errorlevel 1 (

        rem 浠ヤ笅鎻愮ず浣跨敤绾?ASCII锛岄伩鍏嶅紩鍙枫€佸啋鍙枫€佽灏惧弽鏂滄潬涓庣紪鐮佸鑷?cmd 璇В鏋?

        echo [ERROR] CMake on Windows needs ninja.exe OR nmake.exe. Neither was found.

        echo [HINT] winget install Ninja-build.Ninja

        echo [HINT] Or unzip ninja.exe to D:\Tools\ninja and add that folder to PATH.

        echo [HINT] Or install Visual Studio with Desktop development with C++ ^(includes nmake^).

        exit /b 1

    )

)



if not defined CMAKE_EXE (

    echo [閿欒] 鏈壘鍒?cmake.exe

    echo [鎻愮ず] 宸叉悳绱?PATH 涓?env_tools.cmd 涓殑甯歌鐩綍 ^(鍚?D:/Tools/CMake/bin^)

    echo [鎻愮ず] 璇峰畨瑁?CMake 鎴栧皢鍏?bin 鍔犲叆 PATH

    exit /b 1

)



echo [淇℃伅] CMake: !CMAKE_EXE!



if not defined CMAKE_TOOLCHAIN_FILE (

    if defined ARM_GCC_BIN (

        set "CMAKE_TOOLCHAIN_FILE=%SCRIPT_DIR%toolchain-arm-none-eabi.cmake"

        rem 璺緞鍚?Program Files (x86) 鏃讹紝鍕跨敤 %%ARM_GCC_BIN%%锛堟嫭鍙蜂細鐮村潖 if 鍧楄В鏋愶級

        echo [淇℃伅] 鑷姩浣跨敤 ARM 宸ュ叿閾剧洰褰? !ARM_GCC_BIN!

        echo [淇℃伅] 宸ュ叿閾炬枃浠? !CMAKE_TOOLCHAIN_FILE!

    ) else (

        echo [鎻愮ず] 鏈娴嬪埌 arm-none-eabi-gcc銆傚皢浣跨敤鏈満榛樿 C 缂栬瘧鍣紙澶氫负 MSVC锛夈€?

        echo       鑻ラ渶浜ゅ弶缂栬瘧锛岃瀹夎 GNU Arm 鎴栬缃?ARM_GCC_BIN / CMAKE_TOOLCHAIN_FILE銆?

        echo       甯歌璺緞: D:/Tools/GNUArmEmbeddedToolchain/bin

    )

)



echo [淇℃伅] 婧愮爜鐩綍: %CODE_DIR%

echo [淇℃伅] 鏋勫缓鐩綍: %BUILD_DIR%



if defined CMAKE_GENERATOR (

    echo [淇℃伅] 浣跨敤鐢熸垚鍣? !CMAKE_GENERATOR!

) else (

    echo [淇℃伅] 浣跨敤 CMake 榛樿鐢熸垚鍣紙nmake 鍙敤锛涘彲璁剧疆 CMAKE_GENERATOR 瑕嗙洊锛屼緥濡?Ninja锛?

)



rem 涓婃鐢?NMake 绛夊叾瀹冪敓鎴愬櫒閰嶇疆杩囷紝涓庢湰娆?-G 鍐茬獊鏃舵竻鐞嗙紦瀛橈紙鍚﹀垯浼氭姤 generator mismatch锛?

if defined CMAKE_GENERATOR if exist "%BUILD_DIR%\CMakeCache.txt" (

    findstr /C:"CMAKE_GENERATOR:INTERNAL=!CMAKE_GENERATOR!" "%BUILD_DIR%\CMakeCache.txt" >nul 2>&1

    if errorlevel 1 (

        echo [淇℃伅] CMake 缂撳瓨鐢熸垚鍣ㄤ笌褰撳墠 -G !CMAKE_GENERATOR! 涓嶄竴鑷达紝姝ｅ湪娓呯悊 out\build ...

        rmdir /s /q "%BUILD_DIR%"

    )

)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"



if defined CMAKE_TOOLCHAIN_FILE (

    if defined CMAKE_GENERATOR (

        cmake -S "%CODE_DIR%" -B "%BUILD_DIR%" -G "!CMAKE_GENERATOR!" -DCMAKE_TOOLCHAIN_FILE="%CMAKE_TOOLCHAIN_FILE%" -DCMAKE_BUILD_TYPE=Release

    ) else (

        cmake -S "%CODE_DIR%" -B "%BUILD_DIR%" -DCMAKE_TOOLCHAIN_FILE="%CMAKE_TOOLCHAIN_FILE%" -DCMAKE_BUILD_TYPE=Release

    )

) else (

    if defined CMAKE_GENERATOR (

        cmake -S "%CODE_DIR%" -B "%BUILD_DIR%" -G "!CMAKE_GENERATOR!" -DCMAKE_BUILD_TYPE=Release

    ) else (

        cmake -S "%CODE_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release

    )

)

if errorlevel 1 (

    echo [閿欒] CMake 閰嶇疆澶辫触銆備氦鍙夌紪璇戞椂璇风‘璁?ARM_GCC_BIN 鎴?toolchain-arm-none-eabi.cmake 涓殑璺緞銆?

    exit /b 1

)



rem ARM 浜ゅ弶缂栬瘧锛氶粯璁ゅ彧缂?APP锛坈ontroller_fw锛夈€俠ootloader 鏈敼鏃朵笉蹇呴噸缂栵紱闇€涓よ€呮椂鐢?build_all.cmd

if defined CMAKE_TOOLCHAIN_FILE (

    echo ????: set SERIAL_LOG_PORT=COM4 ^&^& build_and_flash.cmd

echo [淇℃伅] 鏋勫缓 controller_fw + controller_fw_standalone ^(鏃?bootloader 鎺掗殰鐢?standalone 鐑у埌 0x08000000^)...

    cmake --build "%BUILD_DIR%" --config Release --target controller_fw

    cmake --build "%BUILD_DIR%" --config Release --target controller_fw_standalone

) else (

    cmake --build "%BUILD_DIR%" --config Release

)

if errorlevel 1 (

    echo [閿欒] 缂栬瘧澶辫触銆?

    exit /b 1

)



echo.

echo [瀹屾垚] 浜х墿鐩綍: %BUILD_DIR%

if exist "%BUILD_DIR%\bootloader.bin" echo        - bootloader.bin / bootloader.hex ^(鏋勫缓鏍圭洰褰曪紝渚夸簬鐑у綍^)

if exist "%BUILD_DIR%\bootloader\bootloader.bin" echo        - bootloader\bootloader.bin / .hex

if exist "%BUILD_DIR%\Release\bootloader\bootloader.bin" echo        - Release\bootloader\bootloader.bin

if exist "%BUILD_DIR%\controller_fw.elf" echo        - controller_fw.elf

if exist "%BUILD_DIR%\controller_fw.bin" echo        - controller_fw.bin

if exist "%BUILD_DIR%\controller_fw_standalone.bin" echo        - controller_fw_standalone.bin ^(鐑у 0x08000000锛屾棤闇€ bootloader^)

if exist "%BUILD_DIR%\Release\controller_fw.exe" echo - Release\controller_fw.exe ^(鏈満浠跨湡^)

echo.

echo 鐑у綍: flash.cmd ^(0x08010000^) / flash_standalone.cmd ^(0x08000000^) / flash_full.cmd ^(bootloader+APP^)

echo 渚濊禆妫€鏌? deps_check.cmd

echo.

echo [鎻愮ず] 鑻ラ渶鍚屾椂缂栬瘧 bootloader + APP锛歜uild_all.cmd

exit /b 0



