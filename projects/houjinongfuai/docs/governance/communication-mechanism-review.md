# Communication Mechanism Review

Status: active
Audience: PM, Codex, Lovable, embedded, hardware
Purpose: identify where the current communication flow is working and what should be tightened.

## 1. Frontend <-> Backend contract path

Current path:

- frontend uses `fetch`
- backend exposes NestJS REST under `/api/v1`
- page safety is partially protected by:
  - `backend/openapi.v1.yaml`
  - `docs/uat/frontend-backend-contract-checklist-v1.md`
  - `backend/test/e2e/view-contract.e2e-spec.ts`

Main problem:

- some historical prototype docs still described placeholder routes and DTOs
- this made it possible for people to read an old page map as if it were the real contract

Optimization:

- runtime API truth must live only in OpenAPI plus the contract checklist
- prototype page docs may describe UI intent, but not live endpoint truth

## 2. Codex <-> Lovable handoff path

Current path:

- backend side: `docs/uat/lovable-codex-sync.md`
- frontend side: `D:\\20251211\\zhinengti\\lovable\\lovablecomhis`

Main problem:

- the backend sync file grew into a mixed artifact containing:
  - current baseline
  - open issues
  - decisions log
  - historical closure notes

Optimization:

- keep `lovable-codex-sync.md` as the live cross-team issue ledger
- move durable process rules into `docs/governance/delivery-workflow.md`
- keep detailed frontend execution only in `lovablecomhis`

## 3. Device / protocol communication path

Current path:

- protocol docs are clear
- simulator skeleton exists
- backend modules exist for:
  - `device-gateway`
  - `protocol-adapter`
  - `runtime-ingest`

Main problem:

- the integration tests for gateway / ingest / settlement are still mostly skeleton coverage
- that means the protocol is documented, but the cross-module replay and side-effect chain is not fully locked by tests yet

Optimization:

- next protocol wave should add executable coverage for:
  - `REGISTER`
  - reconnect takeover by IMEI
  - duplicate packet idempotency
  - out-of-order tolerance
  - offline timeout auto-end
  - ACK / NACK routing and audit

## 4. PM coordination path

Current problem:

- task creation, Git sync, local pull, and acceptance did not yet have one explicit order for all teams

Optimization:

- all cross-team work must follow one PM-owned delivery workflow
- frontend task packages must be committed to Git before Lovable is asked to execute
- local frontend code must be pulled back before joint verification
- embedded and hardware now follow the same pattern through dedicated handoff folders

## 5. PM conclusion

The communication mechanism does not mainly need more channels.
It needs fewer truth sources and a stricter execution order.

The target state is:

- one canonical contract source
- one active issue ledger
- one delivery workflow
- one task package folder per execution team
