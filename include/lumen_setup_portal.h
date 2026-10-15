#pragma once

namespace SetupPortal {

// opens when no wifi details are saved, they never connected, or boot is held 5 s.
// blocks for up to 10 minutes.
void runIfNeeded();

}  // namespace SetupPortal
