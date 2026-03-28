# 新机器环境基线

适用范围：

- 当前仓库：`houjinongfuai`
- 当前阶段：`Phase 1`
- 默认操作系统：`Windows 10/11`
- 默认 shell：`Windows PowerShell 5.1`

本文目标是让新机器上的 Codex / Cursor / 开发人员在不补猜、不临时找工具的情况下，直接拉起当前后端仓库，并为后续前端联调、嵌入式编译、串口调试、烧录调试预留好基础环境。

## 1. 推荐目录布局

推荐目录：

```text
D:\20251211\zhinengti\houjinongfuai
D:\20251211\zhinengti\lovable
```

如果不使用这个固定目录，也至少保持以下相对关系：

- 后端仓库：当前工作区根目录
- 前端仓库：后端仓库同级目录 `..\lovable`

## 2. 必装工具

这些是当前仓库已经实测用到，或者启动脚本直接依赖的工具。

### 2.1 Git

- 作用：拉代码、同步主线、提交代码
- 当前机器实测版本：`git version 2.53.0.windows.1`
- 命令检查：

```powershell
git --version
```

### 2.2 Node.js + npm

- 作用：后端 NestJS、前端 Vite、测试与脚本执行
- 当前机器实测版本：
  - `node v24.14.0`
  - `npm 11.9.0`
- 命令检查：

```powershell
node -v
npm -v
```

说明：

- 当前机器使用 `nvm-windows`
- 相关环境变量：
  - `NVM_HOME`
  - `NVM_SYMLINK`

如果新机器不使用 `nvm-windows`，也可以直接安装固定版本 Node，但建议保持同一大版本。

### 2.3 Docker Desktop

- 作用：本地 PostgreSQL 容器
- 当前机器实测版本：
  - `Docker version 29.2.1`
  - `Docker Compose version v5.0.2`
- 命令检查：

```powershell
docker --version
docker compose version
```

### 2.4 Python

- 作用：仓库内已有 Python 维护脚本，当前不是摆设
- 当前机器实测版本：
  - `Python 3.13.7`
  - `pip 26.0.1`
- 命令检查：

```powershell
python --version
py --version
pip --version
```

当前仓库内已存在的 Python 脚本：

- `backend/scripts/dispatch_db_probe.py`
- `backend/scripts/dispatch_utf8_cleanup.py`
- `backend/scripts/dispatch_utf8_scan.py`

当前机器已验证 Python 模块：

- `pymysql`

建议新机器执行：

```powershell
python -m pip install pymysql
```

### 2.5 PowerShell

- 作用：仓库启动脚本、迁移脚本、seed 脚本
- 当前机器实测版本：`5.1.26100.7920`
- 命令检查：

```powershell
$PSVersionTable.PSVersion
```

说明：

- 当前仓库脚本直接按 Windows PowerShell 运行，没有要求 `pwsh`
- 新机器至少保证 `powershell.exe` 可用

### 2.6 Windows 长路径支持

- 作用：降低 Git checkout、依赖安装、深层目录读写失败的返工风险
- 当前机器实测：
  - `LongPathsEnabled = 1`
- 命令检查：

```powershell
Get-ItemProperty -Path 'HKLM:\\SYSTEM\\CurrentControlSet\\Control\\FileSystem' -Name LongPathsEnabled
```

建议：

- 新机器保持系统级长路径支持开启
- 同时在活跃仓库执行：

```powershell
git config core.longpaths true
```

## 3. 当前仓库启动所需本地配置

### 3.1 后端 `.env`

后端目录存在：

- `backend/.env.example`
- `backend/.env`

新机器首次启动可直接：

```powershell
cd backend
Copy-Item .env.example .env
```

当前后端最关键的配置项：

- `NODE_ENV`
- `PORT`
- `POSTGRES_HOST`
- `POSTGRES_PORT`
- `POSTGRES_DB`
- `POSTGRES_USER`
- `POSTGRES_PASSWORD`
- `DATABASE_URL`

可选的 dispatch 镜像配置：

