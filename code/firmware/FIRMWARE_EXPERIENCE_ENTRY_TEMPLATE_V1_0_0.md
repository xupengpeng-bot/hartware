# Firmware Experience Entry Template

Version: `v1.0.0`
Date: `2026-04-11`
Status: active

Use this template when adding a new entry to the firmware experience library.

```text
### EXP-00NN

- id: EXP-00NN
- title:
- status: active / trial / superseded / retired
- date: YYYY-MM-DD
- scope:
- problem:
- wrong_assumption:
- evidence:
  - source 1
  - source 2
- current_rule:
- implementation_note:
- verification:
- supersedes: none / EXP-00MM
- superseded_by: none / EXP-00PP
```

Rules:

- never delete a wrong historical lesson without replacement
- when correcting an old lesson, mark the old one `superseded`
- the new entry must explain why the old one became wrong
- use repository files, logs, protocol pages, or hardware documents as evidence
