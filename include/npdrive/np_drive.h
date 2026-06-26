// np_drive.h
//
// Typed C++ wrapper over the NP-Drive JSON-RPC methods (UM100 §9.2).
// Method names and parameter ranges mirror the manual exactly. Closed-loop
// methods are valid only on a B100 (channels 1..3); open-loop methods work on
// any channel the model exposes (A100: 1..9, B100: 1..3).
//
// IMPORTANT semantics encoded by the docs that callers must respect:
//   * Move/position commands are FIRE-AND-ACKNOWLEDGE. A `true` return means
//     "command received", not "motion complete". Poll getStatusDriveBusy()
//     (open loop) or getStatusPositioning() (closed loop) for completion.
//   * Only one HV drive channel relay is on at a time. Any move auto-selects
//     its channel; setDriveChannelsOff() grounds all lines.
//   * Auto-Ground is NOT available over the remote interface (UM100 §9.1).
//   * Positions and intervals are in METERS.

#pragma once

#include <cstdint>
#include <string>

#include "npdrive/json_rpc_client.h"

namespace npdrive {

// Drive waveform shaping. amplitude is peak volts (10..300), frequency is
// step/waveform cycles per second (100..4000). These are the closest analog to
// a "speed" the NP-Drive exposes.
struct DriveParams {
    int amplitudeVolts = 100;
    int frequencyHz = 1000;
};

class NpDrive {
public:
    explicit NpDrive(JsonRpcClient& client) : client_(client) {}

    // ---- Open-loop motion (UM100 §9.2.1) ----
    bool goStepsForward(int channel, std::int64_t steps, int amplitude, int frequency);
    bool goStepsReverse(int channel, std::int64_t steps, int amplitude, int frequency);
    bool goContinuousForward(int channel, int amplitude, int frequency);
    bool goContinuousReverse(int channel, int amplitude, int frequency);

    // ---- Closed-loop motion, B100 only (UM100 §9.2.2) ----
    double getPosition(int channel);                       // returns meters
    bool setSensorsOff();
    bool goPosition(int channel, double targetMeters, int amplitude, int frequency);
    bool goInterval(int channel, double deltaMeters, int amplitude, int frequency);
    bool setStopLimit(int channel, double thresholdMeters);
    double getStopLimit(int channel);                      // returns meters
    bool stopPositioning();
    bool getStatusPositioning();                           // true == closed loop active
    bool holdPosition(int channel, double targetMeters, int amplitude, int timeoutSeconds);

    // ---- General (UM100 §9.2.3) ----
    bool stopMotion();
    bool setDriveChannel(int channel);
    bool setDriveChannelsOff();
    int getDriveChannel();                                 // 0 == none, -1 == error
    bool getStatusDriveBusy();                             // true == open-loop move in progress
    bool getStatusDriveOverload();

private:
    JsonRpcClient& client_;
};

// Range constants from the manual, exposed for validation/UI.
namespace limits {
constexpr int kAmplitudeMinV = 10;
constexpr int kAmplitudeMaxV = 300;
constexpr int kFrequencyMinHz = 100;
constexpr int kFrequencyMaxHz = 4000;
constexpr int kChannelMin = 1;
constexpr int kChannelMaxA100 = 9;
constexpr int kChannelMaxB100 = 3;
constexpr double kTargetMinM = -1.0;   // parameter bound; real travel is far smaller
constexpr double kTargetMaxM = 1.0;
}  // namespace limits

}  // namespace npdrive
