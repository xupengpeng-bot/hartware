# Lovable Sync Rule

Status: active
Audience: PM, 软件工程师, Lovable
Purpose: define how frontend changes are synchronized from the cloud repo back to local.

## Rule

- Lovable executes in the cloud frontend repo and the platform auto-syncs each code change to GitHub.
- The latest GitHub `main` result is the primary execution evidence.
- Local acceptance uses Git pull plus local verification.

## Commit lookup

- Lovable may not be able to read the commit SHA inside its own platform.
- 软件工程师 should obtain the SHA from GitHub or from local Git after pull.
- A missing SHA in Lovable chat feedback does not block local verification as long as GitHub has the latest change.

## Local sync owner

软件工程师 owns:

1. pulling the latest frontend Git result back to local
2. local build or integration verification as required by the task
3. recording any mismatch between claimed result and local verification

## Closure rule

A frontend task is not closed just because Lovable says it is done in cloud.
It closes only after the local frontend repo matches the Git result and passes local verification.
