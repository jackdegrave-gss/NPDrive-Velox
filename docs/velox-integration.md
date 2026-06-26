# NP-Drive ↔ Velox integration notes

Source of truth for device behavior: **NP-Drive User Manual UM100, v1.4 (March 2025)**.

## Capability mapping: generic positioner ⇄ NP-Drive

| Positioner concept (what Velox expects) | NP-Drive (B100) | NP-Drive (A100) |
|---|---|---|
| Read absolute position | `getPosition(ch)` → meters | ❌ none |
| Move absolute | `goPosition(ch, target_m, amp, freq)` | ❌ none |
| Move relative | `goInterval(ch, delta_m, amp, freq)` | `goSteps{Fwd,Rev}` (uncalibrated steps) |
| Jog / continuous | `goContinuous{Fwd,Rev}` + `stopMotion` | same |
| Stop | `stopMotion` + `stopPositioning` | `stopMotion` |
| Velocity / speed | **no native velocity** → map to `amplitude` (10–300 V) × `frequency` (100–4000 Hz) | same |
| Arrival window / tolerance | `setStopLimit(ch, thr_m)` | n/a |
| Hold against drift | `holdPosition(ch, target_m, amp, timeout_s)` | n/a |
| Motion-complete | poll `getStatusPositioning` | poll `getStatusDriveBusy` |
| Fault | `getStatusDriveOverload` | same |
| Home / reference | **no hardware home**; B100 sensor is absolute "from center" | none |
| Units | **meters** (convert to/from Velox microns) | n/a |

## Behaviors that shaped the driver

1. **Fire-and-acknowledge.** Every move RPC returns `true` = *command received*,
   not *motion done*. `NpDriveAxis::waitForIdle()` polls status (and overload)
   until idle — this is what makes a Velox blocking "move" work.
2. **One channel at a time** (mechanical relay mux). A move auto-selects its
   channel and drops the others; `setDriveChannelsOff()` grounds all HV lines.
3. **No Auto-Ground over remote** (UM100 §9.1). The UI/handset auto-ground does
   not apply; grounding is the driver's responsibility.
4. **Last command wins.** Any new move from any source (UI, handset, remote,
   TTL trigger) preempts motion in progress.
5. **Transport framing.** The device sends one JSON object per response with no
   length prefix; the manual's `recv(128)` example truncates. `JsonRpcClient`
   reads until the buffer parses as complete JSON.
6. **Type leniency.** The manual shows params as both ints and strings; results
   are coerced defensively (`asBool`/`asDouble`/`asInt`).

## The A100 vs B100 gap (matters for Velox)

Velox positioners assume a coordinate system and feedback. **B100 maps cleanly.**
**A100 cannot** present absolute coordinates or go-to-position — only relative,
uncalibrated step jogs. If A100 support is ever required, confirm with FormFactor
whether Velox accepts an open-loop / jog-only auxiliary positioner at all.

## What is still unknown (blocks the top layer)

The `velox_adapter` export ABI is a **placeholder**. The real contract — COM vs
flat C, method set, units, threading model, registration/discovery — comes from
FormFactor. When it arrives, only `velox_adapter.{h,cpp}` should change; the
`IPositionerAxis` seam keeps the rest stable. Open questions:
[docs/formfactor-questions.md](formfactor-questions.md).