- `DISPATCH_DB_ENABLED`
- `DISPATCH_DB_HOST`
- `DISPATCH_DB_PORT`
- `DISPATCH_DB_NAME`
- `DISPATCH_DB_USER`
- `DISPATCH_DB_PASSWORD`
- `DISPATCH_DB_WRITE_ENABLED`
- `DISPATCH_WRITE_KEY`

### 3.2 启动命令

后端：

```powershell
.\start-backend.ps1
```

前端：

```powershell
.\start-frontend.ps1
```

如果前端不在同级 `..\lovable`，显式传参：

```powershell
.\start-frontend.ps1 -FrontendDir <绝对路径>
```

### 3.3 后端基础验证

```powershell
cd backend
npm install
npm run db:up
npm run db:migrate
npm run build
npm run test:unit
```

## 4. Codex / Cursor 本机工具

当前机器实测：

- Cursor 可执行命令：`cursor`
- Cursor 版本：`2.6.21`
- VS Code 可执行命令：`code`
- VS Code 版本：`1.112.0`
- Codex Desktop 已安装，并把可执行目录放入了 `Path`

命令检查：

```powershell
cursor --version
code --version
```

说明：

- 对当前仓库开发来说，`Cursor` 和 `Codex Desktop` 不是“代码运行依赖”，但属于“协作和执行依赖”
- 新机器若需要继续沿用现有协作方式，应同时安装：
  - Codex Desktop
  - Cursor

## 5. 嵌入式构建与调试基线

### 5.1 ARM GCC

当前机器已实测可用：

- 可执行路径：
  - `D:\Program Files (x86)\GNU Arm Embedded Toolchain\10 2021.10\bin\arm-none-eabi-gcc.exe`
- 版本：
  - `GNU Arm Embedded Toolchain 10.3-2021.10`
- 发现方式：
  - `Get-Command`
  - `PATH`

命令检查：

```powershell
arm-none-eabi-gcc --version
```

建议：

- 新机器安装 ARM GCC，并保证 `arm-none-eabi-gcc` 可直接从命令行调用
- 优先把工具链加进 `PATH`

### 5.2 OpenOCD

当前机器已实测可用：

- 版本：`xPack Open On-Chip Debugger 0.12.0`
- 当前来自 WinGet 安装目录

命令检查：

```powershell
openocd --version
```

### 5.3 ST-LINK 工具

当前机器实际存在以下可执行文件：

- `D:\20251211\智能体\skills\stlink-1.8.0-win32\bin\st-flash.exe`
- `D:\20251211\智能体\skills\stlink-1.8.0-win32\bin\st-info.exe`

当前机器实测版本：

- `st-flash v1.7.0`
- `st-info v1.7.0`

说明：

- 文件存在，但当前 shell 没有稳定把它们解析成全局命令
- 新机器不要照搬这种“文件在，但 PATH 不稳定”的状态
- 更稳的做法：
  - 直接安装官方 ST-LINK / STM32 工具
  - 或把这两个目录放到一个纯 ASCII 路径下，例如 `D:\tools\stlink\bin`
  - 然后显式加入 `PATH`

### 5.4 STM32 串口烧录工具

当前机器实际存在：

- `D:\20251211\智能体\skills\stm32flash-0.7-binaries\stm32flash.exe`

说明：

- 该工具可直接按绝对路径执行
- 但当前 shell 里同样没有稳定解析成全局命令
- 新机器建议统一改成 ASCII 路径，例如：
  - `D:\tools\stm32flash\stm32flash.exe`

命令示例：

```powershell
stm32flash -h
```

### 5.5 建议预装但当前机器未发现的工具

以下是基于本地代码和旁路工程痕迹做出的推断，属于强建议，不是当前仓库已直接调用的硬依赖：

- `STM32CubeProgrammer`
  - 原因：后续 STM32 烧录和量产会更稳
- `SEGGER J-Link`
  - 原因：`D:\20251211\智能体` 下可见多个 `JLinkSettings.ini`
- `Keil MDK`
  - 原因：邻近固件工程里存在大量 `*.uvprojx` / `*.uvoptx`

这三项当前机器未发现已安装的正式程序入口，但从现有工程痕迹看，未来调试/烧录大概率会用到。

## 6. 串口调试基线

当前机器未检测到在线串口设备：

