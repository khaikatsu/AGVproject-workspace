#include <unity.h>
#include "core/CoreProtocol.h"
#include "core/RoutePlanner.h"
#include "core/CoreNavigator.h"
#include "core/MutexClearance.h"
#include <cstring>
#include <string>
#include <limits>

void setUp() {}
void tearDown() {}

static bool decode(const char* json, CoreCommand& command) {
    return CoreProtocol::decodeCommand(reinterpret_cast<const uint8_t*>(json), std::strlen(json), command);
}
// Existing route tests may finish a newly captured left turn before asserting
// the subsequent translation. Capture/STOP details have dedicated tests below.
static MotorAction updateAfterTurnCapture(CoreNavigator& nav, ControlInput& in) {
    auto action = nav.update(in);
    if (!nav.capturingTurnLine()) return action;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(action));
    in.linePattern = 2; in.now += Config::LEFT_CAPTURE_SAMPLE_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
    if (nav.capturingTurnLine()) {
        in.now += Config::LEFT_CAPTURE_SAMPLE_MS;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
    }
    ++in.now;
    return nav.update(in);
}

void test_core_commands() {
    CoreCommand command;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":42,"path":["M1","GS1"],"target":"GS1"})", command));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(CoreCommandType::Goto), static_cast<int>(command.type));
    TEST_ASSERT_EQUAL_INT(42, command.cmdId);
    TEST_ASSERT_EQUAL_STRING("M1", command.path[0]);
    TEST_ASSERT_EQUAL_STRING("GS1", command.target);
    CoreCommand original = command, replay;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":42,"path":["M1","GS1"],"target":"GS1"})", replay));
    TEST_ASSERT_TRUE(CoreProtocol::sameCommand(original, replay));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":42,"path":["G1","GS1"],"target":"GS1"})", replay));
    TEST_ASSERT_FALSE(CoreProtocol::sameCommand(original, replay));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"STOP","cmd_id":43})", command));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"WAIT","cmd_id":44})", command));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"CHARGE_START","cmd_id":45})", command));
}

void test_bad_commands_stay_rejected() {
    CoreCommand command;
    TEST_ASSERT_FALSE(decode(R"({"command":"TASK","cmd_id":46})", command));
    TEST_ASSERT_FALSE(decode(R"({"cmd":"GOTO","cmd_id":0,"path":["M1"],"target":"M1"})", command));
    TEST_ASSERT_FALSE(decode(R"({"cmd":"GOTO","cmd_id":47,"path":["M1"],"target":"G1"})", command));
    TEST_ASSERT_FALSE(decode("{\"cmd\":\"STOP\",\"cmd_id\":49} x", command));
}

void test_grant_owns_payload_before_ack_publish() {
    char wire[128]=R"({"zone":"M2","grant_id":57360})";
    CoreGrant grant;
    TEST_ASSERT_TRUE(CoreProtocol::decodeGrant(reinterpret_cast<uint8_t*>(wire),std::strlen(wire),grant));
    std::memset(wire,'X',sizeof(wire)); // PubSubClient ACK reuses the incoming packet storage.
    TEST_ASSERT_EQUAL_STRING("M2",grant.zone);TEST_ASSERT_EQUAL_INT(57360,grant.grantId);
    char ack[96];TEST_ASSERT_TRUE(CoreProtocol::grantAck(grant,ack,sizeof(ack)));
    TEST_ASSERT_EQUAL_STRING(R"({"zone":"M2","grant_id":57360})",ack);
    TEST_ASSERT_FALSE(CoreProtocol::grantAck(grant,ack,8));
    const char* invalid[]={R"({"zone":"M2XXXXXX","grant_id":1})",R"({"zone":"M7","grant_id":1})",
        R"({"zone":"M2\u0000X","grant_id":1})",R"({"zone":"M2","grant_id":0})",
        R"({"zone":"M2","grant_id":2147483648})",R"({"zone":"M2","grant_id":1}x)"};
    for (const auto* payload:invalid)
        TEST_ASSERT_FALSE(CoreProtocol::decodeGrant(reinterpret_cast<const uint8_t*>(payload),std::strlen(payload),grant));
}

void test_core_wire_formats() {
    char wire[64];
    TEST_ASSERT_TRUE(CoreProtocol::telemetry("UNKNOWN", 85, false, wire, sizeof(wire)));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN|85|0", wire);
    TEST_ASSERT_TRUE(CoreProtocol::telemetry("S1", -1, false, wire, sizeof(wire)));
    TEST_ASSERT_EQUAL_STRING("S1|?|0", wire);
    TEST_ASSERT_TRUE(CoreProtocol::ack(42, wire, sizeof(wire)));
    TEST_ASSERT_EQUAL_STRING("42", wire);
}

void test_bench_voltage_interlock() {
    if (Config::TWO_ROBOT_BENCH_TRIAL) {
        TEST_ASSERT_FALSE(Config::benchVoltageReady(0.71f));
        TEST_ASSERT_FALSE(Config::benchVoltageReady(5.23f));
        TEST_ASSERT_TRUE(Config::benchVoltageReady(Config::BATTERY_EMPTY_VOLTAGE));
        TEST_ASSERT_TRUE(Config::benchVoltageReady(Config::BENCH_MAX_VOLTAGE));
        TEST_ASSERT_FALSE(Config::benchVoltageReady(Config::BATTERY_EMPTY_VOLTAGE - 0.01f));
        TEST_ASSERT_FALSE(Config::benchVoltageReady(Config::BENCH_MAX_VOLTAGE + 0.01f));
        TEST_ASSERT_FALSE(Config::benchVoltageReady(std::numeric_limits<float>::quiet_NaN()));
        TEST_ASSERT_FALSE(Config::benchVoltageReady(std::numeric_limits<float>::infinity()));
        TEST_ASSERT_FALSE(Config::benchVoltageReady(-std::numeric_limits<float>::infinity()));
#if defined(AGV_PROFILE_ID)
        TEST_ASSERT_EQUAL_INT(3, Config::BATTERY_SERIES_CELLS);
        TEST_ASSERT_FALSE(Config::benchVoltageReady(7.4f));
        TEST_ASSERT_FALSE(Config::benchVoltageReady(9.0f));
        TEST_ASSERT_TRUE(Config::benchVoltageReady(12.5f));
#else
        TEST_ASSERT_EQUAL_INT(2, Config::BATTERY_SERIES_CELLS);
        TEST_ASSERT_TRUE(Config::benchVoltageReady(7.4f));
        TEST_ASSERT_FALSE(Config::benchVoltageReady(9.0f));
        TEST_ASSERT_FALSE(Config::benchVoltageReady(12.5f));
#endif
    }
}

void test_motion_lock_reports_actual_condition_and_preserves_fault() {
    AGVMath::MotionStartConditions in;
    in.enabled = in.routeAllowed = in.calibrated = in.reverseAllowed = true;
    in.voltage = Config::BATTERY_FULL_VOLTAGE;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None),
                          static_cast<int>(AGVMath::motionStartError(in)));
    if (Config::TWO_ROBOT_BENCH_TRIAL) {
        for (float bad : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity()}) {
            in.voltage = bad;
            TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::BatteryNotReady),
                                  static_cast<int>(AGVMath::motionStartError(in)));
        }
        in.voltage = Config::BATTERY_EMPTY_VOLTAGE - 0.1f;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::BatteryUnderVoltage),
                              static_cast<int>(AGVMath::motionStartError(in)));
        in.voltage = Config::BENCH_MAX_VOLTAGE + 0.02f;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::BatteryOverVoltage),
                              static_cast<int>(AGVMath::motionStartError(in)));
        for (float boundary : {Config::BATTERY_EMPTY_VOLTAGE, Config::BENCH_MAX_VOLTAGE}) {
            in.voltage = boundary;
            TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None),
                                  static_cast<int>(AGVMath::motionStartError(in)));
        }
    }
    in.voltage = Config::BATTERY_FULL_VOLTAGE;
    in.enabled = false;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::MotionDisabled),
                          static_cast<int>(AGVMath::motionStartError(in)));
    in.enabled = true; in.reverseAllowed = false;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::ReverseSteeringUnconfirmed),
                          static_cast<int>(AGVMath::motionStartError(in)));
    in.reverseAllowed = true; in.calibrationRequired = true; in.calibrated = false;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::BatteryCalibrationRequired),
                          static_cast<int>(AGVMath::motionStartError(in)));
    in.calibrated = true; in.previousError = ErrorCode::LineLost;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::LineLost),
                          static_cast<int>(AGVMath::motionStartError(in)));
    in.previousError = ErrorCode::MotionLocked;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::MotionLocked),
                          static_cast<int>(AGVMath::motionStartError(in)));
    TEST_ASSERT_EQUAL_STRING("BATTERY_UNDERVOLTAGE", errorName(ErrorCode::BatteryUnderVoltage));
    TEST_ASSERT_EQUAL_STRING("BATTERY_OVERVOLTAGE", errorName(ErrorCode::BatteryOverVoltage));
    TEST_ASSERT_EQUAL_STRING("BATTERY_ADC_NOT_READY", errorName(ErrorCode::BatteryNotReady));
#if defined(AGV_ROBOT_PROFILE_01) && AGV_ROBOT_PROFILE_01 == 1
    TEST_ASSERT_EQUAL_STRING("01", Config::AGV_ID);
#elif defined(AGV_ROBOT_PROFILE_02) && AGV_ROBOT_PROFILE_02 == 1
    TEST_ASSERT_EQUAL_STRING("02", Config::AGV_ID);
#endif
}

void test_battery_adc_to_percentage_and_motion_gate() {
#if defined(AGV_PROFILE_ID)
#if defined(AGV_ROBOT_PROFILE_02) && AGV_ROBOT_PROFILE_02 == 1
    // Mau ADC nghi thuc te, dong ho cung luc 12.5 V; nguong khoa giu nguyen.
    constexpr uint32_t referenceAdcMv = 2388;
    constexpr float referenceVoltage = 12.5f, referencePercentage = 95.8333f;
    for (uint32_t mv : {2383u, 2388u, 2395u}) {
        const float measured = AGVMath::batteryVoltage(mv, Config::BATTERY_VOLTAGE_SCALE,
                                                     Config::BATTERY_VOLTAGE_OFFSET);
        TEST_ASSERT_FLOAT_WITHIN(0.05f, 12.5f, measured);
        TEST_ASSERT_TRUE(Config::benchVoltageReady(measured));
    }
    TEST_ASSERT_FALSE(Config::benchVoltageReady(AGVMath::batteryVoltage(
        2500, Config::BATTERY_VOLTAGE_SCALE, Config::BATTERY_VOLTAGE_OFFSET)));
#elif defined(AGV_ROBOT_PROFILE_01) && AGV_ROBOT_PROFILE_01 == 1
    // Xe01: dong ho 11.9 V khi bat; ADC nghi 2416..2427 mV, TB lam tron 2421.
    constexpr uint32_t referenceAdcMv = 2421;
    constexpr float referenceVoltage = 11.9f, referencePercentage = 70.8333f;
    for (uint32_t mv : {2416u, 2421u, 2427u}) {
        const float measured = AGVMath::batteryVoltage(mv, Config::BATTERY_VOLTAGE_SCALE,
                                                     Config::BATTERY_VOLTAGE_OFFSET);
        TEST_ASSERT_FLOAT_WITHIN(0.05f, referenceVoltage, measured);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None),
                              static_cast<int>(AGVMath::motionVoltageError(measured)));
    }
    // He so cu gay qua ap gia; he so moi van phai chan qua ap that.
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::BatteryOverVoltage),
                          static_cast<int>(AGVMath::motionVoltageError(
                              AGVMath::batteryVoltage(referenceAdcMv, 12.5f / 2.3f, 0.0f))));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::BatteryOverVoltage),
                          static_cast<int>(AGVMath::motionVoltageError(AGVMath::batteryVoltage(
                              2700, Config::BATTERY_VOLTAGE_SCALE, Config::BATTERY_VOLTAGE_OFFSET))));
#else
    // Gia tri du phong cho build khong chon profile 01/02.
    constexpr uint32_t referenceAdcMv = 2300;
    constexpr float referenceVoltage = 12.5f, referencePercentage = 95.8333f;
#endif
    const float voltage = AGVMath::batteryVoltage(referenceAdcMv, Config::BATTERY_VOLTAGE_SCALE,
                                                Config::BATTERY_VOLTAGE_OFFSET);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, referenceVoltage, voltage);
    TEST_ASSERT_TRUE(Config::benchVoltageReady(voltage));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, referencePercentage, AGVMath::batteryPercentage(
        voltage, Config::BATTERY_EMPTY_VOLTAGE, Config::BATTERY_FULL_VOLTAGE));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, AGVMath::batteryPercentage(
        11.4f, Config::BATTERY_EMPTY_VOLTAGE, Config::BATTERY_FULL_VOLTAGE));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, AGVMath::batteryPercentage(
        9.0f, Config::BATTERY_EMPTY_VOLTAGE, Config::BATTERY_FULL_VOLTAGE));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, AGVMath::batteryPercentage(
        12.9f, Config::BATTERY_EMPTY_VOLTAGE, Config::BATTERY_FULL_VOLTAGE));
    TEST_ASSERT_FALSE(Config::benchVoltageReady(AGVMath::batteryVoltage(
        0, Config::BATTERY_VOLTAGE_SCALE, Config::BATTERY_VOLTAGE_OFFSET)));
