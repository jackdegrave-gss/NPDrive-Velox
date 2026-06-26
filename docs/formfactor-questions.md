# Questions to resolve with FormFactor (Velox integration)

We have no FormFactor materials yet. These answers determine whether *we* ship a
driver DLL or *they* write the Velox-side glue against our library, and they pin
down the placeholder ABI in `velox_adapter.h`.

## 1. Integration model
- Does Velox support **third-party positioner drivers via a published plug-in
  interface**, or does FormFactor implement the Velox-side integration given a
  control library + ICD from us?
- If a plug-in model exists: is it **COM** (interface IDs, registration) or a
  **flat C export** DLL? What discovery/registration mechanism does Velox use to
  find and load it?

## 2. Driver Developer Kit / ICD
- The exact **interface contract**: required methods, signatures, calling
  convention, error/return model, and any header/IDL files.
- **Threading model**: are calls serialized by Velox, or can it call concurrently?
  Must moves be non-blocking with async completion callbacks, or is a blocking
  move acceptable?
- Versioning/capability negotiation expectations.

## 3. Positioner model & semantics
- How are auxiliary/secondary positioners represented (axis model, naming,
  number of axes)? Each NP-Drive channel is an **independent 1-DOF stage** — do
  we expose channels as separate single-axis positioners or compose multi-axis?
- **Units** at the boundary (microns assumed) and coordinate-system / sign
  conventions; how "home"/reference is expected when there is no hardware home.
- Is **open-loop-only** hardware (our A100) supported at all, or is closed-loop
  feedback mandatory? (Drives whether we bother exposing A100.)
- Velocity model: Velox likely commands a speed; we only have waveform
  `amplitude`×`frequency`. How should a Velox speed value map onto those?

## 4. Logistics
- **Partner/OEM NDA** so FormFactor can release the SDK.
- Which **Velox version(s)** to target, and their test/certification process for
  a third-party driver.
- Reference/sample driver for any existing third-party positioner we can model.
