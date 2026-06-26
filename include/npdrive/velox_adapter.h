// velox_adapter.h
//
// ============================ PLACEHOLDER ABI ===============================
// This is a STUB export surface for the Velox-facing driver DLL. It is a
// reasonable, generic shape for a native positioner plug-in, but it is NOT
// FormFactor's real interface. Once FormFactor provides their Velox Driver
// Developer Kit / ICD, reshape these exports (names, calling convention,
// COM vs C, error model, units, threading) to match it. The body simply
// forwards to npdrive::IPositionerAxis, so swapping the ABI is a thin edit
// and the device/abstraction layers below are unaffected.
//
// Units here are MICRONS at the boundary (Velox convention); the abstraction
// layer converts to the device's native meters.
// ===========================================================================

#pragma once

#include <cstdint>

#if defined(_WIN32)
#  if defined(NPX_BUILD_DLL)
#    define NPX_API __declspec(dllexport)
#  else
#    define NPX_API __declspec(dllimport)
#  endif
#  define NPX_CALL __stdcall
#else
#  define NPX_API
#  define NPX_CALL
#endif

extern "C" {

typedef struct npx_axis npx_axis;  // opaque handle

typedef enum npx_status {
    NPX_OK = 0,
    NPX_ERR_CONNECT = 1,
    NPX_ERR_RANGE = 2,
    NPX_ERR_OVERLOAD = 3,
    NPX_ERR_TIMEOUT = 4,
    NPX_ERR_NO_FEEDBACK = 5,   // operation needs B100 closed loop
    NPX_ERR_NOT_ACCEPTED = 6,  // controller rejected the command
    NPX_ERR_HANDLE = 7,
    NPX_ERR_UNKNOWN = 99,
} npx_status;

typedef enum npx_model {
    NPX_MODEL_A100 = 0,  // open loop, no feedback
    NPX_MODEL_B100 = 1,  // closed loop, absolute feedback
} npx_model;

typedef enum npx_state {
    NPX_STATE_IDLE = 0,
    NPX_STATE_MOVING = 1,
    NPX_STATE_OVERLOAD = 2,
    NPX_STATE_DISCONNECTED = 3,
    NPX_STATE_ERROR = 4,
} npx_state;

// Open one axis = one NP-Drive channel. host is the controller IP, port is
// usually 6002, channel is 1-based, model selects feedback capability.
NPX_API npx_status NPX_CALL npx_open(const char* host, uint16_t port, int channel,
                                     npx_model model, npx_axis** out_axis);

NPX_API void NPX_CALL npx_close(npx_axis* axis);

// Map a Velox "speed" onto the drive waveform (amplitude V, frequency Hz).
NPX_API npx_status NPX_CALL npx_set_speed(npx_axis* axis, int amplitudeVolts, int frequencyHz);

// Blocking absolute / relative moves, microns. (Absolute requires B100.)
NPX_API npx_status NPX_CALL npx_move_abs_um(npx_axis* axis, double targetUm);
NPX_API npx_status NPX_CALL npx_move_rel_um(npx_axis* axis, double deltaUm);

// Read absolute position, microns. (Requires B100.)
NPX_API npx_status NPX_CALL npx_get_pos_um(npx_axis* axis, double* out_um);

NPX_API npx_status NPX_CALL npx_stop(npx_axis* axis);
NPX_API npx_status NPX_CALL npx_get_state(npx_axis* axis, npx_state* out_state);

// Closed-loop arrival window, microns. (Requires B100.)
NPX_API npx_status NPX_CALL npx_set_stop_limit_um(npx_axis* axis, double thresholdUm);

// Human-readable description of the last failure on this handle (never null).
NPX_API const char* NPX_CALL npx_last_error(npx_axis* axis);

}  // extern "C"