- `Get-CimInstance Win32_SerialPort` 无返回

这说明当前扫描时没有插入串口设备，不能说明机器缺少驱动。

新机器建议准备：

- CH340 驱动
- CP210x 驱动
- FTDI 驱动

建议至少准备一个串口调试工具：

- PuTTY
- Tera Term
- SSCOM

当前机器未发现这些串口调试工具已安装，所以这是建议补齐项。

## 7. Git 与大文件

当前机器实测：

- `git-lfs/3.7.1`

命令检查：

```powershell
git lfs version
```

虽然当前仓库不一定每次都依赖 LFS，但新机器预装 `git-lfs` 更稳。

## 8. 建议纳入 PATH 的目录

建议新机器至少保证以下命令能直接运行：

- `git`
- `node`
- `npm`
- `python`
- `py`
- `pip`
- `docker`
- `arm-none-eabi-gcc`
- `openocd`
- `cursor`
- `code`

如果后续有嵌入式实机调试，再补：

- `st-flash`
- `st-info`
- `stm32flash`
- `STM32_Programmer_CLI`
- `JLink`

## 9. 敏感环境变量管理

当前机器进程环境里能看到外部 AI 平台相关密钥变量。这类变量不要写入仓库，也不要固化进公共文档。

建议规则：

- API key 只放用户级环境变量或密码管理器
- 项目级配置只放在本机 `.env`
- 严禁把真实密钥提交进仓库

## 10. 新机器验收清单

新机器装完后，至少跑通下面这组检查：

```powershell
git --version
node -v
npm -v
python --version
docker --version
docker compose version
arm-none-eabi-gcc --version
openocd --version
cursor --version
code --version
git lfs version
```

然后进入后端仓库执行：

```powershell
cd backend
Copy-Item .env.example .env
npm install
npm run db:up
npm run db:migrate
npm run build
npm run test:unit
```

再进入开发体系仓库执行：

```powershell
cd D:\20251211\zhinengti\development-system\projects\houjinongfuai
.\tools\preflight.ps1
```

通过后先读取：

- `PROJECT-CONFIG-REGISTRY.md`

再开始读取 `CURRENT.md` 和 active task。

如果要进入嵌入式或硬件链路，在新机器上不要直接假设工具路径。

必须先向 PM / 用户确认实际工具路径后再继续，至少确认：

- `arm-none-eabi-gcc`
- `openocd`
- `st-flash` 或 `STM32_Programmer_CLI`
- `stm32flash`（如果任务涉及串口烧录）
- 串口调试工具路径（如果任务需要串口终端）

如果需要前端联调，再确认：

```powershell
cd ..\lovable
npm install
npm run dev
```

## 11. 当前机器实测结论

已确认可用：

- Git
- Node / npm
- Python / pip
- Docker / docker compose
- PowerShell
- ARM GCC
- OpenOCD
- Cursor
- VS Code
- git-lfs

已确认存在但未形成稳定全局命令：

- `st-flash`
- `st-info`
- `stm32flash`

当前机器未发现正式安装入口，但建议未来补齐：

- `STM32CubeProgrammer`
- `SEGGER J-Link`
- `Keil MDK`
- 串口调试工具（PuTTY / Tera Term / SSCOM 任一）

## 12. 编码与文档可读性补充

新机器不要忽略编码问题。当前项目的 Windows-first 协作里，编码错误会直接影响入口文档读取和任务理解。

规则：

- 中文或其他非 ASCII 的开发体系文档，优先使用 `UTF-8 with BOM`
- 不要把带中文的 tracked 文本文件保存成 ANSI / GBK
- 不要对整个代码库做无差别批量转码
- 开工前用 `.\tools\preflight.ps1` 检查关键入口文件是否存在乱码或替代字符

## 13. Windows 路径补充

为降低 Windows 平台返工，建议长期保持：

- 工作区根目录尽量短，例如 `D:\20251211\zhinengti`
- 开发体系仓库与业务仓库平级，不继续深层嵌套
- 新任务文件名保持清晰但不要过长
- Git 仓库启用 `core.longpaths`
- 开工前运行 `.\tools\preflight.ps1`
