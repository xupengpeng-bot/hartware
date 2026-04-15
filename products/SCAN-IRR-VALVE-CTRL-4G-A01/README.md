# SCAN-IRR-VALVE-CTRL-4G-A01

Product baseline identity:

- hardware code: `HW-SCAN-IRR-VALVE-CTRL-4G-A01`
- hardware sku: `SCAN-IRR-VALVE-CTRL-4G`
- revision: `A01`
- software family: `SCAN-IRRIGATION-VALVE-CONTROL`

Baseline source:

- copied from the active root controller program on 2026-04-14
- source root: [code](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/products/SCAN-IRR-VALVE-CTRL-4G-A01/code)
- flashing helpers: [build_flash](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/products/SCAN-IRR-VALVE-CTRL-4G-A01/build_flash)
- product capability set: [PRODUCT_CAPABILITY_SET_20260414.md](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/products/SCAN-IRR-VALVE-CTRL-4G-A01/PRODUCT_CAPABILITY_SET_20260414.md)

Current rule:

- make product-specific edits here when valve-control behavior needs to diverge from the active root mainline
- keep shared fixes in the root program unless the product truly needs a different implementation
- treat this folder as the valve-control working baseline, not as a build output cache

Current scope:

- identity strings already changed to valve-control naming
- build and flashing scripts are copied so the product can be built independently
- hardware-specific I/O remapping is still pending and should be implemented here in follow-up edits
