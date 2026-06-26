// smoke_test.cpp
//
// Offline build/run verification of the parts that don't need hardware:
//   * parameter range validation throws before any RPC is issued
//   * meter <-> micron conversion in the positioner abstraction
// Plus an OPT-IN live test: set env NPDRIVE_HOST=<ip> to exercise a real
// controller (read-only calls: getDriveChannel, status).

#define _CRT_SECURE_NO_WARNINGS  // std::getenv is fine for this opt-in test path

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "npdrive/np_drive.h"
#include "npdrive/positioner.h"

using namespace npdrive;

static int g_failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::cerr << "FAIL: " << #cond << " (" << __FILE__ << ":"     \
                      << __LINE__ << ")\n";                               \
            ++g_failures;                                                 \
        }                                                                 \
    } while (0)

template <typename F>
static bool throws(F&& f) {
    try {
        f();
    } catch (...) {
        return true;
    }
    return false;
}

// Minimal IPositionerAxis to test the unit-conversion wrappers without a device.
class FakeAxis : public IPositionerAxis {
public:
    double lastTargetMeters = 0;
    double lastDeltaMeters = 0;
    double positionMeters = 0;

    bool hasFeedback() const override { return true; }
    void moveAbsoluteMeters(double m, bool) override { lastTargetMeters = m; }
    void moveRelativeMeters(double m, bool) override { lastDeltaMeters = m; }
    double getPositionMeters() override { return positionMeters; }
    void stop() override {}
    AxisState status() override { return AxisState::Idle; }
    bool waitForIdle(std::chrono::milliseconds) override { return true; }
};

static void testValidation() {
    JsonRpcClient client;  // intentionally not connected
    NpDrive drive(client);

    // Range checks fire before any socket use, so an unconnected client is fine.
    CHECK(throws([&] { drive.goStepsForward(99, 10, 100, 1000); }));   // bad channel
    CHECK(throws([&] { drive.goStepsForward(1, 10, 5, 1000); }));      // amplitude < 10
    CHECK(throws([&] { drive.goStepsForward(1, 10, 400, 1000); }));    // amplitude > 300
    CHECK(throws([&] { drive.goStepsForward(1, 10, 100, 50); }));      // freq < 100
    CHECK(throws([&] { drive.goStepsForward(1, 10, 100, 5000); }));    // freq > 4000
    CHECK(throws([&] { drive.goStepsForward(1, -1, 100, 1000); }));    // negative steps
    CHECK(throws([&] { drive.goPosition(1, 2.0, 100, 1000); }));       // target > 1 m
    CHECK(throws([&] { drive.holdPosition(1, 0.0, 100, 0); }));        // timeout < 1
}

static void testUnitConversion() {
    FakeAxis axis;
    axis.moveAbsoluteMicrons(250.0);
    CHECK(std::abs(axis.lastTargetMeters - 250e-6) < 1e-15);

    axis.moveRelativeMicrons(-10.0);
    CHECK(std::abs(axis.lastDeltaMeters + 10e-6) < 1e-15);

    axis.positionMeters = 1.2345e-3;  // 1234.5 um
    CHECK(std::abs(axis.getPositionMicrons() - 1234.5) < 1e-9);
}

static void testOpenLoopRejectsMetricRelative() {
    JsonRpcClient client;
    NpDrive drive(client);
    NpDriveAxis openLoop(drive, 1, Model::A100);
    CHECK(!openLoop.hasFeedback());
    // A100 has no distance feedback: a metric relative move must be rejected,
    // not silently mishandled.
    CHECK(throws([&] { openLoop.moveRelativeMeters(1e-6); }));
    CHECK(throws([&] { openLoop.getPositionMeters(); }));
}

static void liveTestIfRequested() {
    const char* host = std::getenv("NPDRIVE_HOST");
    if (!host) {
        std::cout << "[live] skipped (set NPDRIVE_HOST=<ip> to enable)\n";
        return;
    }
    try {
        JsonRpcClient client;
        client.connect(host, 6002);
        NpDrive drive(client);
        std::cout << "[live] connected to " << host << "\n";
        std::cout << "[live] getDriveChannel  = " << drive.getDriveChannel() << "\n";
        std::cout << "[live] driveBusy        = " << drive.getStatusDriveBusy() << "\n";
        std::cout << "[live] driveOverload    = " << drive.getStatusDriveOverload() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[live] FAILED: " << e.what() << "\n";
        ++g_failures;
    }
}

int main() {
    testValidation();
    testUnitConversion();
    testOpenLoopRejectsMetricRelative();
    liveTestIfRequested();

    if (g_failures == 0) {
        std::cout << "ALL OFFLINE TESTS PASSED\n";
        return 0;
    }
    std::cout << g_failures << " CHECK(S) FAILED\n";
    return 1;
}
