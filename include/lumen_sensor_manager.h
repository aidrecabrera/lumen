#pragma once

#include "lumen_app_types.h"

namespace Sensors {

bool init();

bool readCurrent(SensorReading& out);

bool isHealthy();

}  // namespace Sensors
