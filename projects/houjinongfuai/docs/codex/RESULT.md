# Codex Result

Status: active-template
Audience: Codex and PM
Purpose: overwrite the latest-result section after each execution. Keep the field order stable.

## Required format

1. execution time
2. task id
3. status
4. changed files
5. migration or contract summary
6. verification result
7. commit SHA or `no git action`
8. frontend impact
9. pending issues
10. next handoff target

## Latest result

- execution time
  - 2026-03-27
- task id
  - `COD-2026-03-27-035`（**全量收口**：空间真相分层 + 写边界 + 契约输出）
- mode
  - **`BACKEND`**
- status
  - **`fixed`**
- changed files / synced files
  - **`backend/src/common/location/effective-location.ts`**（**`resolveEffectiveLocation`**；**`ASSET_EFFECTIVE_*_SQL`**；**`buildSpatialLocationReadModel*`** 含 **`layersContract`**；**`MAP_TRUTH_NOTICE`**）
  - **`backend/src/common/location/spatial-location-semantics.ts`**（**`SPATIAL_LOCATION_LAYERS_CONTRACT_V1`** 分层说明；**`assertNoForbiddenSpatialWriteKeys`**）
  - **`backend/src/modules/asset/asset.module.ts`**（资产 **POST/PUT** 拒绝 **`reported_*`/`effective_*`/`map_display_*`/`location_read_model`**；列表含 **`spatial_location_contract`**；**`GET /assets/spatial-location-contract`**）
  - **`backend/src/modules/device-ledger/device-ledger.module.ts`**（列表 **`spatial_location_contract`**；**`GET /devices/spatial-location-contract`**）
  - **`backend/src/modules/device-ledger/device-ledger.repository.ts`**（SQL 不再从 **`ext_json`** 直接读 effective）
  - **`backend/src/modules/device-ledger/device-ledger.service.ts`**（**`enrichLocation`**；创建/更新 **assert** 与 class-validator **双保险**）
  - **`backend/test/unit/spatial-location-semantics.spec.ts`**、**`backend/test/unit/effective-location.spec.ts`**、**`backend/test/unit/device-ledger.service.spec.ts`**
  - **`docs/codex/RESULT.md`**
- migration or contract summary
  - **无新 DDL**。
  - **真相输入层**：仅 **`manual_*`** + **`location_source_strategy`**（资产/设备普通表单）。
  - **只读派生层**：**`reported_*`**（遥测/可信导入）；**`effective_*`**（后端按策略计算）。
  - **地图/列表展示**：**`effective_*`**、**`map_display_*`**、**`location_read_model.mapDisplay`**；**`location_read_model.layersContract`** 为机器可读分层说明。
  - **下游稳定引用**：已发布 **`network_model_version_id`** + 图；空间坐标引用 **`effective_*`**，**不得**把地图画布临时坐标当 DB 真相。
  - **写边界**：资产 **POST/PUT** 显式 **400**（**`SPATIAL_WRITE_BOUNDARY`**）；设备 **DTO whitelist** + service **assert**。
  - **保留前期收口**：PumpValve/Topology、**`network_model_version`** 发布态、**solver 仅已发布图** — 未回滚。
- verification result
  - **`npm run build`**：**通过**
  - **`npm run test:unit`**：**通过**（28 tests）
- commit SHA or `no git action`
  - 见本回合 `git log -1`
- frontend impact
  - 可缓存 **`GET /api/v1/assets/spatial-location-contract`**、**`GET /api/v1/devices/spatial-location-contract`** 或列表响应中的 **`spatial_location_contract`**；每条 **`location_read_model.layersContract`** 同义。
  - 列表/地图默认绑 **`effective_*`/`map_display_*`**；表单只写 **`manual_*`**；**不得**提交 **`reported_*`/`effective_*`**。
- pending issues
  - 无（本 **`COD-035`** 空间子包按约定全量收口）
- next handoff target
  - 前端按契约绑定展示与搜索字段；画布坐标仅在提交到 **manual** 前为草稿
