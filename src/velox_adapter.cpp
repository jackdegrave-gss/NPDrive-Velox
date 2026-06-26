// velox_adapter.cpp
//
// Implementation of the PLACEHOLDER Velox export surface. Every entry point is
// a thin, exception-safe forward to npdrive::IPositionerAxis. When FormFactor's
// real ABI is known, only this file (and velox_adapter.h) should need to change.

#include "npdrive/velox_adapter.h"

#include <exception>
#include <memory>
#include <string>

#include "npdrive/json_rpc_client.h"
#include "npdrive/np_drive.h"
#include "npdrive/positioner.h"

using namespace npdrive;

// One handle owns its own connection + device + axis. Declaration order matters:
// client must outlive drive, which must outlive axis (both hold references).
struct npx_axis {
    JsonRpcClient client;
    NpDrive drive;
    NpDriveAxis axis;
    std::string lastError;

    npx_axis(int channel, Model model)
        : drive(client), axis(drive, channel, model) {}
};

namespace {

npx_status mapException(npx_axis* a, const std::exception& e) {
    if (a) a->lastError = e.what();
    const std::string msg = e.what();
    if (dynamic_cast<const TransportError*>(&e)) {
        return msg.find("timed out") != std::string::npos ? NPX_ERR_TIMEOUT : NPX_ERR_CONNECT;
    }
    if (msg.find("overload") != std::string::npos) return NPX_ERR_OVERLOAD;
    if (msg.find("feedback") != std::string::npos) return NPX_ERR_NO_FEEDBACK;
    if (msg.find("not accepted") != std::string::npos) return NPX_ERR_NOT_ACCEPTED;
    if (msg.find("out of range") != std::string::npos ||
        msg.find("range") != std::string::npos) {
        return NPX_ERR_RANGE;
    }
    return NPX_ERR_UNKNOWN;
}

// Run a body that may throw, translating to an npx_status.
template <typename F>
npx_status guard(npx_axis* a, F&& body) {
    if (!a) return NPX_ERR_HANDLE;
    try {
        body();
        a->lastError.clear();
        return NPX_OK;
    } catch (const std::exception& e) {
        return mapException(a, e);
    } catch (...) {
        a->lastError = "unknown error";
        return NPX_ERR_UNKNOWN;
    }
}

}  // namespace

npx_status NPX_CALL npx_open(const char* host, uint16_t port, int channel,
                             npx_model model, npx_axis** out_axis) {
    if (!host || !out_axis) return NPX_ERR_HANDLE;
    *out_axis = nullptr;
    const Model m = (model == NPX_MODEL_B100) ? Model::B100 : Model::A100;
    auto handle = std::make_unique<npx_axis>(channel, m);
    try {
        handle->client.connect(host, port);
    } catch (const std::exception& e) {
        handle->lastError = e.what();
        // Surface the failure but still hand back the handle so the caller can
        // read npx_last_error(); they must npx_close() it.
        *out_axis = handle.release();
        return NPX_ERR_CONNECT;
    }
    *out_axis = handle.release();
    return NPX_OK;
}

void NPX_CALL npx_close(npx_axis* axis) {
    delete axis;
}

npx_status NPX_CALL npx_set_speed(npx_axis* axis, int amplitudeVolts, int frequencyHz) {
    return guard(axis, [&] {
        if (amplitudeVolts < limits::kAmplitudeMinV || amplitudeVolts > limits::kAmplitudeMaxV ||
            frequencyHz < limits::kFrequencyMinHz || frequencyHz > limits::kFrequencyMaxHz) {
            throw std::out_of_range("speed (amplitude/frequency) out of range");
        }
        axis->axis.setSpeed(DriveParams{amplitudeVolts, frequencyHz});
    });
}

npx_status NPX_CALL npx_move_abs_um(npx_axis* axis, double targetUm) {
    return guard(axis, [&] { axis->axis.moveAbsoluteMicrons(targetUm, /*blocking=*/true); });
}

npx_status NPX_CALL npx_move_rel_um(npx_axis* axis, double deltaUm) {
    return guard(axis, [&] { axis->axis.moveRelativeMicrons(deltaUm, /*blocking=*/true); });
}

npx_status NPX_CALL npx_get_pos_um(npx_axis* axis, double* out_um) {
    return guard(axis, [&] {
        if (!out_um) throw std::invalid_argument("out_um is null");
        *out_um = axis->axis.getPositionMicrons();
    });
}

npx_status NPX_CALL npx_stop(npx_axis* axis) {
    return guard(axis, [&] { axis->axis.stop(); });
}

npx_status NPX_CALL npx_get_state(npx_axis* axis, npx_state* out_state) {
    return guard(axis, [&] {
        if (!out_state) throw std::invalid_argument("out_state is null");
        switch (axis->axis.status()) {
            case AxisState::Idle: *out_state = NPX_STATE_IDLE; break;
            case AxisState::Moving: *out_state = NPX_STATE_MOVING; break;
            case AxisState::Overload: *out_state = NPX_STATE_OVERLOAD; break;
            case AxisState::Disconnected: *out_state = NPX_STATE_DISCONNECTED; break;
            default: *out_state = NPX_STATE_ERROR; break;
        }
    });
}

npx_status NPX_CALL npx_set_stop_limit_um(npx_axis* axis, double thresholdUm) {
    return guard(axis, [&] { axis->axis.setStopLimitMeters(thresholdUm * 1e-6); });
}

const char* NPX_CALL npx_last_error(npx_axis* axis) {
    if (!axis) return "invalid handle";
    return axis->lastError.empty() ? "" : axis->lastError.c_str();
}
