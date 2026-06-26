// np_drive.cpp
#include "npdrive/np_drive.h"

#include <stdexcept>

namespace npdrive {
namespace {

void requireRange(const char* name, double value, double lo, double hi) {
    if (value < lo || value > hi) {
        throw std::out_of_range(std::string(name) + " out of range [" +
                                std::to_string(lo) + ", " + std::to_string(hi) + "]");
    }
}

void validateAmpFreq(int amplitude, int frequency) {
    requireRange("amplitude", amplitude, limits::kAmplitudeMinV, limits::kAmplitudeMaxV);
    requireRange("frequency", frequency, limits::kFrequencyMinHz, limits::kFrequencyMaxHz);
}

void validateChannel(int channel) {
    // Upper bound is model-dependent; enforce the permissive A100 bound here and
    // let the higher (positioner) layer enforce model-specific limits.
    requireRange("channel", channel, limits::kChannelMin, limits::kChannelMaxA100);
}

// The firmware returns booleans/numbers; coerce defensively since some builds
// have been observed to echo values as strings.
bool asBool(const nlohmann::json& j) {
    if (j.is_boolean()) return j.get<bool>();
    if (j.is_number()) return j.get<double>() != 0.0;
    if (j.is_string()) {
        const std::string s = j.get<std::string>();
        return s == "true" || s == "1" || s == "True";
    }
    return false;
}

double asDouble(const nlohmann::json& j) {
    if (j.is_number()) return j.get<double>();
    if (j.is_string()) return std::stod(j.get<std::string>());
    throw std::runtime_error("expected numeric result");
}

int asInt(const nlohmann::json& j) {
    if (j.is_number()) return static_cast<int>(j.get<double>());
    if (j.is_string()) return std::stoi(j.get<std::string>());
    throw std::runtime_error("expected integer result");
}

}  // namespace

// ---- Open-loop motion ----

bool NpDrive::goStepsForward(int channel, std::int64_t steps, int amplitude, int frequency) {
    validateChannel(channel);
    validateAmpFreq(amplitude, frequency);
    if (steps < 0) throw std::out_of_range("steps must be >= 0");
    return asBool(client_.call("goStepsForward", {channel, steps, amplitude, frequency}));
}

bool NpDrive::goStepsReverse(int channel, std::int64_t steps, int amplitude, int frequency) {
    validateChannel(channel);
    validateAmpFreq(amplitude, frequency);
    if (steps < 0) throw std::out_of_range("steps must be >= 0");
    return asBool(client_.call("goStepsReverse", {channel, steps, amplitude, frequency}));
}

bool NpDrive::goContinuousForward(int channel, int amplitude, int frequency) {
    validateChannel(channel);
    validateAmpFreq(amplitude, frequency);
    return asBool(client_.call("goContinuousForward", {channel, amplitude, frequency}));
}

bool NpDrive::goContinuousReverse(int channel, int amplitude, int frequency) {
    validateChannel(channel);
    validateAmpFreq(amplitude, frequency);
    return asBool(client_.call("goContinuousReverse", {channel, amplitude, frequency}));
}

// ---- Closed-loop motion (B100) ----

double NpDrive::getPosition(int channel) {
    validateChannel(channel);
    return asDouble(client_.call("getPosition", {channel}));
}

bool NpDrive::setSensorsOff() {
    return asBool(client_.call("setSensorsOff"));
}

bool NpDrive::goPosition(int channel, double targetMeters, int amplitude, int frequency) {
    validateChannel(channel);
    validateAmpFreq(amplitude, frequency);
    requireRange("target", targetMeters, limits::kTargetMinM, limits::kTargetMaxM);
    return asBool(client_.call("goPosition", {channel, targetMeters, amplitude, frequency}));
}

bool NpDrive::goInterval(int channel, double deltaMeters, int amplitude, int frequency) {
    validateChannel(channel);
    validateAmpFreq(amplitude, frequency);
    requireRange("delta", deltaMeters, limits::kTargetMinM, limits::kTargetMaxM);
    return asBool(client_.call("goInterval", {channel, deltaMeters, amplitude, frequency}));
}

bool NpDrive::setStopLimit(int channel, double thresholdMeters) {
    validateChannel(channel);
    requireRange("threshold", thresholdMeters, 0.0, 1.0);
    return asBool(client_.call("setStopLimit", {channel, thresholdMeters}));
}

double NpDrive::getStopLimit(int channel) {
    validateChannel(channel);
    return asDouble(client_.call("getStopLimit", {channel}));
}

bool NpDrive::stopPositioning() {
    return asBool(client_.call("stopPositioning"));
}

bool NpDrive::getStatusPositioning() {
    return asBool(client_.call("getStatusPositioning"));
}

bool NpDrive::holdPosition(int channel, double targetMeters, int amplitude, int timeoutSeconds) {
    validateChannel(channel);
    requireRange("amplitude", amplitude, limits::kAmplitudeMinV, limits::kAmplitudeMaxV);
    requireRange("target", targetMeters, limits::kTargetMinM, limits::kTargetMaxM);
    if (timeoutSeconds < 1) throw std::out_of_range("timeout must be >= 1 second");
    return asBool(client_.call("holdPosition", {channel, targetMeters, amplitude, timeoutSeconds}));
}

// ---- General ----

bool NpDrive::stopMotion() {
    return asBool(client_.call("stopMotion"));
}

bool NpDrive::setDriveChannel(int channel) {
    validateChannel(channel);
    return asBool(client_.call("setDriveChannel", {channel}));
}

bool NpDrive::setDriveChannelsOff() {
    return asBool(client_.call("setDriveChannelsOff"));
}

int NpDrive::getDriveChannel() {
    return asInt(client_.call("getDriveChannel"));
}

bool NpDrive::getStatusDriveBusy() {
    return asBool(client_.call("getStatusDriveBusy"));
}

bool NpDrive::getStatusDriveOverload() {
    return asBool(client_.call("getStatusDriveOverload"));
}

}  // namespace npdrive
