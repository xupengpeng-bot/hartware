# 云端开发流程

适用范围：

- 当前仓库：`houjinongfuai`
- 当前阶段：`Phase 1`
- 协作对象：Codex、Cursor、软件工程师、PM

本文目标不是讨论“能不能上云”，而是定义一套对当前项目可执行的云端开发流程，让云端开发和本地联调各司其职，避免来回切换时丢上下文、丢环境、丢边界。

## 1. 结论先行

对当前项目，最优解不是“全本地”或“全云端”，而是：

- 云端优先承担：文档、后端、前端、测试、迁移、seed、任务协作
- 本地承担：浏览器联调、数据库本机验证、嵌入式编译、串口调试、烧录、实机联调

一句话规则：

- 纯代码和纯文档工作，优先云端
- 涉及 USB、串口、板卡、烧录器、OpenOCD、J-Link、ST-Link，切回本地

## 2. 云端与本地的职责边界

### 2.1 适合放云端的工作

- 读取 `CURRENT.md`、`RESULT.md`、治理文档、任务卡
- 后端 NestJS 开发
- SQL migration / seed 编写与整理
- 单元测试
- 大多数 e2e
- 前端普通页面开发
- DTO / contract / response shell 对齐
- 文档整理、任务拆解、交付说明
- Git 同步、review、提交

### 2.2 必须回本地的工作

- 串口调试
- ST-Link / J-Link / OpenOCD 实机调试
- STM32 烧录
- ARM GCC + 板卡联调
- 现场设备网络、485、Modbus、串口桥联调
- 本地浏览器与本地服务联调
- 任何需要直接接触 USB 设备的动作

### 2.3 不要在云端做的事

- 假设能直接访问本地串口或烧录器
- 假设云端能替代板卡现场验证
- 在没有明确边界时，把硬件问题当普通后端问题处理

## 3. 云端工作区标准结构

推荐目录关系：

```text
<workspace-root>/houjinongfuai
<workspace-root>/lovable
```

当前项目约定：

- 后端仓库：当前工作区根目录
- 前端仓库：同级目录 `../lovable`

如果云端平台只能开一个仓库工作区：

- 后端仓库仍作为主工作区
- 前端联调任务只有在你明确把 `lovable` 同步到同级目录时才执行

## 4. 云端环境最小集

参考：

- `docs/codex/NEW-MACHINE-ENV-BASELINE.md`

云端最小必备：

- `git`
- `node`
- `npm`
- `docker`
- `powershell` 或等效 shell
- 可运行当前后端的 PostgreSQL 容器环境

云端推荐但非刚需：

- `python`
- `pip`
- `git-lfs`

云端通常不要求：

- 串口驱动
- ST-Link
- J-Link
- STM32CubeProgrammer
- `stm32flash`

这些保留给本地。

## 5. 云端开工标准流程

### 5.1 第一步：同步代码

进入后端仓库：

```powershell
git fetch --all --prune
git checkout main
git pull --ff-only origin main
git status --short
git rev-parse HEAD
```

如果任务涉及前端联调，再进入前端仓库：

```powershell
git fetch --all --prune
git checkout main
git pull --ff-only origin main
git status --short
git rev-parse HEAD
git rev-parse origin/main
```

### 5.2 第二步：读取项目事实来源

云端 Codex / Cursor 不要凭聊天猜任务，必须按顺序读取：

1. `AGENTS.md`
2. `docs/codex/README.md`
3. `docs/codex/CURRENT.md`
4. `docs/codex/WORK-MODES.md`
5. `docs/codex/TASK-TYPES.md`
6. `docs/governance/file-only-command-protocol.md`
7. `docs/governance/requirements-to-tasks-rule.md`
8. `docs/governance/definition-of-ready.md`
9. `docs/governance/delivery-workflow.md`
10. `docs/governance/current-wave-2026-03-24.md`
11. `CURRENT.md` 指向的 active task
12. `docs/codex/RESULT.md`

### 5.3 第三步：判断当前任务是否允许执行

如果 `docs/codex/CURRENT.md` 中：

- `active task: none`
- 或 `mode: IDLE`

则云端应停止，并返回：

- `no active task`

不要自行扩范围开发。

## 6. 云端启动与验证流程

### 6.1 后端

```powershell
cd backend
Copy-Item .env.example .env
npm install
npm run db:up
npm run db:migrate
npm run build
npm run test:unit
```

如果需要完整验证，再跑：

```powershell
npm run db:seed:test
npm run test:e2e
```

### 6.2 前端

只有满足以下条件再在云端跑前端：

