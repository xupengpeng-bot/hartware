# Remote Upgrade Platform and Embedded Coordination

Date: `2026-04-12`
Status: active reference

## Platform capabilities already available

1. archive software versions
2. upload program packages, source packages, and reference bundles
3. create single-device or batch upgrade jobs
4. split each job into per-device upgrade items
5. dispatch upgrade intent commands to devices
6. receive device-side upgrade progress and final results

## Downlink command contract

Platform dispatches firmware upgrade as a device gateway action command:

- action name: `upgrade_firmware`
- short action: `upg`
- scope: `common`
- mode: synchronous accept or reject only
- rule: no delayed replay

Meaning:

- platform asks the device to start one upgrade workflow
- device first decides whether to accept this upgrade
- download, verification, installation, reboot, and final result are reported asynchronously

## Embedded acceptance chain

After `upg` arrives, embedded should execute only this chain:

1. validate whether upgrade is currently allowed
2. return `AK` if accepted
3. return `NK` if rejected
4. if accepted, start downloading the package
5. verify package integrity after download
6. install only after verification succeeds
7. reboot after installation
8. report the final result after reboot

## Meaning of `AK`

`AK` means only:

- the device accepted the upgrade job
- the device will enter the upgrade workflow

`AK` does not mean:

- download succeeded
- installation succeeded
- final upgrade succeeded

## When to return `NK`

Return `NK` directly when:

- an upgrade is already in progress
- the pump is in a critical running state and upgrade is not allowed
- voltage, battery, or protection state does not satisfy upgrade conditions
- package information is missing
- download URL is invalid
- version data does not match
- current firmware does not support this upgrade

## Required command payload fields

At minimum, embedded must read these fields from `upg`:

1. `upgrade_token`
2. `upgrade_job_id`
3. `upgrade_item_id`
4. `release_id`
5. `release_code`
6. `package_artifact_id`
7. `package_download_url`
8. `package_file_name`
9. `package_checksum`
10. `package_size_bytes`

Embedded must not guess package versions or reconstruct download URLs.

## Report endpoint

Upgrade progress and result reports are sent to:

`POST /api/v1/firmware/device-upgrade-reports`

Minimum report fields:

1. `upgrade_token`
2. `imei`
3. `stage`
4. `result`
5. `progress_percent`

Recommended extra fields:

1. `reason_code`
2. `message`
3. `firmware_version`
4. `checksum`
5. `extra`

## Allowed stages

Only these values are allowed:

1. `command_acked`
2. `downloading`
3. `installing`
4. `rebooting`
5. `succeeded`
6. `failed`
7. `cancelled`

## Allowed results

Only these values are allowed:

1. `accepted`
2. `running`
3. `succeeded`
4. `failed`
5. `cancelled`

## Recommended report rhythm

1. immediately after accept: `stage=command_acked`
2. during download: `stage=downloading`
3. during install: `stage=installing`
4. just before reboot: `stage=rebooting`
5. final success: `stage=succeeded`
6. any failure: `stage=failed`

## Strong rules

1. each `upgrade_token` may execute only once
2. do not replay old upgrades after reconnect
3. do not continue a failed upgrade silently
4. never install if checksum verification failed
5. installation failure must report a clear reason code
6. success must report the new firmware version
7. if rollback is supported, rollback result must also be reported

## Responsibility boundary

Platform is responsible for:

1. version archive
2. upgrade jobs
3. upgrade items
4. command dispatch
5. progress aggregation
6. result presentation

Embedded is responsible for:

1. accept or reject the upgrade
2. download package data
3. verify package integrity
4. install package data
5. reboot
6. report upgrade stages and final result

## Explicitly not in scope for this round

1. automatic gray release strategy
2. package selection on device
3. platform URL synthesis on device
4. treating upgrade accept as final success
5. replaying old `upg` after reconnect