#else
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.4f, AGVMath::batteryVoltage(
        1480, Config::BATTERY_VOLTAGE_SCALE, Config::BATTERY_VOLTAGE_OFFSET));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, AGVMath::batteryPercentage(
        7.4f, Config::BATTERY_EMPTY_VOLTAGE, Config::BATTERY_FULL_VOLTAGE));
#endif
}

void test_mutex_clearance_requires_measured_next_tag() {
    MutexClearance clearance;
    MutexClearance::Release release;
    clearance.grant("M1", 91, "G1", true);
    clearance.reached("G1", "M1");
    TEST_ASSERT_FALSE(clearance.pending(release));
    clearance.reached("M1", "G2"); // 35 cm; earliest read leaves only 7 cm.
    TEST_ASSERT_FALSE(clearance.pending(release));
    clearance.reached("G2", "M5"); // Verified straight 35 + 35 cm.
    TEST_ASSERT_TRUE(clearance.pending(release));
    TEST_ASSERT_EQUAL_STRING("M1", release.zone);
    TEST_ASSERT_EQUAL_INT(91, release.grantId);
    clearance.queued(release);
    TEST_ASSERT_FALSE(clearance.pending(release));
    TEST_ASSERT_FALSE(clearance.canAcceptGrant("M1", 91));
    clearance.reset();
    TEST_ASSERT_FALSE(clearance.canAcceptGrant("M1", 91));
    TEST_ASSERT_TRUE(clearance.canAcceptGrant("M1", 94));

    clearance.grant("M4", 92, "M4", true);
    clearance.reached("M4", "GS2"); // 50 cm measured exit.
    TEST_ASSERT_TRUE(clearance.pending(release));
    TEST_ASSERT_EQUAL_STRING("M4", release.zone);
    TEST_ASSERT_EQUAL_INT(92, release.grantId);
    clearance.reset(); // Disconnect keeps Core ownership; no local replay.
    TEST_ASSERT_FALSE(clearance.pending(release));
}

void test_mutex_clearance_rejects_unvisited_or_wrong_route() {
    MutexClearance clearance;
    MutexClearance::Release release;
    clearance.grant("M6", 93, "G5", true);
    clearance.reached("M6", "G6");
    TEST_ASSERT_FALSE(clearance.pending(release));
    clearance.reached("G5", "M6");
    clearance.reached("M6", "G4");
    TEST_ASSERT_FALSE(clearance.pending(release));
    clearance.reached("G4", "K4"); // A turn is not the measured straight chain.
    TEST_ASSERT_FALSE(clearance.pending(release));
    clearance.reached("M6", "G6");
    TEST_ASSERT_TRUE(clearance.pending(release));
    TEST_ASSERT_EQUAL_STRING("M6", release.zone);
    TEST_ASSERT_EQUAL_INT(93, release.grantId);
}

void test_mutex_encoder_release_requires_both_wheels_tag_loss_and_safe_route() {
    if (!Config::MUTEX_ENCODER_RELEASE_ENABLED) return;
    TEST_ASSERT_EQUAL_FLOAT(17.0f, Config::ROBOT_LENGTH_CM);
    TEST_ASSERT_EQUAL_FLOAT(18.5f, Config::MUTEX_RELEASE_FROM_FIRST_READ_CM);
    TEST_ASSERT_EQUAL_UINT32(30, Config::MUTEX_RELEASE_MIN_PULSES);

    MutexClearance clearance;
    MutexClearance::Release release;
    clearance.grant("M2", 501, "G1", true);
    clearance.reached("G1", "M2", 100, 200, 1000);
    clearance.seen("M2", 1050); // A repeated read restarts tag-loss confirmation.
    clearance.progress(130, 230, 1149, true);
    TEST_ASSERT_FALSE(clearance.pending(release));
    clearance.progress(130, 229, 1150, true); // Right wheel is still one pulse short.
    TEST_ASSERT_FALSE(clearance.pending(release));
    clearance.progress(130, 230, 1150, false); // STOP/fault/turn cannot release.
    TEST_ASSERT_FALSE(clearance.pending(release));
    clearance.progress(130, 230, 1150, true);
    TEST_ASSERT_TRUE(clearance.pending(release));
    TEST_ASSERT_EQUAL_STRING("M2", release.zone);
    TEST_ASSERT_EQUAL_INT(501, release.grantId);
    TEST_ASSERT_TRUE(release.encoderProof);
    TEST_ASSERT_EQUAL_UINT32(30, release.leftPulses);
    TEST_ASSERT_EQUAL_UINT32(30, release.rightPulses);
    clearance.queued(release);

    // Lifetime translation counters and subtraction remain correct at wrap.
    clearance.grant("M3", 502, "M5", true);
    clearance.reached("M5", "M3", UINT32_MAX - 9, UINT32_MAX - 9, 2000);
    clearance.progress(20, 20, 2100, true);
    TEST_ASSERT_TRUE(clearance.pending(release));
    TEST_ASSERT_EQUAL_STRING("M3", release.zone);
    TEST_ASSERT_EQUAL_INT(502, release.grantId);
}

void test_mutex_exit_release_continues_without_reacquiring_current_zone() {
    if (!Config::MUTEX_ENCODER_RELEASE_ENABLED) return;
    const char* trips[][3] = {
        {"G1", "M1", "GS1"}, {"GS1", "M2", "G6"},
        {"GS2", "M3", "G1"}, {"G6", "M4", "GS2"},
        {"G1", "M5", "M6"}, {"M5", "M6", "G6"}
    };
    for (const auto& trip : trips) {
        CoreCommand c; c.type=CoreCommandType::Goto; c.cmdId=610;
        c.pathLength=2; c.passThrough[0]=true;
        std::strcpy(c.path[0],trip[1]); std::strcpy(c.path[1],trip[2]); std::strcpy(c.target,trip[2]);
        RoutePlan plan;
        TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,trip[0],c,plan,false,
                                           RoutePlanner::edgeHeading(trip[0],trip[1])));
        CoreNavigator nav; TEST_ASSERT_TRUE(nav.start(trip[0],c,plan,0));
        nav.grant(trip[0],600); TEST_ASSERT_TRUE(nav.grant(trip[1],601));
        ControlInput in; in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
        in.linePattern=2; in.now=Config::RFID_STOP_DWELL_MS+1;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
        if (trip[2][0]=='M') TEST_ASSERT_TRUE(nav.grant(trip[2],602));
        TEST_ASSERT_TRUE(nav.canPassThrough(trip[1],in)); nav.reachedNode(trip[1],++in.now,&in);
        MutexClearance clearance; clearance.grant(trip[1],601,trip[0],true);
        clearance.reached(trip[0],trip[1],100,100,in.now);
        in.now+=Config::MUTEX_TAG_LOST_CONFIRM_MS;
        clearance.progress(130,130,in.now,true); MutexClearance::Release release;
        TEST_ASSERT_TRUE(clearance.pending(release));
        TEST_ASSERT_FALSE(nav.completeZoneExit(release.zone,600)); // Wrong token cannot record clearance.
        TEST_ASSERT_TRUE(nav.completeZoneExit(release.zone,release.grantId));
        clearance.queued(release);
        TEST_ASSERT_FALSE(clearance.canAcceptGrant(release.zone,release.grantId));
        TEST_ASSERT_FALSE(nav.completeZoneExit(release.zone,release.grantId));
        TEST_ASSERT_FALSE(nav.grant(release.zone,603)); // Late grant must not reclaim the cleared zone.
        TEST_ASSERT_NULL(nav.missingZone()); TEST_ASSERT_NULL(nav.requestedZone());
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
        MotorAction early=MotorAction::Stop;
        TEST_ASSERT_TRUE(nav.refreshFollowing(in,early));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(early));
        TEST_ASSERT_EQUAL_UINT8(1,nav.pathIndex()); TEST_ASSERT_EQUAL_INT(610,nav.routeCommandId());
    }
}

void test_mutex_exit_clearance_keeps_next_zone_and_pass_through_checks() {
    CoreCommand c; c.type=CoreCommandType::Goto; c.cmdId=620; c.pathLength=3;
    std::strcpy(c.path[0],"M5"); std::strcpy(c.path[1],"M6"); std::strcpy(c.path[2],"G6");
    std::strcpy(c.target,"G6"); c.passThrough[0]=c.passThrough[1]=true;
    RoutePlan plan; TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"G1",c,plan,false,RobotHeading::East));
    CoreNavigator nav; TEST_ASSERT_TRUE(nav.start("G1",c,plan,0)); TEST_ASSERT_TRUE(nav.grant("M5",621));
    ControlInput in; in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
    in.linePattern=2; in.now=Config::RFID_STOP_DWELL_MS+1; nav.update(in);
    TEST_ASSERT_TRUE(nav.grant("M6",622));
    TEST_ASSERT_TRUE(nav.canPassThrough("M5",in)); nav.reachedNode("M5",++in.now,&in);
    TEST_ASSERT_TRUE(nav.completeZoneExit("M5",621));
    nav.revokeGrant("M6",622);
    TEST_ASSERT_EQUAL_STRING("M6",nav.missingZone()); TEST_ASSERT_EQUAL_STRING("M6",nav.requestedZone());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_FALSE(nav.canPassThrough("M6",in));
    TEST_ASSERT_TRUE(nav.grant("M6",623));
    TEST_ASSERT_TRUE(nav.canPassThrough("M6",in)); // Cleared M5 does not force a dwell at M6.
    nav.reachedNode("M6",++in.now,&in);
    nav.revokeGrant("M6",623); // Clearance for M5 must not apply to M6.
    TEST_ASSERT_EQUAL_STRING("M6",nav.missingZone());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
}

void test_mutex_exit_clearance_resets_on_new_route_fault_and_session() {
    for (unsigned transition=0;transition<3;++transition) {
        CoreCommand c; c.type=CoreCommandType::Goto; c.cmdId=630; c.pathLength=2;
        std::strcpy(c.path[0],"M3"); std::strcpy(c.path[1],"G1"); std::strcpy(c.target,"G1");
        c.passThrough[0]=true; RoutePlan plan;
        TEST_ASSERT_TRUE(RoutePlanner::plan("M4","GS2",c,plan));
        CoreNavigator nav; TEST_ASSERT_TRUE(nav.start("GS2",c,plan,0)); TEST_ASSERT_TRUE(nav.grant("M3",631));
        ControlInput in; in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
        in.linePattern=2; in.now=Config::RFID_STOP_DWELL_MS+1; nav.update(in);
        nav.reachedNode("M3",++in.now,&in); TEST_ASSERT_TRUE(nav.completeZoneExit("M3",631));
        in.obstacle=true;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        in.obstacle=false; ++in.now;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
        if(transition==0) nav.stop();
        if(transition==1) { in.mqtt=false; nav.update(in); TEST_ASSERT_TRUE(nav.clearFault()); }
        if(transition==2) { nav.resetGrants(); TEST_ASSERT_TRUE(nav.clearFault()); }
        c.cmdId=632; c.pathLength=1; c.passThrough[0]=false;
        std::strcpy(c.path[0],"G1"); std::strcpy(c.target,"G1");
        TEST_ASSERT_TRUE(RoutePlanner::plan("GS2","M3",c,plan));
        TEST_ASSERT_TRUE(nav.start("M3",c,plan,in.now));
        TEST_ASSERT_EQUAL_STRING("M3",nav.missingZone()); TEST_ASSERT_EQUAL_STRING("M3",nav.requestedZone());
        TEST_ASSERT_TRUE(nav.grant("M3",633));
    }
}

void test_directed_route_turns() {
    TEST_ASSERT_TRUE(RoutePlanner::directedEdge("M1", "G2"));
    TEST_ASSERT_FALSE(RoutePlanner::directedEdge("M5", "G2"));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Right), static_cast<int>(RoutePlanner::turn("G1", "M1", "G2")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight), static_cast<int>(RoutePlanner::turn("G1", "M1", "GS1")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left), static_cast<int>(RoutePlanner::turn("G2", "M5", "M6")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Right), static_cast<int>(RoutePlanner::turn("G5", "M6", "G6")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Reverse), static_cast<int>(RoutePlanner::turn("G2", "K2", "G2")));

    CoreCommand command;
    RoutePlan plan;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":50,"path":["M5","M6","G6"],"target":"G6"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan("M1", "G2", command, plan));
    TEST_ASSERT_EQUAL_INT(3, plan.count);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight), static_cast<int>(plan.departure[0]));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left), static_cast<int>(plan.departure[1]));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight), static_cast<int>(plan.departure[2]));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr, "G2", command, plan));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Unknown), static_cast<int>(plan.departure[0]));
    TEST_ASSERT_FALSE(RoutePlanner::plan("M5", "G2", command, plan));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":51,"path":["G2"],"target":"G2"})", command));
    TEST_ASSERT_FALSE(RoutePlanner::plan(nullptr, "M5", command, plan));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":52,"path":["GS1","M2"],"target":"M2"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr, "S1", command, plan));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight), static_cast<int>(plan.departure[0]));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left), static_cast<int>(plan.departure[1]));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":58,"path":["GS2"],"target":"GS2"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr, "S2", command, plan));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight), static_cast<int>(plan.departure[0]));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":53,"path":["M2"],"target":"M2"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan("S1", "GS1", command, plan, true));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Right), static_cast<int>(plan.departure[0]));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":55,"path":["GS1"],"target":"GS1"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan("G1", "M1", command, plan, true));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::TurnAround), static_cast<int>(plan.departure[0]));
}

