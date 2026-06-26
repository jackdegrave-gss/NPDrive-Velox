# NP-Drive → Velox driver

C++ driver stack to integrate the **Renaissance Scientific NP-Drive** nanopositioner
controller (model **B100**, closed-loop) with **FormFactor's Velox** probe-station
software suite.

## Status

| Layer | State |
|-------|-------|
| `JsonRpcClient` — TCP/JSON-RPC 2.0 transport (port 6002) | ✅ implemented, builds |
| `NpDrive` — typed wrapper over every UM100 §9.2 method | ✅ implemented, builds |
| `IPositionerAxis` / `NpDriveAxis` — generic positioner abstraction | ✅ implemented, builds |
| `velox_adapter` — Velox-facing DLL export surface | ⚠️ **placeholder ABI** — reshape to FormFactor's ICD |
| Live hardware validation | ⛔ pending NP-Drive on the bench |

The bottom three layers are fully determined by the NP-Drive manual (UM100 v1.4).
The Velox adapter is a stub: it forwards to `IPositionerAxis`, but its export
contract is a *guess* until FormFactor supplies their driver developer kit. See
[docs/formfactor-questions.md](docs/formfactor-questions.md).

## Build

Requires Visual Studio 2026 Build Tools (MSVC + bundled CMake/Ninja). From a
normal shell:

```
scripts\build.bat
```

Outputs to `build\`:
- `npdrive_core.lib` — static core library
- `npdrive_velox.dll` — Velox-facing driver (placeholder ABI)
- `npdrive_smoke.exe` — offline tests + opt-in live test

## Test

```
build\npdrive_smoke.exe
```

Offline tests (range validation, unit conversion, open-loop guards) run with no
hardware. To exercise a real controller, set its IP first:

```
set NPDRIVE_HOST=192.168.68.118
build\npdrive_smoke.exe
```

## Layout

```
include/npdrive/   public headers (json_rpc_client, np_drive, positioner, velox_adapter)
src/               implementations
test/              smoke_test.cpp
third_party/       nlohmann/json (vendored, MIT)
docs/              integration notes + FormFactor questions
scripts/build.bat  one-shot build using the installed Build Tools
```

## Key device facts baked into this driver

- One HV channel relay on at a time; any move auto-selects its channel.
- **Auto-Ground is unavailable over the remote interface** — grounding is
  explicit (`setDriveChannelsOff`).
- Move commands return "received", not "arrived" — completion is detected by
  polling `getStatusDriveBusy` / `getStatusPositioning` (handled in `NpDriveAxis`).
- Positions/intervals are in **meters**; the abstraction exposes micron helpers
  for Velox.
- B100 closed loop = channels 1..3, absolute capacitive feedback. A100 = open
  loop, up to 9 channels, **no feedback** (relative jogs only).
