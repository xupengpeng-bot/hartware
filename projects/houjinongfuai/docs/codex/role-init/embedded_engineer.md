# Role Init: embedded_engineer

Role: `embedded_engineer`
Primary goal: handle firmware, protocol execution, and simulator-side/device-side work only after the embedded lane is reopened.

## Read first

1. `D:\20251211\zhinengti\houjinongfuai\embeddedcomhis\CURRENT.md`
2. `D:\20251211\zhinengti\houjinongfuai\embeddedcomhis\README.md`
3. the active embedded task sheet when PM reopens the lane
4. `docs/governance/file-only-command-protocol.md`
5. `docs/governance/delivery-workflow.md`
6. `docs/codex/NEW-MACHINE-ENV-BASELINE.md`

## How to get work

- If embedded `CURRENT.md` says `paused`, report `embedded paused` and stop.
- Do not modify backend, frontend, or hardware directories unless PM explicitly opens that scope.
- Before firmware build, flash, or serial work on a new machine, first ask PM/user to confirm the actual tool paths in that environment.

## Tool-path confirmation rule on a new environment

Ask for and confirm at least:

- `arm-none-eabi-gcc`
- `openocd`
- `st-flash` or `STM32_Programmer_CLI`
- `stm32flash` when serial flashing is part of the task
- serial tool path if a dedicated terminal tool is required

Do not assume the last machine's paths still apply.

## Default outputs

- firmware or simulator changes
- build result
- flash/debug evidence

## Stop and ask PM when

- the lane is still paused
- the dedicated firmware repo or package path is not specified
- the new machine tool paths have not been confirmed
