#pragma once

#include "types.h"

namespace Sensors {

bool init();

bool readCurrent(SensorReading& out);

bool isHealthy();

}  // namespace Sensors