void test_fixed_bay_heading_and_reverse_exit() {
    CoreCommand command;
    TEST_ASSERT_FALSE(decode(R"({"cmd":"SET_HEADING","cmd_id":69,"node":"S1","heading":"E"})",command));
    const char* bays[]={"S1","S2","K1","K2","K3","K4","K5","K6"};
    const RobotHeading heads[]={RobotHeading::South,RobotHeading::North,
        RobotHeading::West,RobotHeading::West,RobotHeading::West,
        RobotHeading::East,RobotHeading::East,RobotHeading::East};
    for (unsigned i=0;i<8;++i) {
        const char* gate=RoutePlanner::bayGate(bays[i]);
        const bool station=RoutePlanner::station(bays[i]);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(heads[i]),static_cast<int>(RoutePlanner::bayHeading(bays[i])));
        command=CoreCommand{};command.type=CoreCommandType::Goto;command.pathLength=1;
        std::strcpy(command.path[0],gate);std::strcpy(command.target,gate);
        RoutePlan route;
        // Fixed default applies even if a stale incoming/explicit heading says otherwise.
        TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,bays[i],command,route));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(station ? RouteTurn::Straight : RouteTurn::Reverse),static_cast<int>(route.departure[0]));
        TEST_ASSERT_TRUE(RoutePlanner::reverseAllowed(bays[i],command,route,false,true));
        TEST_ASSERT_EQUAL(station,RoutePlanner::reverseAllowed(bays[i],command,route,false,false));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(heads[i]),static_cast<int>(RoutePlanner::edgeHeading(bays[i],gate,!station)));
        CoreNavigator nav;TEST_ASSERT_TRUE(nav.start(bays[i],command,route,0));
        ControlInput input;input.wifi=input.mqtt=input.motionConfigured=input.lineConfigured=input.distanceValid=true;
        input.linePattern=2;input.now=Config::RFID_STOP_DWELL_MS+1;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(station ? MotorAction::Forward : MotorAction::Backward),static_cast<int>(nav.update(input)));
        nav.reachedNode(gate,input.now+1);TEST_ASSERT_FALSE(nav.active());
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(input)));
        std::strcpy(command.path[0],bays[i]);std::strcpy(command.target,bays[i]);
        TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,gate,command,route,false,RoutePlanner::edgeHeading(gate,bays[i])));
        if (station && Config::REVERSE_HOME_SUPPORTED) {
            TEST_ASSERT_TRUE(route.reverseAfterTurn[0]);
            TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::TurnAround),static_cast<int>(route.departure[0]));
            continue; // Rotation and backward translation covered by reverse-home test.
        }
        TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight),static_cast<int>(route.departure[0]));
        TEST_ASSERT_TRUE(nav.start(gate,command,route,input.now));input.now+=Config::RFID_STOP_DWELL_MS+1;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(input)));
    }
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RobotHeading::Unknown),static_cast<int>(RoutePlanner::bayHeading("GS1")));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":70,"path":["GS1","M2"],"target":"M2"})",command));
    RoutePlan route;TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"S1",command,route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight),static_cast<int>(route.departure[0]));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left),static_cast<int>(route.departure[1]));
    route.departure[1]=RouteTurn::Reverse;
    TEST_ASSERT_FALSE(RoutePlanner::reverseAllowed("S1",command,route,false,true));
}

void test_station_arrival_turns_at_station_before_ready() {
    if (Config::REVERSE_HOME_SUPPORTED) return; // Both stations now reverse-dock; legacy profiles keep this test.
    const char* stations[]={"S1","S2"};const char* gates[]={"GS1","GS2"};const char* previous[]={"M1","M4"};
    for (unsigned i=0;i<2;++i) {
        if (i == 0 && Config::REVERSE_HOME_SUPPORTED) continue; // Dedicated GS1 reverse-dock coverage below.
        CoreCommand command;command.type=CoreCommandType::Goto;command.cmdId=73+i;command.pathLength=1;
        std::strcpy(command.target,stations[i]);std::strcpy(command.path[0],stations[i]);
        RoutePlan route;TEST_ASSERT_TRUE(RoutePlanner::plan(previous[i],gates[i],command,route));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left),static_cast<int>(route.departure[0]));
        TEST_ASSERT_TRUE(RoutePlanner::reverseAllowed(gates[i],command,route,false,true));
        TEST_ASSERT_TRUE(RoutePlanner::reverseAllowed(gates[i],command,route,false,false));
        CoreNavigator nav;TEST_ASSERT_TRUE(nav.start(gates[i],command,route,0));
        ControlInput in;in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
        in.now=Config::RFID_STOP_DWELL_MS;in.linePattern=2;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft),static_cast<int>(nav.update(in)));
        in.linePattern=0;in.now+=Config::TURN_MIN_MS+1;nav.update(in);
        in.linePattern=2;++in.now;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
        TEST_ASSERT_TRUE(nav.following());TEST_ASSERT_FALSE(nav.segmentReverse());
        nav.reachedNode(stations[i],in.now);TEST_ASSERT_TRUE(nav.active());
        TEST_ASSERT_FALSE(nav.stationOriented());
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        // A fresh GOTO cannot interrupt parking while the front still faces inward.
        TEST_ASSERT_FALSE(nav.start(stations[i],command,route,in.now));
        in.now+=Config::RFID_STOP_DWELL_MS;in.linePattern=2;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotRight),static_cast<int>(nav.update(in)));
        in.now+=Config::TURN_MIN_MS+1; // Old line is not sufficient for readiness.
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotRight),static_cast<int>(nav.update(in)));
        in.linePattern=0;++in.now;nav.update(in);in.linePattern=2;++in.now;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        TEST_ASSERT_FALSE(nav.active());TEST_ASSERT_TRUE(nav.stationOriented());
        std::strcpy(command.target,gates[i]);std::strcpy(command.path[0],gates[i]);
        TEST_ASSERT_TRUE(RoutePlanner::plan(gates[i],stations[i],command,route,true));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight),static_cast<int>(route.departure[0]));
        TEST_ASSERT_TRUE(nav.start(stations[i],command,route,in.now));in.now+=Config::RFID_STOP_DWELL_MS+1;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    }
    CoreCommand command;TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":76,"path":["S2"],"target":"S2"})",command));
    RoutePlan route;TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"GS2",command,route,false,RobotHeading::South));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight),static_cast<int>(route.departure[0]));
    CoreNavigator timeout;TEST_ASSERT_TRUE(timeout.start("GS2",command,route,0));
    timeout.reachedNode("S2",1);
    ControlInput in;in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
    in.linePattern=2;in.now=1+Config::RFID_STOP_DWELL_MS;timeout.update(in);
    in.now+=Config::TURN_TIMEOUT_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(timeout.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::TurnTimeout),static_cast<int>(timeout.error()));
    TEST_ASSERT_FALSE(timeout.stationOriented());
}

void test_bay_exit_turnaround_counts_map_branches() {
    CoreCommand command;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":71,"path":["G1","M5"],"target":"M5"})",command));
    RoutePlan route;TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"K1",command,route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Reverse),static_cast<int>(route.departure[0]));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::TurnAround),static_cast<int>(route.departure[1]));
    TEST_ASSERT_EQUAL_INT(2,route.turnCrossings[1]); // G1 has a north branch before the desired east line.
    TEST_ASSERT_TRUE(RoutePlanner::reverseAllowed("K1",command,route,false,true));
    CoreNavigator nav;TEST_ASSERT_TRUE(nav.start("K1",command,route,0));nav.reachedNode("G1",100);
    TEST_ASSERT_TRUE(nav.grant("M5",126));
    ControlInput in;in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
    in.now=100+Config::RFID_STOP_DWELL_MS;in.linePattern=2;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotRight),static_cast<int>(nav.update(in)));
    TEST_ASSERT_EQUAL_INT(Config::G1_TURN_AROUND_PWM > 0, nav.precisionPivot());
    if (Config::G1_TURN_AROUND_PWM > 0)
        TEST_ASSERT_EQUAL_INT(Config::G1_TURN_AROUND_PWM, nav.precisionPivotPwm());
    in.linePattern=0;in.now+=Config::TURN_MIN_MS+1;nav.update(in);
    in.linePattern=2;++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotRight),static_cast<int>(nav.update(in)));
    in.now+=Config::TURN_MIN_MS+1; // One continuous centered patch must not count twice.
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotRight),static_cast<int>(nav.update(in)));
    in.linePattern=0;++in.now;nav.update(in);in.linePattern=2;++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    TEST_ASSERT_FALSE(nav.precisionPivot());
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":72,"path":["GS1"],"target":"GS1"})",command));
    TEST_ASSERT_TRUE(RoutePlanner::plan("G1","M1",command,route,true));
    TEST_ASSERT_EQUAL_INT(1,route.turnCrossings[0]); // No north branch at M1; the next line is the opposite line.
    CoreNavigator single;TEST_ASSERT_TRUE(single.start("M1",command,route,0));TEST_ASSERT_TRUE(single.grant("M1",127));
    in.now=Config::RFID_STOP_DWELL_MS;in.linePattern=2;single.update(in);
    TEST_ASSERT_FALSE(single.precisionPivot()); // M1 quay dau van dung PWM cu.
    in.now+=Config::TURN_MIN_MS+1;in.linePattern=0;single.update(in);++in.now;in.linePattern=2;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(single.update(in)));
    CoreNavigator timeout;TEST_ASSERT_TRUE(timeout.start("M1",command,route,0));TEST_ASSERT_TRUE(timeout.grant("M1",128));
    in.now=Config::RFID_STOP_DWELL_MS;in.linePattern=0;timeout.update(in);
    in.now+=Config::TURN_TIMEOUT_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(timeout.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::TurnTimeout),static_cast<int>(timeout.error()));
}

void test_manual_tag_relocation_does_not_claim_reverse_heading() {
    CoreCommand command;
    RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":66,"path":["GS1","M2"],"target":"M2"})", command));
    // Station convention overrides even a stale GS1 -> S1 incoming scan.
    TEST_ASSERT_TRUE(RoutePlanner::plan("GS1", "S1", command, route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight), static_cast<int>(route.departure[0]));
    const char* incoming = RoutePlanner::confirmedIncoming(false, "GS1", "S1");
    TEST_ASSERT_NULL(incoming);
    TEST_ASSERT_TRUE(RoutePlanner::plan(incoming, "S1", command, route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight), static_cast<int>(route.departure[0]));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left), static_cast<int>(route.departure[1]));
    // Genuine expected arrivals must preserve the measured route heading.
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":67,"path":["M2"],"target":"M2"})", command));
    incoming = RoutePlanner::confirmedIncoming(true, "S1", "GS1");
    TEST_ASSERT_EQUAL_STRING("S1", incoming);
    TEST_ASSERT_TRUE(RoutePlanner::plan(incoming, "GS1", command, route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left), static_cast<int>(route.departure[0]));
    // Manual relocation between ordinary tags cannot invent a boot heading.
    TEST_ASSERT_TRUE(RoutePlanner::plan(RoutePlanner::confirmedIncoming(false, "S1", "GS1"),
                                      "GS1", command, route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Unknown), static_cast<int>(route.departure[0]));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":68,"path":["GS2","M3"],"target":"M3"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(RoutePlanner::confirmedIncoming(false, "GS2", "S2"),
                                      "S2", command, route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight), static_cast<int>(route.departure[0]));
}

void test_solo_route_crosses_mutex_without_grant() {
    CoreCommand command;
    RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":56,"path":["GS1","M2"],"target":"M2"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr, "S1", command, route));
    TEST_ASSERT_TRUE(RoutePlanner::oneRobotTrialRoute("01", "S1", command, route));
    TEST_ASSERT_FALSE(RoutePlanner::oneRobotTrialRoute("02", "S1", command, route));
    CoreNavigator normal;
    TEST_ASSERT_TRUE(normal.start("S1", command, route, 0));
    normal.reachedNode("GS1", 202);
    TEST_ASSERT_EQUAL_STRING("M2", normal.missingZone());
    CoreNavigator solo;
    TEST_ASSERT_TRUE(solo.start("S1", command, route, 0, true));
    solo.reachedNode("GS1", 202);
    TEST_ASSERT_NULL(solo.missingZone());
    ControlInput input;
    input.wifi = input.mqtt = input.motionConfigured = input.lineConfigured = input.distanceValid = true;
    input.linePattern = 2;
    input.now = 202 + Config::RFID_STOP_DWELL_MS + 1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft), static_cast<int>(solo.update(input)));
    solo.stop();
    TEST_ASSERT_NULL(solo.missingZone());
}

void test_core_navigator_stops_at_rfid() {
    CoreCommand command;
    RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":54,"path":["GS1"],"target":"GS1"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr, "S1", command, route));
    CoreNavigator nav;
    TEST_ASSERT_TRUE(nav.start("S1", command, route, 0));
    ControlInput input;
    input.wifi = input.mqtt = input.motionConfigured = input.lineConfigured = input.distanceValid = true;
    input.linePattern = 2;
    input.now = Config::RFID_STOP_DWELL_MS - 1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(input)));
    input.now = Config::RFID_STOP_DWELL_MS + 1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(nav.update(input)));
    nav.reachedNode("GS1", 202);
    TEST_ASSERT_EQUAL_INT(AGV_IDLE, nav.state());
}

