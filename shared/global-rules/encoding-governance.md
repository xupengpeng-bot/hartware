# Encoding Governance

Status: active
Audience: PM, engineers, AI agents
Purpose: keep cross-device text, Chinese content, and Windows-first workflows readable and stable.

## Why this exists

Encoding drift has repeatedly caused avoidable confusion:

- Chinese paths or docs render as garbled text
- PowerShell reads the same file differently on different machines
- text cleanup gets mixed with real delivery work

Encoding is therefore part of delivery quality, not a cosmetic cleanup task.

## Core rules

1. Do not store tracked text files in ANSI, GBK, or locale-default encodings.
2. For human-facing docs that contain Chinese or other non-ASCII content in the Windows-first workflow, prefer `UTF-8 with BOM`.
3. For source code and existing script files, preserve the current repository convention unless there is a specific compatibility reason to convert.
4. Do not mass-convert files blindly. Verify the target file type, toolchain sensitivity, and current convention first.
5. When a file is part of the active read chain, readability in default PowerShell is required.

## What must be checked

At minimum, these items should stay readable:

- development-system entry docs
- business-doc entry docs
- task files that contain Chinese content
- PowerShell-facing docs and scripts that are meant to be read directly on Windows

## Verification rule

Encoding work is considered acceptable only when:

1. key entry files render correctly with default `Get-Content`
2. tracked text files do not contain unexpected replacement characters such as `�`
3. binary files are not being treated as text during scanning

## Operational rule

If a task uncovers repeated encoding friction:

- report it as a delivery-quality issue
- record it as a development-system improvement candidate
- do not silently normalize large file sets unless the task explicitly includes that scope

## Scope boundary

Encoding governance applies to:

- development-system docs
- business-facing docs
- scripts used in the Windows workflow

It does not authorize broad codebase rewrites by default.
