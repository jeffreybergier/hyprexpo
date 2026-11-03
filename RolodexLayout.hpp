#pragma once

#include <algorithm>
#include <cmath>

// Pure, output-independent geometry. Coordinates are logical pixels; callers
// apply output scale only at the renderer boundary.
namespace Hyprexpo::Rolodex {
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
    Row row(double v) const {
        const double localY = (v - 0.5) * height;
        const double perspective = 1.0 / (1.0 + localY * std::sin(tilt) / (height * 2.5));
        return {cx - width * perspective / 2, cx + width * perspective / 2,
                cy + localY * std::cos(tilt) * perspective};
    }
    bool contains(double x, double y) const {
        if (!visible) return false;
        const auto top = row(0), bottom = row(1);
        if (y < top.y || y > bottom.y) return false;
        const double t = (y - top.y) / (bottom.y - top.y);
        return x >= top.left + t * (bottom.left - top.left)
            && x <= top.right + t * (bottom.right - top.right);
    }
};
inline Card card(int index, double position, int count, double width, double height) {
    const double d = distance(index, position, count);
    const double depth = std::cos(std::clamp(d * 0.55, -1.5, 1.5));
    const double scale = 0.64 * (0.65 + depth * 0.35);
    return {width / 2, height / 2 + std::sin(d * 0.55) * height * 0.49,
            width * scale, height * scale, std::clamp(-d * 0.58, -1.1, 1.1), depth,
            width > 0 && height > 0 && std::abs(d) < 2.6};
}
inline Card blend(const Card& from, const Card& to, double progress) {
    const double t = std::clamp(progress, 0.0, 1.0);
    const auto mix = [t](double a, double b) { return a + (b - a) * t; };
    return {mix(from.cx, to.cx), mix(from.cy, to.cy), mix(from.width, to.width),
            mix(from.height, to.height), mix(from.tilt, to.tilt), mix(from.depth, to.depth),
            t == 0 ? from.visible : (t == 1 ? to.visible : from.visible || to.visible)};
}
inline Card desktop(double width, double height) {
    return {width / 2, height / 2, width, height, 0, 1, true};
}
inline double fingerPosition(double position, double delta, double units, int count) {
    if (count <= 0 || !std::isfinite(delta)) return position;
    return std::remainder(position + delta / std::max(1.0, units), static_cast<double>(count));
}
inline double interpolate(double from, double to, double elapsed) {
    const double t = std::clamp(elapsed / 0.24, 0.0, 1.0);
    return from + (to - from) * (1.0 - std::pow(1.0 - t, 3));
}
} // namespace Hyprexpo::Rolodex
