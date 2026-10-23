#include "../firmware/NCKHAGV/src/Config.h"
#include <string_view>

#if defined(AGV_ROBOT_PROFILE_01) && AGV_ROBOT_PROFILE_01 == 1
static_assert(std::string_view(Config::AGV_ID) == "01", "Robot 01 firmware has the wrong MQTT ID");
#elif defined(AGV_ROBOT_PROFILE_02) && AGV_ROBOT_PROFILE_02 == 1
static_assert(std::string_view(Config::AGV_ID) == "02", "Robot 02 firmware has the wrong MQTT ID");
#else
#error "Compile this check with exactly one robot profile"
#endif

static_assert(Config::TWO_ROBOT_BENCH_TRIAL && Config::ENABLE_CORE_MOTION,
              "Robot profile must select the supervised bench build");
static_assert(Config::BASE_SPEED == AGV_BENCH_MOTOR_SPEED &&
              Config::LEFT_SPEED_TRIM == AGV_BENCH_LEFT_SPEED_TRIM &&
              Config::RIGHT_SPEED_TRIM == AGV_BENCH_RIGHT_SPEED_TRIM,
              "Robot profile tuning is not being used");

int main() { return 0; }