void test_two_robot_navigation_requires_and_revokes_grant() {
    CoreCommand command;
    RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":62,"path":["M2"],"target":"M2"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan("S1", "GS1", command, route));
    CoreNavigator nav;
    TEST_ASSERT_TRUE(nav.start("GS1", command, route, 0));
    TEST_ASSERT_EQUAL_STRING("M2", nav.missingZone());
    TEST_ASSERT_TRUE(nav.grant("M2", 124));
    TEST_ASSERT_NULL(nav.missingZone());
    nav.revokeGrant("M2", 123); // Old token cannot revoke the current grant.
    TEST_ASSERT_NULL(nav.missingZone());
    nav.revokeGrant("M2", 124);
    TEST_ASSERT_EQUAL_STRING("M2", nav.missingZone());
}

void test_turn_does_not_reacquire_junction_before_blank() {
    CoreCommand command;
    RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":55,"path":["M2"],"target":"M2"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan("S1", "GS1", command, route));
    CoreNavigator nav;
    TEST_ASSERT_TRUE(nav.start("GS1", command, route, 0, true));
    ControlInput input;
    input.wifi = input.mqtt = input.motionConfigured = input.lineConfigured = input.distanceValid = true;
    input.linePattern = 3;
    input.now = Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft), static_cast<int>(nav.update(input)));
    input.linePattern = 7;
    input.now += Config::TURN_MIN_MS + 1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft), static_cast<int>(nav.update(input)));
    input.linePattern = 0;
    ++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft), static_cast<int>(nav.update(input)));
    input.linePattern = 2;
    ++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(updateAfterTurnCapture(nav,input)));
}

void test_robot02_gs2_turn_waits_for_grant_and_stops_on_network_loss() {
    CoreCommand command;
    RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":64,"path":["GS2","M3","G1"],"target":"G1"})",command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"S2",command,route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Straight),static_cast<int>(route.departure[0]));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left),static_cast<int>(route.departure[1]));
    CoreNavigator nav;
    TEST_ASSERT_TRUE(nav.start("S2",command,route,0,false));
    ControlInput input;
    input.wifi=input.mqtt=input.motionConfigured=input.lineConfigured=input.distanceValid=true;
    input.linePattern=2;input.now=Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(input)));
    nav.reachedNode("GS2",++input.now);
    TEST_ASSERT_EQUAL_STRING("M3",nav.missingZone());
    input.now+=Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(input)));
    TEST_ASSERT_TRUE(nav.grant("M3",125));
    input.linePattern=6;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft),static_cast<int>(nav.update(input)));
    input.linePattern=7;input.now+=Config::TURN_MIN_MS+1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft),static_cast<int>(nav.update(input)));
    input.linePattern=0;++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft),static_cast<int>(nav.update(input)));
    input.linePattern=2;++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(updateAfterTurnCapture(nav,input)));
    input.mqtt=false;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(input)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::NotConnected),static_cast<int>(nav.error()));
    TEST_ASSERT_FALSE(nav.active());
    // Reconnect/reset at GS2 does not establish heading without a prior RFID.
    TEST_ASSERT_TRUE(nav.clearFault());
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":65,"path":["M3","G1"],"target":"G1"})",command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"GS2",command,route));
    TEST_ASSERT_FALSE(nav.start("GS2",command,route,input.now,false));
}

void test_trial_gently_recovers_from_small_line_offset() {
    CoreCommand command;
    RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":57,"path":["GS1"],"target":"GS1"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan("G1", "M1", command, route));
    CoreNavigator nav;
    TEST_ASSERT_TRUE(nav.start("M1", command, route, 0, true));
    ControlInput input;
    input.wifi = input.mqtt = input.motionConfigured = input.lineConfigured = input.distanceValid = true;
    input.now = Config::RFID_STOP_DWELL_MS + 1;
    input.linePattern = 6;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::GentleCorrectLeft), static_cast<int>(nav.update(input)));
    TEST_ASSERT_EQUAL_STRING("GENTLE_CORRECT_LEFT", motorActionName(AGVMath::lineAction(6)));
    input.linePattern = 2; ++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(nav.update(input)));
    input.linePattern = 3; ++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::GentleCorrectRight), static_cast<int>(nav.update(input)));
    input.linePattern = 4; ++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::CorrectLeft), static_cast<int>(nav.update(input)));
    input.linePattern = 1; ++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::CorrectRight), static_cast<int>(nav.update(input)));
    input.linePattern = 0; ++input.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(input)));
}

void test_node_order_parser_and_correlation() {
    CoreCommand order, duplicate;
    const char* payload=R"({"cmd":"NODE_ORDER","cmd_id":102,"route_cmd_id":101,"node":"GS1","next_node":"M2","path_index":1,"maneuver":"LEFT","turn_crossings":1})";
    TEST_ASSERT_TRUE(decode(payload,order));TEST_ASSERT_TRUE(decode(payload,duplicate));
    TEST_ASSERT_TRUE(CoreProtocol::sameCommand(order,duplicate));
    duplicate.routeCmdId=100;TEST_ASSERT_FALSE(CoreProtocol::sameCommand(order,duplicate));
    TEST_ASSERT_FALSE(decode(R"({"cmd":"NODE_ORDER","cmd_id":102,"route_cmd_id":102,"node":"GS1","next_node":"M2","path_index":1,"maneuver":"LEFT","turn_crossings":1})",order));
    TEST_ASSERT_FALSE(decode(R"({"cmd":"NODE_ORDER","cmd_id":102,"route_cmd_id":101,"node":"GS1","next_node":"M2","path_index":33,"maneuver":"LEFT","turn_crossings":1})",order));
    TEST_ASSERT_FALSE(decode(R"({"cmd":"NODE_ORDER","cmd_id":102,"route_cmd_id":101,"node":"GS1","next_node":"M2","path_index":1,"maneuver":"LEFT","turn_crossings":0})",order));
    TEST_ASSERT_FALSE(decode(R"({"cmd":"NODE_ORDER","cmd_id":102,"route_cmd_id":101,"node":"GS1","next_node":"M2","path_index":1,"maneuver":"FORWARD","turn_crossings":0})",order));
}

static CoreCommand orderFor(const char* node,const char* next,const char* turn,uint8_t step,int32_t route,uint8_t crossings) {
    CoreCommand c;c.type=CoreCommandType::NodeOrder;c.cmdId=route+step+10;c.routeCmdId=route;
    std::strcpy(c.node,node);std::strcpy(c.nextNode,next);std::strcpy(c.maneuver,turn);
    c.pathIndex=step;c.turnCrossings=crossings;return c;
}

#if defined(AGV_ROBOT_PROFILE_01) && AGV_ROBOT_PROFILE_01 == 1
void test_robot01_blank_line_waits_for_rfid_without_moving() {
    static_assert(Config::LINE_LOSS_CONFIRM_MS == 5000u, "Robot 01 blank-line hold must be five seconds");
    CoreCommand command; RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":160,"path":["GS1","M2"],"target":"M2"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr, "S1", command, route));
    ControlInput in;
    in.wifi = in.mqtt = in.motionConfigured = in.lineConfigured = in.distanceValid = true;
    in.linePattern = 2;
    in.now = Config::RFID_STOP_DWELL_MS;

    CoreNavigator arrival;
    TEST_ASSERT_TRUE(arrival.start("S1", command, route, 0, false, true));
    TEST_ASSERT_TRUE(arrival.authorizeNodeOrder(orderFor("S1", "GS1", "STRAIGHT", 0, 160, 0)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(arrival.update(in)));
    const uint32_t blankAt = in.now + 10;
    in.now = blankAt; in.linePattern = 0;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(arrival.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None), static_cast<int>(arrival.error()));
    in.now = blankAt + Config::LINE_LOSS_CONFIRM_MS - 1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(arrival.update(in)));
    arrival.reachedNode("GS1", in.now); // RFID is still polled while the motor is stopped.
    TEST_ASSERT_TRUE(arrival.needsNodeOrder());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(arrival.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None), static_cast<int>(arrival.error()));

    CoreNavigator timeout;
    TEST_ASSERT_TRUE(timeout.start("S1", command, route, 0, false, true));
    TEST_ASSERT_TRUE(timeout.authorizeNodeOrder(orderFor("S1", "GS1", "STRAIGHT", 0, 160, 0)));
    in.now = Config::RFID_STOP_DWELL_MS; in.linePattern = 2;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(timeout.update(in)));
    in.now = blankAt; in.linePattern = 0;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(timeout.update(in)));
    in.now = blankAt + (Config::LINE_LOSS_GRACE_MS ? Config::LINE_LOSS_GRACE_MS : Config::LINE_LOSS_CONFIRM_MS);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(timeout.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::LineLost), static_cast<int>(timeout.error()));
    TEST_ASSERT_FALSE(timeout.active());

    CoreNavigator recovered;
    TEST_ASSERT_TRUE(recovered.start("S1", command, route, 0, false, true));
    TEST_ASSERT_TRUE(recovered.authorizeNodeOrder(orderFor("S1", "GS1", "STRAIGHT", 0, 160, 0)));
    in.now = Config::RFID_STOP_DWELL_MS; in.linePattern = 2;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(recovered.update(in)));
    in.now = blankAt; in.linePattern = 0;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(recovered.update(in)));
    in.now = blankAt + 100; in.linePattern = 2;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(recovered.update(in)));
    in.now = blankAt + 150;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(recovered.update(in)));
    in.now = blankAt + 200;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(recovered.update(in)));
    in.now = blankAt + 300; in.linePattern = 0;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(recovered.update(in)));
    in.mqtt = false; ++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(recovered.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::NotConnected), static_cast<int>(recovered.error()));
}
#endif

#if defined(AGV_ROBOT_PROFILE_02) && AGV_ROBOT_PROFILE_02 == 1
void test_robot02_blank_line_holds_before_fault() {
    static_assert(Config::LINE_LOSS_GRACE_MS == 7000u, "Robot02 keeps the task for seven seconds on line loss");
    CoreCommand command; RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":161,"path":["GS2"],"target":"GS2"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr, "S2", command, route));
    CoreNavigator nav;
    TEST_ASSERT_TRUE(nav.start("S2", command, route, 0, false, false));
    ControlInput in;
    in.wifi = in.mqtt = in.motionConfigured = in.lineConfigured = in.distanceValid = true;
    in.linePattern = 2;
    in.now = Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(nav.update(in)));
    in.linePattern = 0; ++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None), static_cast<int>(nav.error()));
    in.now += Config::LINE_LOSS_GRACE_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::LineLost), static_cast<int>(nav.error()));
}
#endif

void test_server_node_order_stops_then_pivots_left_with_mutex() {
    CoreCommand command;RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":101,"path":["GS1","M2"],"target":"M2"})",command));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"S1",command,route));
    CoreNavigator nav;TEST_ASSERT_TRUE(nav.start("S1",command,route,0,false,true));
    ControlInput in;in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
    in.linePattern=2;in.now=Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_TRUE(nav.needsNodeOrder());TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    in.now=5000;TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    auto order=orderFor("S1","GS1","STRAIGHT",0,101,0);
    order.routeCmdId=100;TEST_ASSERT_FALSE(nav.authorizeNodeOrder(order));order.routeCmdId=101;
    TEST_ASSERT_TRUE(nav.authorizeNodeOrder(order));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    nav.reachedNode("GS1",++in.now);in.now+=Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_TRUE(nav.needsNodeOrder());TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_FALSE(nav.authorizeNodeOrder(order)); // Old S1 departure cannot run at GS1.
    order=orderFor("GS1","M2","RIGHT",1,101,1);TEST_ASSERT_FALSE(nav.authorizeNodeOrder(order));
    std::strcpy(order.maneuver,"LEFT");TEST_ASSERT_TRUE(nav.authorizeNodeOrder(order));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in))); // Order never bypasses mutex.
    TEST_ASSERT_TRUE(nav.grant("M2",201));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft),static_cast<int>(nav.update(in)));
    in.linePattern=7;in.now+=Config::TURN_MIN_MS+1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft),static_cast<int>(nav.update(in)));
    in.linePattern=0;++in.now;TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft),static_cast<int>(nav.update(in)));
    in.linePattern=2;++in.now;TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(updateAfterTurnCapture(nav,in)));
    TEST_ASSERT_FALSE(nav.authorizeNodeOrder(order)); // Duplicate cannot restart a pivot during following.
    in.mqtt=false;TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
}