- 已明确任务涉及前端
- `../lovable` 存在
- 云端环境允许本地端口暴露或预览

命令：

```powershell
cd ../lovable
npm install
npm run dev
```

### 6.3 云端验证优先级

推荐顺序：

1. `build`
2. `unit`
3. `e2e`
4. 需要时再前后端联调

不要一上来就把问题推给前端或硬件，先在云端把可复现的后端问题清掉。

## 7. 云端开发的标准工作方式

### 7.1 云端工作模式

默认做法：

- 在云端先完成代码编写
- 在云端先完成 migration / seed / contract 对齐
- 在云端先完成 build / unit / e2e
- 通过后再决定是否需要本地接管

### 7.2 云端提交门槛

至少满足：

- `npm run build` 通过
- `npm run test:unit` 通过

推荐满足：

- `npm run test:e2e` 通过

如果没过：

- 明确列出失败项
- 明确说明是代码问题、seed 问题、环境问题，还是必须转本地的硬件问题

### 7.3 云端提交范围

可以提交：

- 代码
- migration
- seed
- 测试
- 文档
- 启动脚本

不要提交：

- 本地日志
- `node_modules`
- `dist`
- `.env`
- `.web-test`
- 临时抓包、临时 shell 输出

## 8. 从云端切回本地的标准条件

满足任一条件，就应该切回本地：

- 需要串口
- 需要烧录
- 需要板卡实机
- 需要 OpenOCD / J-Link / ST-Link
- 需要核对 USB 驱动
- 需要浏览器 + 本地服务联调
- 云端端口、代理或权限限制影响验证
- 问题只在本机硬件链路中出现

不要在云端硬扛这类问题。

## 9. 云端到本地的交接格式

当云端完成一轮工作，需要本地接手时，交接必须写清楚：

- task id
- mode
- 当前分支 / commit SHA
- 已验证项
- 未验证项
- 本地必须执行的动作
- 风险点
- 下一接手角色

建议格式：

```text
task id: ...
mode: ...
status: ...
verified:
- build
- unit
- e2e
local-only next step:
- flash STM32 with ...
- verify COM port ...
pending issues:
- ...
next handoff target:
- local engineer / embedded engineer / hardware engineer
```

## 10. Codex / Cursor 的云端初始化口令

### 10.1 Codex

```text
这是一个文件驱动项目。先不要直接写代码。

请先执行：
1. 同步当前后端仓库到 origin/main 最新
2. 如果任务涉及前端，再同步同级 ../lovable 到 origin/main 最新
3. 按顺序读取：
   - AGENTS.md
   - docs/codex/README.md
   - docs/codex/CURRENT.md
   - docs/codex/WORK-MODES.md
   - docs/codex/TASK-TYPES.md
   - docs/governance/file-only-command-protocol.md
   - docs/governance/requirements-to-tasks-rule.md
   - docs/governance/definition-of-ready.md
   - docs/governance/delivery-workflow.md
   - docs/governance/current-wave-2026-03-24.md
   - CURRENT.md 指向的 active task
   - docs/codex/RESULT.md
4. 如果 active task = none，停止并报告 no active task
5. 如果有 active task，只执行该任务，不扩范围
6. 云端只负责代码、文档、测试；如涉及串口、烧录、板卡、OpenOCD、J-Link、ST-Link，明确报告 must switch to local
```

### 10.2 Cursor

```text
先不要开始编码。先同步仓库，再按项目文件执行。

步骤：
1. 同步当前仓库到 origin/main
2. 如果任务涉及前端，再同步同级 ../lovable
3. 严格读取 AGENTS.md、CURRENT.md、WORK-MODES.md、治理文档和 active task
4. 如果 active task = none，直接报告 no active task
5. 云端只做代码、文档、测试；涉及串口、烧录、板卡实机联调时停止并标记 must switch to local
```

## 11. 推荐的混合开发节奏

一轮完整交付建议这样走：

1. 云端同步主线并读取任务
2. 云端完成代码、migration、seed、测试
3. 云端提交代码并写清验证结果
4. 如果需要设备或串口，切回本地联调
5. 本地完成实机验证后，再补最终验证说明

这样能把云端的环境一致性和本地的硬件可达性都用上。

## 12. 当前项目的最终建议

对当前项目，建议长期采用下面这条规则：

- 云端优先开发
- 本地优先联调
- 文档永远作为唯一事实来源

不要把“云端能写代码”误判成“云端能代替本地硬件验证”。

对你们现在的阶段，最稳的模式是：

- 日常研发：云端
- 联调验收：本地
- 嵌入式 / 硬件：本地
