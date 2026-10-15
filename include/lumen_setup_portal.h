#pragma once

#include <stdint.h>

namespace SetupPortal {

// opens when no wifi details are saved or they never connected.
// blocks for up to 10 minutes.
void runIfNeeded();

// holding boot for 5 s while running restarts into the portal. gpio0 is a strapping pin,
// so a hold through reset would enter the bootloader instead.
void watchButton(uint64_t now_ms);

}  // namespace SetupPortal