void test_server_node_order_right_then_station_turn() {
    if (Config::REVERSE_HOME_SUPPORTED) return; // Legacy firmware contract; reverse-home is tested separately.
    CoreCommand command;RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":111,"path":["S1"],"target":"S1"})",command));
    // A known west-facing front needs a physical right pivot to the north station line.
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"GS1",command,route,false,RobotHeading::West));
    CoreNavigator nav;TEST_ASSERT_TRUE(nav.start("GS1",command,route,0,false,true));
    ControlInput in;in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
    in.linePattern=2;in.now=Config::RFID_STOP_DWELL_MS;
    auto order=orderFor("GS1","S1","RIGHT",0,111,1);TEST_ASSERT_TRUE(nav.authorizeNodeOrder(order));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotRight),static_cast<int>(nav.update(in)));
    in.linePattern=0;in.now+=Config::TURN_MIN_MS+1;nav.update(in);in.linePattern=2;++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    nav.reachedNode("S1",in.now);in.now+=Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_TRUE(nav.stationTurnRequired());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    order=orderFor("S1","S1","STATION_TURN_RIGHT",1,111,1);TEST_ASSERT_TRUE(nav.authorizeNodeOrder(order));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotRight),static_cast<int>(nav.update(in)));
    in.linePattern=0;in.now+=Config::TURN_MIN_MS+1;nav.update(in);in.linePattern=2;++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_FALSE(nav.active());TEST_ASSERT_TRUE(nav.stationOriented());
    CoreNavigator timeout;TEST_ASSERT_TRUE(timeout.start("GS1",command,route,0,false,true));
    in.now=Config::SEGMENT_TIMEOUT_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(timeout.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::MotionTimeout),static_cast<int>(timeout.error()));
}

void test_reverse_home_turn_translation_and_safety() {
    CoreCommand command; RoutePlan route; CoreCommand parsed;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":200,"path":["S1"],"target":"S1"})",command));
    const char* wire=R"({"cmd":"NODE_ORDER","cmd_id":210,"route_cmd_id":200,"node":"GS1","next_node":"S1","path_index":0,"maneuver":"RIGHT","turn_crossings":1,"reverse_after_turn":true})";
    TEST_ASSERT_EQUAL(Config::REVERSE_HOME_SUPPORTED,decode(wire,parsed));
    if (!Config::REVERSE_HOME_SUPPORTED) return;
    TEST_ASSERT_TRUE(parsed.reverseAfterTurn);
    auto changed=parsed;changed.reverseAfterTurn=false;TEST_ASSERT_FALSE(CoreProtocol::sameCommand(parsed,changed));
    TEST_ASSERT_FALSE(decode(R"({"cmd":"NODE_ORDER","cmd_id":210,"route_cmd_id":200,"node":"GS1","next_node":"S1","path_index":0,"maneuver":"RIGHT","turn_crossings":1,"reverse_after_turn":1})",parsed));
    TEST_ASSERT_FALSE(decode(R"({"cmd":"NODE_ORDER","cmd_id":210,"route_cmd_id":200,"node":"GS2","next_node":"S1","path_index":0,"maneuver":"RIGHT","turn_crossings":1,"reverse_after_turn":true})",parsed));
    const RobotHeading heads[]={RobotHeading::East,RobotHeading::West,RobotHeading::North,RobotHeading::South};
    const RouteTurn turns[]={RouteTurn::Right,RouteTurn::Left,RouteTurn::TurnAround,RouteTurn::Straight};
    for (unsigned i=0;i<4;++i) {
        TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"GS1",command,route,false,heads[i]));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(turns[i]),static_cast<int>(route.departure[0]));
        TEST_ASSERT_TRUE(route.reverseAfterTurn[0]);
        TEST_ASSERT_TRUE(RoutePlanner::reverseAllowed("GS1",command,route,false,true));
        TEST_ASSERT_FALSE(RoutePlanner::reverseAllowed("GS1",command,route,false,false));
        CoreNavigator nav;TEST_ASSERT_TRUE(nav.start("GS1",command,route,0,false,true));
        auto order=orderFor("GS1","S1",RoutePlanner::turnName(turns[i]),0,200,i==2 ? 2 : i==3 ? 0 : 1);
        TEST_ASSERT_FALSE(nav.authorizeNodeOrder(order)); // Old core must not authorize a different movement.
        order.reverseAfterTurn=true;TEST_ASSERT_TRUE(nav.authorizeNodeOrder(order));
        ControlInput in;in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
        in.linePattern=2;in.now=Config::RFID_STOP_DWELL_MS;
        if (i!=3) {
            const auto pivot=i==1 ? MotorAction::PivotLeft : MotorAction::PivotRight;
            TEST_ASSERT_EQUAL_INT(static_cast<int>(pivot),static_cast<int>(nav.update(in)));
            TEST_ASSERT_FALSE(nav.segmentReverse()); // Front sonar during rotation.
            in.now+=Config::TURN_MIN_MS+1;
            TEST_ASSERT_EQUAL_INT(static_cast<int>(pivot),static_cast<int>(nav.update(in))); // Old line cannot finish rotation.
            const unsigned crossings=i==2 ? 2 : 1;
            for (unsigned k=0;k<crossings;++k) {
                in.linePattern=0;++in.now;nav.update(in);
                in.linePattern=2;in.now+=Config::TURN_MIN_MS+1;
                const auto action=nav.update(in);
                TEST_ASSERT_EQUAL_INT(static_cast<int>(k+1==crossings ? MotorAction::Stop : pivot),static_cast<int>(action));
            }
        }
        ++in.now;TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Backward),static_cast<int>(updateAfterTurnCapture(nav,in)));
        TEST_ASSERT_TRUE(nav.segmentReverse());TEST_ASSERT_TRUE(nav.active());TEST_ASSERT_FALSE(nav.stationOriented());
        in.obstacle=true;++in.now;TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        TEST_ASSERT_TRUE(nav.active());TEST_ASSERT_TRUE(nav.segmentReverse());
        in.obstacle=false;in.now+=100;TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Backward),static_cast<int>(nav.update(in)));
        nav.reachedNode("S1",++in.now);TEST_ASSERT_FALSE(nav.active());TEST_ASSERT_TRUE(nav.stationOriented());
        TEST_ASSERT_FALSE(nav.stationTurnRequired());TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    }
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"GS1",command,route,false,RobotHeading::East));
    CoreNavigator premature;TEST_ASSERT_TRUE(premature.start("GS1",command,route,0));
    premature.reachedNode("S1",1);TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::UnexpectedNode),static_cast<int>(premature.error()));
    for (unsigned hazard=0;hazard<5;++hazard) {
        CoreNavigator nav;TEST_ASSERT_TRUE(nav.start("GS1",command,route,0));
        ControlInput in;in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
        in.linePattern=2;in.now=Config::RFID_STOP_DWELL_MS;nav.update(in);
        in.linePattern=0;in.now+=Config::TURN_MIN_MS+1;nav.update(in);in.linePattern=2;++in.now;nav.update(in);
        if (hazard==0) in.distanceValid=false;
        if (hazard==1) in.mqtt=false;
        if (hazard==2) nav.stop();
        if (hazard==3) {in.linePattern=0;nav.update(in);in.now+=(Config::LINE_LOSS_GRACE_MS ? Config::LINE_LOSS_GRACE_MS : Config::LINE_LOSS_CONFIRM_MS)+1;}
        if (hazard==4) in.now+=Config::SEGMENT_TIMEOUT_MS;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        TEST_ASSERT_FALSE(nav.active());TEST_ASSERT_FALSE(nav.stationOriented());
    }
    CoreNavigator turnTimeout;TEST_ASSERT_TRUE(turnTimeout.start("GS1",command,route,0));
    ControlInput in;in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
    in.linePattern=2;in.now=Config::RFID_STOP_DWELL_MS;turnTimeout.update(in);in.now+=Config::TURN_TIMEOUT_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(turnTimeout.update(in)));
    const bool captureEnabled =
        (route.departure[0] == RouteTurn::Left && Config::LEFT_TURN_CAPTURE_ENABLED) ||
        (route.departure[0] == RouteTurn::Right && Config::RIGHT_TURN_CAPTURE_ENABLED);
    if (Config::TURN_RETRY_ENABLED && !captureEnabled &&
        (route.departure[0] == RouteTurn::Left || route.departure[0] == RouteTurn::Right)) {
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None),static_cast<int>(turnTimeout.error()));
        TEST_ASSERT_TRUE(turnTimeout.active());
        ++in.now;
        const auto retryAction = route.departure[0] == RouteTurn::Left ?
            MotorAction::PivotLeft : MotorAction::PivotRight;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(retryAction),static_cast<int>(turnTimeout.update(in)));
        in.now += Config::TURN_TIMEOUT_MS;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(turnTimeout.update(in)));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::LineLost),static_cast<int>(turnTimeout.error()));
    } else {
        TEST_ASSERT_EQUAL_INT(static_cast<int>(captureEnabled ? ErrorCode::LineLost : ErrorCode::TurnTimeout),
                              static_cast<int>(turnTimeout.error()));
    }
}

void test_left_turn_retries_same_direction_once() {
    if (!Config::TURN_RETRY_ENABLED || Config::LEFT_TURN_CAPTURE_ENABLED) return;
    CoreCommand command; RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":270,"path":["M6"],"target":"M6"})", command));
    TEST_ASSERT_TRUE(RoutePlanner::plan("G2", "M5", command, route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left), static_cast<int>(route.departure[0]));

    ControlInput in;
    in.wifi = in.mqtt = in.motionConfigured = in.lineConfigured = in.distanceValid = true;
    in.linePattern = 2;
    in.now = Config::RFID_STOP_DWELL_MS;

    CoreNavigator recovered;
    TEST_ASSERT_TRUE(recovered.start("M5", command, route, 0, true));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft), static_cast<int>(recovered.update(in)));
    in.now += Config::TURN_TIMEOUT_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(recovered.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None), static_cast<int>(recovered.error()));
    TEST_ASSERT_TRUE(recovered.active());
    ++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft), static_cast<int>(recovered.update(in)));
    in.linePattern = 0; ++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft), static_cast<int>(recovered.update(in)));
    in.linePattern = 2; in.now += Config::TURN_MIN_MS + 1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(recovered.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None), static_cast<int>(recovered.error()));
    TEST_ASSERT_TRUE(recovered.active());

    CoreNavigator failed;
    in.linePattern = 2; in.now = Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_TRUE(failed.start("M5", command, route, 0, true));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft), static_cast<int>(failed.update(in)));
    in.now += Config::TURN_TIMEOUT_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(failed.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None), static_cast<int>(failed.error()));
    ++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft), static_cast<int>(failed.update(in)));
    in.now += Config::TURN_TIMEOUT_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(failed.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::LineLost), static_cast<int>(failed.error()));
    TEST_ASSERT_FALSE(failed.active());

    CoreNavigator disconnected;
    in.mqtt = true; in.linePattern = 2; in.now = Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_TRUE(disconnected.start("M5", command, route, 0, true));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft), static_cast<int>(disconnected.update(in)));
    in.now += Config::TURN_TIMEOUT_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(disconnected.update(in)));
    in.mqtt = false; ++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(disconnected.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::NotConnected), static_cast<int>(disconnected.error()));
    TEST_ASSERT_FALSE(disconnected.active());
}

void test_reverse_home_s2_uses_matching_route_and_order() {
    if (!Config::REVERSE_HOME_SUPPORTED) return;
    CoreCommand command;command.type=CoreCommandType::Goto;command.cmdId=250;command.pathLength=1;
    std::strcpy(command.path[0],"S2");std::strcpy(command.target,"S2");
    const RobotHeading heads[]={RobotHeading::East,RobotHeading::West,RobotHeading::North,RobotHeading::South};
    const RouteTurn turns[]={RouteTurn::Left,RouteTurn::Right,RouteTurn::Straight,RouteTurn::TurnAround};
    for(unsigned i=0;i<4;++i){
        RoutePlan route;TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr,"GS2",command,route,false,heads[i]));
        TEST_ASSERT_TRUE(route.reverseAfterTurn[0]);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(turns[i]),static_cast<int>(route.departure[0]));
        CoreNavigator nav;TEST_ASSERT_TRUE(nav.start("GS2",command,route,0,false,true));
        auto order=orderFor("GS2","S2",RoutePlanner::turnName(turns[i]),0,250,i==2?0:i==3?2:1);
        TEST_ASSERT_FALSE(nav.authorizeNodeOrder(order));order.reverseAfterTurn=true;
        TEST_ASSERT_TRUE(nav.authorizeNodeOrder(order));
        ControlInput in;in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
        in.linePattern=2;in.now=Config::RFID_STOP_DWELL_MS;
        if(i!=2){
            const auto pivot=i==0?MotorAction::PivotLeft:MotorAction::PivotRight;
            TEST_ASSERT_EQUAL_INT(static_cast<int>(pivot),static_cast<int>(nav.update(in)));
            for(unsigned k=0;k<(i==3?2u:1u);++k){
                in.linePattern=0;in.now+=Config::TURN_MIN_MS+1;nav.update(in);
                in.linePattern=2;in.now+=Config::TURN_MIN_MS+1;nav.update(in);
            }
        }
        ++in.now;TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Backward),static_cast<int>(updateAfterTurnCapture(nav,in)));
        in.obstacle=true;++in.now;TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        in.obstacle=false;++in.now;TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Backward),static_cast<int>(nav.update(in)));
        nav.reachedNode("S2",++in.now);TEST_ASSERT_FALSE(nav.active());TEST_ASSERT_TRUE(nav.stationOriented());
        TEST_ASSERT_FALSE(nav.stationTurnRequired());
    }
    CoreCommand parsed;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"NODE_ORDER","cmd_id":251,"route_cmd_id":250,"node":"GS2","next_node":"S2","path_index":0,"maneuver":"LEFT","turn_crossings":1,"reverse_after_turn":true})",parsed));
}

