# Product Baselines

Use this directory for product-specific firmware baselines that intentionally branch from the active shared program.

## Naming Rule

Create one folder per board or product baseline using:

- `<HARDWARE-SKU>-<REV>`

Current product baselines:

- [SCAN-IRR-VALVE-CTRL-4G-A01](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/products/SCAN-IRR-VALVE-CTRL-4G-A01)

## Put Here

- a copied `code` tree when a product must diverge from the active shared firmware
- a copied `build_flash` helper set when the product needs independent build or flashing scripts
- product-only notes describing why the branch exists and what still remains shared

## Do Not Put Here

- hardware-only PDFs or board images
- current root build outputs
- temporary logs

Those stay in:

- [Hardware](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/Hardware)
- root build output folders under [hartware](/D:/Develop/houji/houjinongfuAI-Cursor/hartware)
