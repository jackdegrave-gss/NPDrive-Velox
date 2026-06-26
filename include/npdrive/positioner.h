// positioner.h
//
// Generic single-axis positioner abstraction. This is the stable seam between
// the NP-Drive device layer and whatever interface FormFactor's Velox driver
// kit ultimately requires. Keep this interface free of NP-Drive specifics so
// the Velox adapter binds to it, not to the JSON-RPC layer.
//
// Each NP-Drive channel is an independent 1-DOF stage, so it maps to one axis.
// A multi-axis Velox positioner (X/Y/Z) is composed from several axes.

#pragma once

#include <chrono>
#include <string>

#include "npdrive/np_drive.h"

namespace npdrive {

enum class AxisState {
    Idle,       // connected, not moving
    Moving,     // open- or closed-loop motion in progress
    Overload,   // drive overload latched/observed
    Disconnected,
    Error,
};

enum class Model {
    A100,  // open loop only, channels 1..9, no position feedback
    B100,  // closed loop, channels 1..3, absolute capacitive sensor (meters)
};

// Abstract positioner axis. moveAbsolute/getPosition require feedback (B100);
// on an open-loop axis they throw std::logic_error.
class IPositionerAxis {
public:
    virtual ~IPositionerAxis() = default;

    virtual bool hasFeedback() const = 0;

    // Absolute/relative motion in meters. If `blocking`, returns only once the
    // axis is idle (or throws on timeout/overload).
    virtual void moveAbsoluteMeters(double targetMeters, bool blocking = true) = 0;
    virtual void moveRelativeMeters(double deltaMeters, bool blocking = true) = 0;

    virtual double getPositionMeters() = 0;     // requires feedback
    virtual void stop() = 0;
    virtual AxisState status() = 0;

    // Block until the axis reports idle. Returns true if idle, false on timeout.
    virtual bool waitForIdle(std::chrono::milliseconds timeout) = 0;

    // Micron convenience wrappers (Velox typically works in microns).
    void moveAbsoluteMicrons(double um, bool blocking = true) {
        moveAbsoluteMeters(um * 1e-6, blocking);
    }
    void moveRelativeMicrons(double um, bool blocking = true) {
        moveRelativeMeters(um * 1e-6, blocking);
    }
    double getPositionMicrons() { return getPositionMeters() * 1e6; }
};

// Concrete NP-Drive-backed axis.
class NpDriveAxis : public IPositionerAxis {
public:
    NpDriveAxis(NpDrive& drive, int channel, Model model, DriveParams speed = {})
        : drive_(drive), channel_(channel), model_(model), speed_(speed) {}

    bool hasFeedback() const override { return model_ == Model::B100; }

    // Maps a logical "speed" onto the drive waveform (amplitude/frequency).
    void setSpeed(const DriveParams& speed) { speed_ = speed; }
    DriveParams speed() const { return speed_; }

    int channel() const { return channel_; }

    // Tuning for blocking waits.
    void setPollInterval(std::chrono::milliseconds p) { pollInterval_ = p; }
    void setMoveTimeout(std::chrono::milliseconds t) { moveTimeout_ = t; }

    void moveAbsoluteMeters(double targetMeters, bool blocking = true) override;
    void moveRelativeMeters(double deltaMeters, bool blocking = true) override;
    double getPositionMeters() override;
    void stop() override;
    AxisState status() override;
    bool waitForIdle(std::chrono::milliseconds timeout) override;

    // Closed-loop stop window (B100). Sets how close to target counts as arrived.
    void setStopLimitMeters(double thresholdMeters);

private:
    void ensureFeedback(const char* op) const;
    bool isMoving();  // true if open- or closed-loop motion is active

    NpDrive& drive_;
    int channel_;
    Model model_;
    DriveParams speed_;
    std::chrono::milliseconds pollInterval_{50};
    std::chrono::milliseconds moveTimeout_{60000};
};

}  // namespace npdrive