static ControlInput straightInput() {
    ControlInput in;
    in.wifi=in.mqtt=in.motionConfigured=in.lineConfigured=in.distanceValid=true;
    in.linePattern=2; in.now=Config::RFID_STOP_DWELL_MS+1;
    return in;
}
static bool startStraightTrip(CoreNavigator& nav, ControlInput& in, bool through = true) {
    CoreCommand c;
    if (!decode(through ? R"({"cmd":"GOTO","cmd_id":42,"path":["M4","GS2","M3","G1"],"target":"G1","pass_through":[0,1,2]})"
                       : R"({"cmd":"GOTO","cmd_id":42,"path":["M4","GS2","M3","G1"],"target":"G1"})",c)) return false;
    RoutePlan plan;
    if (!RoutePlanner::plan("","G6",c,plan,false,RobotHeading::South) ||
        !nav.start("G6",c,plan,0,false,true) || !nav.grant("M4",101)) return false;
    CoreCommand order;
    if (!decode(R"({"cmd":"NODE_ORDER","cmd_id":43,"route_cmd_id":42,"node":"G6","next_node":"M4","path_index":0,"maneuver":"STRAIGHT","turn_crossings":0})",order) ||
        !nav.authorizeNodeOrder(order)) return false;
    return nav.update(in) == MotorAction::Forward;
}
void test_pass_through_parser_dedup_and_bounds() {
    CoreCommand c, old;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":42,"path":["M4","GS2","M3","G1"],"target":"G1","pass_through":[0,1,2]})",c));
    TEST_ASSERT_TRUE(c.passThrough[0] && c.passThrough[1] && c.passThrough[2]);
    TEST_ASSERT_FALSE(c.passThrough[3]);old=c;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":42,"path":["M4","GS2","M3","G1"],"target":"G1"})",c));
    TEST_ASSERT_FALSE(CoreProtocol::sameCommand(old,c));
    for (const auto* list : {"[0,0]","[3]","[-1]","[true]","[1.5]","[\"0\"]","{}","null","[32]","[4294967295]"}) {
        const std::string wire=std::string(R"({"cmd":"GOTO","cmd_id":42,"path":["M4","GS2","M3","G1"],"target":"G1","pass_through":)")+list+"}";
        TEST_ASSERT_FALSE(decode(wire.c_str(),c));
    }
    std::string wire=R"({"cmd":"GOTO","cmd_id":50,"target":"GS2","path":[)";
    for (int i=0;i<32;++i) wire+=(i ? "," : "")+std::string(i%2 ? "\"GS2\"" : "\"M4\"");
    wire+="],\"pass_through\":[";
    for (int i=0;i<31;++i) wire+=(i ? "," : "")+std::to_string(i);
    wire+="]}";
    TEST_ASSERT_TRUE(decode(wire.c_str(),c));TEST_ASSERT_EQUAL_INT(32,c.pathLength);
    TEST_ASSERT_TRUE(c.passThrough[30]);TEST_ASSERT_FALSE(c.passThrough[31]);
}
void test_straight_trip_reports_each_rfid_without_dwell() {
    CoreNavigator nav;auto in=straightInput();TEST_ASSERT_TRUE(startStraightTrip(nav,in));
    TEST_ASSERT_FALSE(nav.grant("M3",202)); // Not a whole-route reservation at C4.
    TEST_ASSERT_TRUE(nav.canPassThrough("M4",in));nav.reachedNode("M4",++in.now,&in);
    TEST_ASSERT_TRUE(nav.following());TEST_ASSERT_FALSE(nav.needsNodeOrder());TEST_ASSERT_EQUAL_INT(1,nav.pathIndex());
    TEST_ASSERT_EQUAL_STRING("M3",nav.requestedZone());TEST_ASSERT_TRUE(nav.grant("M3",202));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    MutexClearance clearance;clearance.grant("M4",101,"G6",true);clearance.reached("G6","M4");
    MutexClearance::Release release;TEST_ASSERT_FALSE(clearance.pending(release));
    TEST_ASSERT_TRUE(nav.canPassThrough("GS2",in));nav.reachedNode("GS2",++in.now,&in);
    clearance.reached("M4","GS2");TEST_ASSERT_TRUE(clearance.pending(release));
    TEST_ASSERT_EQUAL_STRING("M4",release.zone);TEST_ASSERT_EQUAL_INT(101,release.grantId);
    clearance.queued(release);nav.revokeGrant("M4",101);
    TEST_ASSERT_EQUAL_INT(2,nav.pathIndex());TEST_ASSERT_FALSE(nav.needsNodeOrder());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    TEST_ASSERT_TRUE(nav.canPassThrough("M3",in));nav.reachedNode("M3",++in.now,&in);
    TEST_ASSERT_EQUAL_INT(3,nav.pathIndex());TEST_ASSERT_FALSE(nav.needsNodeOrder());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    TEST_ASSERT_FALSE(nav.canPassThrough("G1",in));nav.reachedNode("G1",++in.now,&in);
    TEST_ASSERT_FALSE(nav.active());TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
}
void test_through_waits_at_gate_without_lookahead_grant() {
    CoreNavigator nav;auto in=straightInput();TEST_ASSERT_TRUE(startStraightTrip(nav,in));
    nav.reachedNode("M4",++in.now,&in);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    TEST_ASSERT_EQUAL_STRING("M3",nav.requestedZone());
    TEST_ASSERT_FALSE(nav.canPassThrough("GS2",in));nav.reachedNode("GS2",++in.now,&in);
    TEST_ASSERT_TRUE(nav.needsNodeOrder());TEST_ASSERT_EQUAL_STRING("M3",nav.missingZone());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_TRUE(nav.grant("M3",202)); // Receipt cannot replace NODE_ORDER after stopping.
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    CoreCommand order;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"NODE_ORDER","cmd_id":44,"route_cmd_id":42,"node":"GS2","next_node":"M3","path_index":2,"maneuver":"STRAIGHT","turn_crossings":0})",order));
    TEST_ASSERT_TRUE(nav.authorizeNodeOrder(order));in.now+=Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
}
void test_through_never_overrides_hazards_or_stop() {
    for (int hazard=0;hazard<6;++hazard) {
        CoreNavigator nav;auto in=straightInput();TEST_ASSERT_TRUE(startStraightTrip(nav,in));
        if(hazard==0)in.wifi=false;
        if(hazard==1)in.mqtt=false;
        if(hazard==2)in.obstacle=true;
        if(hazard==3)in.linePattern=0;
        if(hazard==4)in.distanceValid=false;
        if(hazard==5)in.now+=Config::SEGMENT_TIMEOUT_MS;
        TEST_ASSERT_FALSE(nav.canPassThrough("M4",in));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    }
    CoreNavigator nav;auto in=straightInput();TEST_ASSERT_TRUE(startStraightTrip(nav,in));
    nav.revokeGrant("M4",101);TEST_ASSERT_FALSE(nav.canPassThrough("M4",in));
    nav.reachedNode("M4",++in.now,&in);TEST_ASSERT_TRUE(nav.needsNodeOrder());
    nav.stop();TEST_ASSERT_FALSE(nav.canPassThrough("GS2",in));TEST_ASSERT_FALSE(nav.active());
    CoreNavigator wrong;TEST_ASSERT_TRUE(startStraightTrip(wrong,in));
    wrong.reachedNode("M3",in.now,&in);TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::UnexpectedNode),static_cast<int>(wrong.error()));
    CoreNavigator disconnected;TEST_ASSERT_TRUE(startStraightTrip(disconnected,in));
    disconnected.resetGrants();TEST_ASSERT_FALSE(disconnected.active());
}
void test_through_rejects_turns_and_keeps_legacy_dwell() {
    CoreCommand c;RoutePlan plan;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":42,"path":["M4","G5","K5"],"target":"K5","pass_through":[0]})",c));
    TEST_ASSERT_TRUE(RoutePlanner::plan("","G6",c,plan,false,RobotHeading::South));
    CoreNavigator turn;TEST_ASSERT_FALSE(turn.start("G6",c,plan,0,false,true));
    TEST_ASSERT_FALSE(RoutePlanner::straightThrough("G6","M4","G5"));
    TEST_ASSERT_FALSE(RoutePlanner::straightThrough("GS1","S1","GS1"));
    TEST_ASSERT_FALSE(RoutePlanner::straightThrough("G5","K5","G5"));
    TEST_ASSERT_FALSE(RoutePlanner::straightThrough("K1","G1","M5"));
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":42,"path":["G1","M5"],"target":"M5","pass_through":[]})",c));
    TEST_ASSERT_TRUE(RoutePlanner::plan("","K1",c,plan));
    CoreNavigator exit;TEST_ASSERT_TRUE(exit.start("K1",c,plan,0,false,true));
    CoreNavigator legacy;auto in=straightInput();TEST_ASSERT_TRUE(startStraightTrip(legacy,in,false));
    TEST_ASSERT_FALSE(legacy.canPassThrough("M4",in));legacy.reachedNode("M4",++in.now,&in);
    TEST_ASSERT_TRUE(legacy.needsNodeOrder());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(legacy.update(in)));
}
void test_sonar_pause_keeps_route_and_segment_clock() {
    CoreNavigator nav;auto in=straightInput();TEST_ASSERT_TRUE(startStraightTrip(nav,in));
    const auto routeId = nav.routeCommandId();
    in.now += 100;
    in.obstacle = true;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_TRUE(nav.obstaclePaused());
    TEST_ASSERT_TRUE(nav.active());
    TEST_ASSERT_EQUAL_INT(routeId,nav.routeCommandId());
    in.now += Config::SEGMENT_TIMEOUT_MS + 100;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None),static_cast<int>(nav.error()));
    in.obstacle = false;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    TEST_ASSERT_FALSE(nav.obstaclePaused());
    TEST_ASSERT_EQUAL_INT(routeId,nav.routeCommandId());
    in.obstacle = true;nav.update(in);nav.stop();
    TEST_ASSERT_FALSE(nav.obstaclePaused());
    TEST_ASSERT_FALSE(nav.active());
}

