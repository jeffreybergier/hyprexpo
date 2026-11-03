#pragma once

#include <cmath>
#include <optional>

namespace Hyprexpo {
// Pure activation state. The first 8 units open the overview; excess movement
// belongs to its existing 24-unit rotation accumulator, not another gesture.
struct RolodexActivation {
    double pending = 0;
    double lastMs = 0;

    void reset() { pending = 0; lastMs = 0; }

    std::optional<double> update(double delta, double nowMs, bool eligible) {
        if (!eligible || !std::isfinite(delta) || delta == 0) {
            reset();
            return std::nullopt;
        }
        if (nowMs - lastMs > 250 || delta * pending < 0)
            pending = 0;
        lastMs = nowMs;
        pending += delta;
        if (std::abs(pending) < 8)
            return std::nullopt;
        const double remainder = pending - std::copysign(8.0, pending);
        reset();
        return remainder;
    }
};
}
