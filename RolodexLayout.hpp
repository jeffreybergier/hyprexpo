#pragma once

#include <algorithm>
#include <cmath>

// Pure, output-independent geometry. Coordinates are logical pixels; callers
// apply output scale only at the renderer boundary.
namespace Hyprexpo::Rolodex {
inline constexpr double ANIMATION_SECONDS = 0.3;
inline double easeOut(double progress) {
    const double t = std::clamp(progress, 0.0, 1.0);
    return 1.0 - std::pow(1.0 - t, 5);
}
inline double distance(double index, double position, int count) {
    if (count <= 0) return 0;
    return std::remainder(index - position, static_cast<double>(count));
}
inline int wrap(int index, int count) {
    return count > 0 ? (index % count + count) % count : -1;
}
struct Row { double left, right, y; };
struct Card {
    double cx, cy, width, height, tilt, depth;
    bool visible;
    double pivot = 0.5;
    double opacity = 1.0;
    Row row(double v) const {
        const double localY = (v - pivot) * height;
        const double perspective = 1.0 / (1.0 + (v - pivot) * std::sin(tilt) / 5.0);
        return {cx - width * perspective / 2, cx + width * perspective / 2,
                cy + localY * std::cos(tilt) * perspective};
    }
    bool contains(double x, double y) const {
        if (!visible || opacity <= 0 || width <= 0 || height <= 0) return false;
        const auto top = row(0), bottom = row(1);
        if (bottom.y <= top.y || y < top.y || y > bottom.y) return false;
        const double t = (y - top.y) / (bottom.y - top.y);
        return x >= top.left + t * (bottom.left - top.left)
            && x <= top.right + t * (bottom.right - top.right);
    }
};
inline Card card(int index, double position, int count, double width, double height) {
    if (count <= 0 || width <= 0 || height <= 0) return {};
    // One outgoing card (-1 < d < 0), followed by the ordered rear stack.
    // Unlike shortest-distance wrapping, this never exchanges two visible
    // cards' depth at the halfway point. Recycle only below the viewport.
    double d = std::fmod(index - position + 1.0, static_cast<double>(count));
    if (d <= 0) d += count;
    d = count == 1 ? 0 : d - 1.0;
    const double rear = std::max(0.0, d);
    const double turn = std::clamp(-d, 0.0, 1.0);
    const double scale = 0.70 / (1.0 + rear * 0.055);
    // Lower the hinge as it rolls forward; this also offsets the initial
    // perspective expansion so the top edge peels down, never bobs upward.
    const double drop = 0.22 * turn + 0.30 * std::pow(turn, 3);
    const double hinge = height * (0.86 - 0.065 * std::min(rear, 3.0) + drop);
    const double tilt = turn * 1.48 - std::min(rear, 3.0) * 0.065;
    const double rearLimit = std::min(3.0, static_cast<double>(count - 1));
    // Introduce recycled cards gently at the back, including two-card stacks.
    const double opacity = rear > 0 ? std::clamp((rearLimit - rear) / 0.4, 0.0, 1.0) : 1.0;
    return {width / 2, hinge, width * scale, height * scale, tilt,
            1.0 - d * 0.15, opacity > 0, 1.0, opacity};
}
inline Card blend(const Card& from, const Card& to, double progress) {
    const double t = std::clamp(progress, 0.0, 1.0);
    const auto mix = [t](double a, double b) { return a + (b - a) * t; };
    return {mix(from.cx, to.cx), mix(from.cy, to.cy), mix(from.width, to.width),
            mix(from.height, to.height), mix(from.tilt, to.tilt), mix(from.depth, to.depth),
            t == 0 ? from.visible : (t == 1 ? to.visible : from.visible || to.visible),
            mix(from.pivot, to.pivot), mix(from.opacity, to.opacity)};
}
inline Card desktop(double width, double height) {
    return {width / 2, height / 2, width, height, 0, 1, true};
}
inline double fingerPosition(double position, double delta, double units, int count) {
    if (count <= 0 || !std::isfinite(delta)) return position;
    return std::remainder(position + delta / std::max(1.0, units), static_cast<double>(count));
}
inline double interpolate(double from, double to, double elapsed) {
    return from + (to - from) * easeOut(elapsed / ANIMATION_SECONDS);
}
} // namespace Hyprexpo::Rolodex