// Exercise the actual navigator with sensor/time sequences, not just config values.
static void startRecoveryTrip(CoreNavigator& nav, ControlInput& in, bool reverse = false,
                              uint32_t startedAt = 0) {
    CoreCommand c; RoutePlan route;
    TEST_ASSERT_TRUE(decode(reverse ?
        R"({"cmd":"GOTO","cmd_id":301,"path":["G5"],"target":"G5"})" :
        R"({"cmd":"GOTO","cmd_id":301,"path":["GS1"],"target":"GS1"})", c));
    TEST_ASSERT_TRUE(RoutePlanner::plan(nullptr, reverse ? "K5" : "S1", c, route));
    TEST_ASSERT_TRUE(nav.start(reverse ? "K5" : "S1", c, route, startedAt));
    in = straightInput(); in.now = startedAt + Config::RFID_STOP_DWELL_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(reverse ? MotorAction::Backward : MotorAction::Forward),
                          static_cast<int>(nav.update(in)));
}
static uint32_t loseLine(CoreNavigator& nav, ControlInput& in, uint8_t side) {
    in.linePattern = side; ++in.now; nav.update(in);
    in.linePattern = 0; ++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
    TEST_ASSERT_TRUE(nav.active()); TEST_ASSERT_TRUE(nav.recoveringLine());
    TEST_ASSERT_FALSE(nav.following());
    TEST_ASSERT_EQUAL_INT(301, nav.routeCommandId()); TEST_ASSERT_EQUAL_INT(0, nav.pathIndex());
    return in.now;
}
void test_line_search_direction_pulses_and_reverse() {
    if (!Config::LINE_RECOVERY_ENABLED) return;
    for (bool reverse : {false, true}) for (uint8_t side : {4, 6, 1, 3}) {
        CoreNavigator nav; ControlInput in; startRecoveryTrip(nav, in, reverse);
        const auto lostAt = loseLine(nav, in, side);
        const auto pivot = side == 4 || side == 6 ? MotorAction::PivotLeft : MotorAction::PivotRight;
        TEST_ASSERT_EQUAL(reverse, nav.segmentReverse());
        in.now = lostAt + Config::LINE_RECOVERY_STOP_MS - 1;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
        in.now++;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(pivot), static_cast<int>(nav.update(in)));
        in.now += Config::LINE_RECOVERY_PULSE_MS;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
        in.now += Config::LINE_RECOVERY_REST_MS;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(pivot), static_cast<int>(nav.update(in)));
        // A delayed loop landing in the next nominal pulse must still rest.
        in.now += Config::LINE_RECOVERY_PULSE_MS + Config::LINE_RECOVERY_REST_MS;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
        ++in.now;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None), static_cast<int>(nav.error()));
        TEST_ASSERT_TRUE(nav.recoveringLine()); TEST_ASSERT_FALSE(nav.following());
        in.now = lostAt + Config::LINE_RECOVERY_TIMEOUT_MS;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
        if (Config::LINE_LOSS_GRACE_MS) {
            TEST_ASSERT_TRUE(nav.active());
            TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None),static_cast<int>(nav.error()));
            in.now = lostAt + Config::LINE_LOSS_GRACE_MS;
            TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        }
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::LineLost), static_cast<int>(nav.error()));
        in.linePattern = 2; ++in.now;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
        TEST_ASSERT_FALSE(nav.active());
    }
}
void test_line_search_requires_stable_center_and_keeps_task() {
    if (!Config::LINE_RECOVERY_ENABLED) return;
    for (bool reverse : {false, true}) {
        CoreNavigator nav; ControlInput in; startRecoveryTrip(nav, in, reverse);
        const auto lostAt = loseLine(nav, in, 4);
        in.now = lostAt + Config::LINE_RECOVERY_STOP_MS;
        nav.update(in);
        // Side-only and 101/111 must never resume translation during recovery.
        for (uint8_t pattern : {4, 1, 5, 7, 6, 3}) {
            in.linePattern = pattern; ++in.now;
            const auto a = nav.update(in);
            TEST_ASSERT_TRUE(a == MotorAction::Stop || a == MotorAction::PivotLeft);
            TEST_ASSERT_FALSE(nav.following());
        }
        in.linePattern = 2; ++in.now; const auto centeredAt = in.now;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
        // Repeated reads with one timestamp cannot establish stable line.
        for (int i = 0; i < 5; ++i)
            TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
        in.now = centeredAt + Config::LINE_RECOVERY_CENTER_MS / 2;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
        in.linePattern = 0; ++in.now; nav.update(in); // Break continuity.
        in.linePattern = 2; ++in.now; nav.update(in);
        in.now += Config::LINE_RECOVERY_CENTER_MS / 2; nav.update(in);
        in.now += Config::LINE_RECOVERY_CENTER_MS / 2;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(reverse ? MotorAction::Backward : MotorAction::Forward),
                              static_cast<int>(nav.update(in)));
        TEST_ASSERT_TRUE(nav.following()); TEST_ASSERT_FALSE(nav.recoveringLine());
        TEST_ASSERT_EQUAL_INT(301, nav.routeCommandId()); TEST_ASSERT_EQUAL_INT(0, nav.pathIndex());
        TEST_ASSERT_EQUAL_STRING(reverse ? "G5" : "GS1", nav.nextNode());
        TEST_ASSERT_FALSE(nav.needsNodeOrder());
        // A second loss in the same segment may wait but cannot sweep again.
        const auto secondLoss = loseLine(nav, in, 1);
        in.now = secondLoss + Config::LINE_RECOVERY_STOP_MS;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
        in.now = secondLoss + (Config::LINE_LOSS_GRACE_MS ? Config::LINE_LOSS_GRACE_MS : Config::LINE_LOSS_CONFIRM_MS);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::LineLost), static_cast<int>(nav.error()));
    }
}
void test_line_search_no_direction_and_expired_memory_hold_still() {
    if (!Config::LINE_RECOVERY_ENABLED) return;
    for (int memoryCase = 0; memoryCase < 3; ++memoryCase) {
        CoreNavigator nav; ControlInput in; startRecoveryTrip(nav, in);
        if (memoryCase) { in.linePattern = 4; ++in.now; nav.update(in); }
        in.linePattern = 2;
        in.now += memoryCase == 2 ? Config::LINE_RECOVERY_MEMORY_MS / 2 : Config::LINE_RECOVERY_MEMORY_MS + 1;
        nav.update(in);
        in.linePattern = 0; ++in.now; const auto lostAt = in.now; nav.update(in);
        in.now = lostAt + Config::LINE_RECOVERY_STOP_MS;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(memoryCase == 2 ? MotorAction::PivotLeft : MotorAction::Stop),
                              static_cast<int>(nav.update(in)));
    }
}
void test_robot01_side_recovery_resumes_same_route_before_deadline() {
    if (!Config::GENTLE_FORWARD_CORRECTION) return;
    for (bool reverse : {false, true}) for (uint8_t side : {1, 3, 4, 6}) {
        CoreNavigator nav; ControlInput in;
        startRecoveryTrip(nav, in, reverse, UINT32_MAX - Config::RFID_STOP_DWELL_MS - 180u);
        const auto lostAt = loseLine(nav, in, side);
        MotorAction action = MotorAction::Stop;
        for (unsigned sample=0; sample<=5; ++sample) {
            in.now = lostAt + 100 + sample*20;
            // A side eye and side+center are evidence of the same side.
            in.linePattern = sample%2 ? (side<=3 ? 1 : 4) : side;
            action = nav.update(in);
            if (!reverse && sample<5)
                TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(action));
            if (sample<5 || reverse) TEST_ASSERT_TRUE(nav.recoveringLine());
        }
        if (reverse) { TEST_ASSERT_FALSE(nav.following()); continue; }
        TEST_ASSERT_EQUAL_INT(static_cast<int>(AGVMath::lineAction(in.linePattern)),static_cast<int>(action));
        TEST_ASSERT_TRUE(nav.following()); TEST_ASSERT_FALSE(nav.recoveringLine());
        TEST_ASSERT_EQUAL_INT(301,nav.routeCommandId()); TEST_ASSERT_EQUAL_INT(0,nav.pathIndex());
        TEST_ASSERT_EQUAL_STRING("GS1",nav.nextNode());
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None),static_cast<int>(nav.error()));
    }
}
void test_robot01_side_recovery_rejects_noise_stale_samples_and_latched_fault() {
    if (!Config::GENTLE_FORWARD_CORRECTION) return;
    for (uint8_t interruption : {0, 2, 3, 5, 7}) {
        CoreNavigator nav; ControlInput in; startRecoveryTrip(nav,in);
        const auto lostAt=loseLine(nav,in,4);
        for (unsigned elapsed : {100u,120u,140u,160u}) {
            in.now=lostAt+elapsed; in.linePattern=6;
            TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        }
        in.now=lostAt+180; in.linePattern=interruption; nav.update(in);
        in.now=lostAt+200; in.linePattern=6;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        TEST_ASSERT_TRUE(nav.recoveringLine());
        // One timestamp and a >50ms sampling gap cannot count as stable line.
        for (int repeat=0;repeat<5;++repeat) nav.update(in);
        in.now=lostAt+300; nav.update(in);
        TEST_ASSERT_TRUE(nav.recoveringLine());
        in.now=lostAt+400; nav.update(in);
        TEST_ASSERT_TRUE(nav.recoveringLine());
        in.now=lostAt+Config::LINE_RECOVERY_TIMEOUT_MS;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::LineLost),static_cast<int>(nav.error()));
        in.linePattern=2; in.now+=20;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        TEST_ASSERT_FALSE(nav.active());
    }
}
void test_robot01_side_recovery_revalidates_after_grant_or_obstacle() {
    if (!Config::GENTLE_FORWARD_CORRECTION) return;
    for (bool obstacle : {false,true}) {
        CoreNavigator nav; auto in=straightInput(); TEST_ASSERT_TRUE(startStraightTrip(nav,in));
        in.linePattern=4; ++in.now; nav.update(in);
        in.linePattern=0; ++in.now; const auto lostAt=in.now; nav.update(in);
        for (unsigned elapsed : {100u,120u,140u,160u}) {
            in.now=lostAt+elapsed; in.linePattern=6;
            TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        }
        if (obstacle) in.obstacle=true; else nav.revokeGrant("M4",101);
        in.now=lostAt+180;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        if (obstacle) in.obstacle=false; else TEST_ASSERT_TRUE(nav.grant("M4",102));
        in.now=lostAt+200;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        TEST_ASSERT_TRUE(nav.recoveringLine());
        for (unsigned elapsed : {220u,240u,260u,280u}) {
            in.now=lostAt+elapsed;
            TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        }
        in.now=lostAt+300;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(AGVMath::lineAction(6)),static_cast<int>(nav.update(in)));
        TEST_ASSERT_TRUE(nav.following()); TEST_ASSERT_EQUAL_INT(42,nav.routeCommandId());
    }
}
void test_line_search_hazards_stop_cancel_and_grant() {
    if (!Config::LINE_RECOVERY_ENABLED) return;
    for (int hazard = 0; hazard < 7; ++hazard) {
        CoreNavigator nav; auto in = straightInput(); TEST_ASSERT_TRUE(startStraightTrip(nav, in));
        in.linePattern = 1; ++in.now; nav.update(in);
        in.linePattern = 0; ++in.now; nav.update(in);
        in.now += Config::LINE_RECOVERY_STOP_MS;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotRight), static_cast<int>(nav.update(in)));
        if (hazard == 0) in.mqtt = false;
        if (hazard == 1) in.wifi = false;
        if (hazard == 2) in.distanceValid = false;
        if (hazard == 3) in.motionConfigured = false;
        if (hazard == 4) nav.stop();
        if (hazard == 5) nav.revokeGrant("M4", 101);
        if (hazard == 6) in.obstacle = true;
        ++in.now;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
        if (hazard < 5) TEST_ASSERT_FALSE(nav.active());
        if (hazard == 5) { TEST_ASSERT_TRUE(nav.active()); TEST_ASSERT_EQUAL_STRING("M4", nav.missingZone()); }
        if (hazard == 6) {
            TEST_ASSERT_TRUE(nav.obstaclePaused()); TEST_ASSERT_TRUE(nav.recoveringLine());
            in.now += 1000; TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
            in.obstacle = false; ++in.now; nav.update(in);
            TEST_ASSERT_TRUE(nav.active()); TEST_ASSERT_EQUAL_INT(42, nav.routeCommandId());
        }
    }
}
void test_line_search_rfid_stops_and_clears_old_direction() {
    if (!Config::LINE_RECOVERY_ENABLED) return;
    CoreNavigator nav; auto in = straightInput(); TEST_ASSERT_TRUE(startStraightTrip(nav, in));
    in.linePattern = 4; ++in.now; nav.update(in);
    in.linePattern = 0; ++in.now; nav.update(in);
    TEST_ASSERT_FALSE(nav.canPassThrough("M4", in));
    nav.reachedNode("M4", ++in.now, &in);
    TEST_ASSERT_FALSE(nav.recoveringLine()); TEST_ASSERT_TRUE(nav.needsNodeOrder());
    TEST_ASSERT_EQUAL_INT(1, nav.pathIndex()); TEST_ASSERT_NULL(nav.missingZone());
    TEST_ASSERT_TRUE(nav.authorizeNodeOrder(orderFor("M4", "GS2", "STRAIGHT", 1, 42, 0)));
    in.linePattern = 2; in.now += Config::RFID_STOP_DWELL_MS; nav.update(in);
    in.linePattern = 0; ++in.now; nav.update(in);
    in.now += Config::LINE_RECOVERY_STOP_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(nav.update(in)));
    CoreNavigator wrong; ControlInput bad; startRecoveryTrip(wrong, bad);
    loseLine(wrong, bad, 4); wrong.reachedNode("M3", ++bad.now);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::UnexpectedNode), static_cast<int>(wrong.error()));
}
void test_line_search_clock_wrap_and_segment_deadline() {
    if (!Config::LINE_RECOVERY_ENABLED) return;
    CoreNavigator nav; ControlInput in; startRecoveryTrip(nav, in, false, UINT32_MAX - 1100u);
    const auto lostAt = loseLine(nav, in, 4);
    in.now = lostAt + Config::LINE_RECOVERY_STOP_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft), static_cast<int>(nav.update(in)));
    in.linePattern = 2; ++in.now; nav.update(in);
    in.now += 50; nav.update(in); in.now += 50;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward), static_cast<int>(nav.update(in)));
    CoreNavigator deadline; startRecoveryTrip(deadline, in);
    in.now += Config::SEGMENT_TIMEOUT_MS - 100;
    loseLine(deadline, in, 4); in.now += 100;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop), static_cast<int>(deadline.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::MotionTimeout), static_cast<int>(deadline.error()));
}

