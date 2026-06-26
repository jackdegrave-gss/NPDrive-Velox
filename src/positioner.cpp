// positioner.cpp
#include "npdrive/positioner.h"

#include <stdexcept>
#include <thread>

namespace npdrive {

void NpDriveAxis::ensureFeedback(const char* op) const {
    if (!hasFeedback()) {
        throw std::logic_error(std::string(op) +
                               " requires position feedback (B100); this axis is open-loop");
    }
}

bool NpDriveAxis::isMoving() {
    // Either kind of motion counts as "moving".
    if (drive_.getStatusDriveBusy()) return true;
    if (model_ == Model::B100 && drive_.getStatusPositioning()) return true;
    return false;
}

void NpDriveAxis::moveAbsoluteMeters(double targetMeters, bool blocking) {
    ensureFeedback("moveAbsolute");
    if (!drive_.goPosition(channel_, targetMeters, speed_.amplitudeVolts, speed_.frequencyHz)) {
        throw std::runtime_error("goPosition was not accepted by the controller");
    }
    if (blocking && !waitForIdle(moveTimeout_)) {
        throw std::runtime_error("moveAbsolute timed out before reaching target");
    }
}

void NpDriveAxis::moveRelativeMeters(double deltaMeters, bool blocking) {
    bool accepted;
    if (hasFeedback()) {
        accepted = drive_.goInterval(channel_, deltaMeters,
                                     speed_.amplitudeVolts, speed_.frequencyHz);
    } else {
        // Open-loop axis: no distance units. We cannot honor a metric delta
        // without a calibration; reject rather than silently misbehave.
        throw std::logic_error(
            "moveRelativeMeters on an open-loop axis: use step-based jogs instead "
            "(A100 has no distance feedback)");
    }
    if (!accepted) {
        throw std::runtime_error("goInterval was not accepted by the controller");
    }
    if (blocking && !waitForIdle(moveTimeout_)) {
        throw std::runtime_error("moveRelative timed out before completing");
    }
}

double NpDriveAxis::getPositionMeters() {
    ensureFeedback("getPosition");
    return drive_.getPosition(channel_);
}

void NpDriveAxis::stop() {
    // Stop both motion engines; harmless if one is idle.
    drive_.stopMotion();
    if (model_ == Model::B100) {
        drive_.stopPositioning();
    }
}

AxisState NpDriveAxis::status() {
    try {
        if (drive_.getStatusDriveOverload()) return AxisState::Overload;
        if (isMoving()) return AxisState::Moving;
        return AxisState::Idle;
    } catch (const TransportError&) {
        return AxisState::Disconnected;
    } catch (...) {
        return AxisState::Error;
    }
}

bool NpDriveAxis::waitForIdle(std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
        if (drive_.getStatusDriveOverload()) {
            stop();
            throw std::runtime_error("drive overload during motion");
        }
        if (!isMoving()) return true;
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(pollInterval_);
    }
}

void NpDriveAxis::setStopLimitMeters(double thresholdMeters) {
    ensureFeedback("setStopLimit");
    if (!drive_.setStopLimit(channel_, thresholdMeters)) {
        throw std::runtime_error("setStopLimit was not accepted by the controller");
    }
}

}  // namespace npdrive