static void startG1Left(CoreNavigator& nav, ControlInput& in) {
    CoreCommand c; RoutePlan route;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":510,"path":["K1"],"target":"K1"})",c));
    TEST_ASSERT_TRUE(RoutePlanner::plan("M3","G1",c,route));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RouteTurn::Left),static_cast<int>(route.departure[0]));
    TEST_ASSERT_TRUE(nav.start("G1",c,route,0,false,true));
    TEST_ASSERT_TRUE(nav.authorizeNodeOrder(orderFor("G1","K1","LEFT",0,510,1)));
    in=straightInput();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft),static_cast<int>(nav.update(in)));
}
void test_robot02_left_captures_center_before_min_without_blank() {
    if (!Config::LEFT_TURN_CAPTURE_ENABLED) return;
    CoreNavigator nav; ControlInput in; startG1Left(nav,in);
    const auto turnAt=in.now;
    TEST_ASSERT_FALSE(nav.observeTurnLine(in)); // Still the original stripe.
    in.linePattern=1;in.now+=10;
    TEST_ASSERT_FALSE(nav.observeTurnLine(in)); // Only right eye: no 000 sample.
    TEST_ASSERT_TRUE(nav.precisionPivot());
    in.linePattern=3;in.now+=10;
    TEST_ASSERT_TRUE(nav.observeTurnLine(in)); // Center returns before TURN_MIN_MS.
    TEST_ASSERT_TRUE(in.now-turnAt<Config::TURN_MIN_MS);
    TEST_ASSERT_TRUE(nav.capturingTurnLine());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_EQUAL_INT(510,nav.routeCommandId());TEST_ASSERT_EQUAL_INT(0,nav.pathIndex());
    TEST_ASSERT_EQUAL_STRING("K1",nav.nextNode());TEST_ASSERT_FALSE(nav.needsNodeOrder());
    TEST_ASSERT_FALSE(nav.following());
    in.linePattern=2;in.now+=Config::LEFT_CAPTURE_SAMPLE_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_TRUE(nav.following());TEST_ASSERT_FALSE(nav.capturingTurnLine());
    ++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::None),static_cast<int>(nav.error()));
}
void test_robot02_left_old_line_and_timeout_never_start_long_retry() {
    if (!Config::LEFT_TURN_CAPTURE_ENABLED) return;
    CoreNavigator nav; ControlInput in; startG1Left(nav,in);
    const auto turnAt=in.now;
    for (uint8_t pattern : {2,6,7,5,3}) {
        in.linePattern=pattern;in.now+=30;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft),static_cast<int>(nav.update(in)));
        TEST_ASSERT_FALSE(nav.capturingTurnLine());
    }
    in.linePattern=0;++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotLeft),static_cast<int>(nav.update(in)));
    TEST_ASSERT_TRUE(nav.precisionPivot());
    in.now=turnAt+Config::TURN_TIMEOUT_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::LineLost),static_cast<int>(nav.error()));
    TEST_ASSERT_FALSE(nav.active());
    in.linePattern=2;++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
}
void test_robot02_left_overshoot_countersteers_briefly_then_faults() {
    if (!Config::LEFT_TURN_CAPTURE_ENABLED) return;
    CoreNavigator nav; ControlInput in; startG1Left(nav,in);
    in.linePattern=0;++in.now;nav.update(in);
    in.linePattern=2;++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    const auto capturedAt=in.now;
    in.linePattern=0;++in.now;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    in.now=capturedAt+Config::LINE_RECOVERY_STOP_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::PivotRight),static_cast<int>(nav.update(in)));
    TEST_ASSERT_TRUE(nav.precisionPivot());TEST_ASSERT_FALSE(nav.following());
    in.now+=Config::LINE_RECOVERY_PULSE_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    in.linePattern=2;++in.now;nav.update(in);
    in.now+=Config::LEFT_CAPTURE_MAX_GAP_MS+1;nav.update(in);
    in.linePattern=7;++in.now; // Wide junction breaks centered stability.
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_TRUE(nav.capturingTurnLine());
    in.linePattern=0;in.now=capturedAt+Config::TURN_CAPTURE_CONFIRM_TIMEOUT_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ErrorCode::LineLost),static_cast<int>(nav.error()));
    TEST_ASSERT_FALSE(nav.active());
}
void test_robot02_left_capture_hazards_and_grant_remain_effective() {
    if (!Config::LEFT_TURN_CAPTURE_ENABLED) return;
    for (int hazard=0;hazard<4;++hazard) {
        CoreNavigator nav;ControlInput in;startG1Left(nav,in);
        in.linePattern=0;++in.now;nav.update(in);
        in.linePattern=2;++in.now;nav.update(in);
        TEST_ASSERT_TRUE(nav.capturingTurnLine());
        if(hazard==0)in.mqtt=false;
        if(hazard==1)in.distanceValid=false;
        if(hazard==2)nav.stop();
        if(hazard==3)in.obstacle=true;
        ++in.now;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        if(hazard<3)TEST_ASSERT_FALSE(nav.active());
        else {TEST_ASSERT_TRUE(nav.active());TEST_ASSERT_TRUE(nav.obstaclePaused());}
    }
    CoreCommand c;RoutePlan route;CoreNavigator nav;
    TEST_ASSERT_TRUE(decode(R"({"cmd":"GOTO","cmd_id":511,"path":["M6"],"target":"M6"})",c));
    TEST_ASSERT_TRUE(RoutePlanner::plan("G2","M5",c,route));
    TEST_ASSERT_TRUE(nav.start("M5",c,route,0));
    TEST_ASSERT_TRUE(nav.grant("M5",501));TEST_ASSERT_TRUE(nav.grant("M6",502));
    auto in=straightInput();nav.update(in);
    in.linePattern=0;++in.now;nav.update(in);
    in.linePattern=2;++in.now;nav.update(in);
    TEST_ASSERT_TRUE(nav.capturingTurnLine());nav.revokeGrant("M6",502);
    in.linePattern=0;in.now+=Config::LINE_RECOVERY_STOP_MS;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_EQUAL_STRING("M6",nav.missingZone());TEST_ASSERT_TRUE(nav.active());
}

void test_robot02_left_side_center_votes_align_and_settle() {
    if (!Config::LEFT_TURN_CAPTURE_ENABLED) return;
    for (uint8_t side : {3,6}) {
        CoreNavigator nav; ControlInput in; startG1Left(nav,in);
        in.linePattern=0;++in.now;nav.update(in);
        in.linePattern=side;++in.now;nav.update(in);
        const auto capturedAt=in.now;
        TEST_ASSERT_TRUE(nav.capturingTurnLine());
        // Constant 011/110 is real line evidence and selects the sensor side.
        in.now+=Config::LINE_RECOVERY_REST_MS;
        if(side==6) nav.update(in); // Direction reversal requires a STOP first.
        in.now+=Config::LINE_RECOVERY_REST_MS;
        const auto expected=side==3 ? MotorAction::PivotRight : MotorAction::PivotLeft;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(Config::LINE_RECOVERY_TRACK_CONFIRM ? MotorAction::Stop : expected),static_cast<int>(nav.update(in)));
        TEST_ASSERT_FALSE(nav.following());TEST_ASSERT_FALSE(nav.settlingTurnLine());
        // Mixed side+center votes complete without 010 being held for 100ms.
        in.now+=Config::LEFT_CAPTURE_SAMPLE_MS;in.linePattern=2;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
        TEST_ASSERT_TRUE(nav.following());TEST_ASSERT_TRUE(nav.settlingTurnLine());
        TEST_ASSERT_EQUAL_INT(510,nav.routeCommandId());TEST_ASSERT_EQUAL_INT(0,nav.pathIndex());
        TEST_ASSERT_TRUE(in.now-capturedAt<Config::LINE_RECOVERY_TIMEOUT_MS);
        ++in.now;TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Forward),static_cast<int>(nav.update(in)));
        in.now+=Config::LEFT_CAPTURE_SETTLE_MS;nav.update(in);
        TEST_ASSERT_FALSE(nav.settlingTurnLine());
    }
}
void test_robot02_left_vote_noise_spacing_wide_and_blank() {
    if (!Config::LEFT_TURN_CAPTURE_ENABLED) return;
    CoreNavigator nav;ControlInput in;startG1Left(nav,in);
    in.linePattern=0;++in.now;nav.update(in);
    in.linePattern=3;++in.now;nav.update(in);
    in.linePattern=2;
    for(int i=0;i<5;++i)nav.update(in); // Same millisecond cannot add votes.
    TEST_ASSERT_TRUE(nav.capturingTurnLine());
    in.now+=Config::LEFT_CAPTURE_SAMPLE_MS;in.linePattern=0;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    in.now+=Config::LEFT_CAPTURE_SAMPLE_MS;in.linePattern=2;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(nav.update(in)));
    TEST_ASSERT_TRUE(nav.following()); // 011,000,010: two of three.
    for(uint8_t wide : {5,7}) {
        CoreNavigator wideNav;startG1Left(wideNav,in);
        in.linePattern=0;++in.now;wideNav.update(in);
        in.linePattern=3;++in.now;wideNav.update(in);
        in.linePattern=wide;
        for(int i=0;i<5;++i) {
            in.now+=Config::LEFT_CAPTURE_SAMPLE_MS;
            TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(wideNav.update(in)));
            TEST_ASSERT_FALSE(wideNav.following());
        }
    }
    CoreNavigator stale;startG1Left(stale,in);
    in.linePattern=0;++in.now;stale.update(in);
    in.linePattern=3;++in.now;stale.update(in);
    in.linePattern=2;in.now+=Config::LEFT_CAPTURE_MAX_GAP_MS+1;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MotorAction::Stop),static_cast<int>(stale.update(in)));
    TEST_ASSERT_TRUE(stale.capturingTurnLine()); // Old side sample expired.
    in.now+=Config::LEFT_CAPTURE_SAMPLE_MS;stale.update(in);
    TEST_ASSERT_TRUE(stale.following());
}

int main() {
    static_assert(Config::ENABLE_CORE_MOTION ==
                  (Config::ONE_ROBOT_TRIAL || Config::TWO_ROBOT_BENCH_TRIAL),
                  "Only explicit trial builds may enable motion");
    static_assert(Config::RFID_STOP_DWELL_MS == (Config::ENABLE_CORE_MOTION ? 1000u : 200u),
                  "RFID pause must match the selected firmware mode");
    static_assert(!Config::ONE_ROBOT_TRIAL || (Config::GENTLE_CORRECTION_SPEED > 0 &&
                  Config::GENTLE_CORRECTION_SPEED < Config::CORRECTION_SPEED),
                  "Trial gentle correction must keep both wheels moving");
    UNITY_BEGIN();
    RUN_TEST(test_robot02_left_captures_center_before_min_without_blank);
    RUN_TEST(test_robot02_left_old_line_and_timeout_never_start_long_retry);
    RUN_TEST(test_robot02_left_overshoot_countersteers_briefly_then_faults);
    RUN_TEST(test_robot02_left_capture_hazards_and_grant_remain_effective);
    RUN_TEST(test_robot02_left_side_center_votes_align_and_settle);
    RUN_TEST(test_robot02_left_vote_noise_spacing_wide_and_blank);
    RUN_TEST(test_line_search_direction_pulses_and_reverse);
    RUN_TEST(test_line_search_requires_stable_center_and_keeps_task);
    RUN_TEST(test_robot01_side_recovery_resumes_same_route_before_deadline);
    RUN_TEST(test_robot01_side_recovery_rejects_noise_stale_samples_and_latched_fault);
    RUN_TEST(test_robot01_side_recovery_revalidates_after_grant_or_obstacle);
    RUN_TEST(test_line_search_no_direction_and_expired_memory_hold_still);
    RUN_TEST(test_line_search_hazards_stop_cancel_and_grant);
    RUN_TEST(test_line_search_rfid_stops_and_clears_old_direction);
    RUN_TEST(test_line_search_clock_wrap_and_segment_deadline);
    RUN_TEST(test_pass_through_parser_dedup_and_bounds);
    RUN_TEST(test_straight_trip_reports_each_rfid_without_dwell);
    RUN_TEST(test_through_waits_at_gate_without_lookahead_grant);
    RUN_TEST(test_through_never_overrides_hazards_or_stop);
    RUN_TEST(test_through_rejects_turns_and_keeps_legacy_dwell);
    if (Config::REAR_ULTRASONIC_ECHO_PIN >= 0) RUN_TEST(test_sonar_pause_keeps_route_and_segment_clock);
    RUN_TEST(test_core_commands);
    RUN_TEST(test_grant_owns_payload_before_ack_publish);
    RUN_TEST(test_node_order_parser_and_correlation);
    RUN_TEST(test_server_node_order_stops_then_pivots_left_with_mutex);
    RUN_TEST(test_server_node_order_right_then_station_turn);
    RUN_TEST(test_reverse_home_turn_translation_and_safety);
    RUN_TEST(test_left_turn_retries_same_direction_once);
    RUN_TEST(test_reverse_home_s2_uses_matching_route_and_order);
#if defined(AGV_ROBOT_PROFILE_01) && AGV_ROBOT_PROFILE_01 == 1
    RUN_TEST(test_robot01_blank_line_waits_for_rfid_without_moving);
#endif
#if defined(AGV_ROBOT_PROFILE_02) && AGV_ROBOT_PROFILE_02 == 1
    RUN_TEST(test_robot02_blank_line_holds_before_fault);
#endif
    RUN_TEST(test_bad_commands_stay_rejected);
    RUN_TEST(test_core_wire_formats);
    RUN_TEST(test_bench_voltage_interlock);
    RUN_TEST(test_motion_lock_reports_actual_condition_and_preserves_fault);
    RUN_TEST(test_battery_adc_to_percentage_and_motion_gate);
    RUN_TEST(test_mutex_clearance_requires_measured_next_tag);
    RUN_TEST(test_mutex_clearance_rejects_unvisited_or_wrong_route);
    RUN_TEST(test_mutex_encoder_release_requires_both_wheels_tag_loss_and_safe_route);
    RUN_TEST(test_mutex_exit_release_continues_without_reacquiring_current_zone);
    RUN_TEST(test_mutex_exit_clearance_keeps_next_zone_and_pass_through_checks);
    RUN_TEST(test_mutex_exit_clearance_resets_on_new_route_fault_and_session);
    RUN_TEST(test_directed_route_turns);
    RUN_TEST(test_fixed_bay_heading_and_reverse_exit);
    RUN_TEST(test_station_arrival_turns_at_station_before_ready);
    RUN_TEST(test_bay_exit_turnaround_counts_map_branches);
    RUN_TEST(test_manual_tag_relocation_does_not_claim_reverse_heading);
    RUN_TEST(test_solo_route_crosses_mutex_without_grant);
    RUN_TEST(test_core_navigator_stops_at_rfid);
    RUN_TEST(test_two_robot_navigation_requires_and_revokes_grant);
    RUN_TEST(test_turn_does_not_reacquire_junction_before_blank);
    RUN_TEST(test_robot02_gs2_turn_waits_for_grant_and_stops_on_network_loss);
    if (Config::ONE_ROBOT_TRIAL) RUN_TEST(test_trial_gently_recovers_from_small_line_offset);
    return UNITY_END();
}
