#include "../RolodexLayout.hpp"
#include "../RolodexActivation.hpp"
#include <cassert>
#include <iostream>
using namespace Hyprexpo::Rolodex;
int main() {
    Hyprexpo::RolodexActivation activation;
    assert(!activation.update(3, 1000, true));
    assert(activation.update(7, 1010, true).value() == 2);
    assert(activation.update(-32, 1020, true).value() == -24);
    assert(!activation.update(4, 1030, true));
    assert(!activation.update(-4, 1040, true)); // reversal resets
    assert(activation.update(-4, 1050, true).value() == 0);
    assert(!activation.update(4, 1060, true));
    assert(!activation.update(4, 1400, true)); // idle resets
    assert(!activation.update(4, 1410, false)); // modifier/submap/lock resets
    assert(!activation.update(4, 1420, true));
    assert(!activation.update(0, 1430, true)); // axis stop resets
    assert(!activation.update(4, 1440, true));
    activation.reset();
    assert(!activation.update(4, 1450, true));
    assert(wrap(-1, 10) == 9 && wrap(10, 10) == 0 && wrap(0, 0) == -1);
    assert(distance(0, 9, 10) == 1 && distance(9, 0, 10) == -1);
    assert(ANIMATION_SECONDS == 0.3);
    assert(interpolate(0, 1, 0) == 0 && interpolate(0, 1, 0.3) == 1);
    assert(interpolate(0, 1, 0.24) < 1);
    assert(interpolate(0, 1, 0.15) > 0.95);
    assert(easeOut(-1) == 0 && easeOut(2) == 1);
    double lastIncrement = 1;
    for (int i = 1; i <= 100; ++i) {
        const double increment = easeOut(i / 100.0) - easeOut((i - 1) / 100.0);
        assert(increment >= 0 && increment <= lastIncrement);
        lastIncrement = increment;
    }
    assert(interpolate(0, 1, 0.1) > 0 && interpolate(0, 1, 0.1) < 1);
    assert(interpolate(2, -1, 1) == -1);
    for (int count : {1, 2, 3, 10, 50}) {
        for (double position : {-10.0, -0.5, 0.0, 0.5, 9.75, 50.0}) {
            for (int id = 0; id < count; ++id) {
                const auto c = card(id, position, count, 1920, 1080);
                const auto scaled = card(id, position, count, 5760, 3240);
                const auto top = c.row(0), bottom = c.row(1);
                assert(std::isfinite(top.y) && bottom.y > top.y && top.right > top.left);
                assert(std::abs(scaled.row(0).left - 3 * top.left) < 1e-6);
                assert(!c.contains(-1, -1));
                if (c.visible) assert(c.contains(c.cx, c.cy));
                assert(!c.contains(c.cx, bottom.y + 1));
                for (int i = 0; i < 96; ++i) assert(c.row((i + 1) / 96.0).y > c.row(i / 96.0).y);
            }
        }
    }
    assert(fingerPosition(0, 24, 96, 10) == 0.25);
    assert(fingerPosition(0.25, -24, 96, 10) == 0);
    assert(fingerPosition(9, 192, 96, 10) == 1);
    assert(fingerPosition(0, 1000000, 96, 10) < 10);
    assert(fingerPosition(0, 100, 96, 1) == std::remainder(100.0 / 96, 1.0));
    assert(fingerPosition(3, 10, 96, 0) == 3);
    const auto full = desktop(1920, 1080);
    const auto partial = card(0, 0.35, 10, 1920, 1080);
    const auto openingStart = blend(full, partial, 0);
    const auto openingEnd = blend(full, partial, 1);
    assert(openingStart.row(0).left == 0 && openingStart.row(0).y == 0);
    assert(openingStart.row(1).right == 1920 && openingStart.row(1).y == 1080);
    assert(openingEnd.width == partial.width && openingEnd.tilt == partial.tilt);
    const auto interrupted = blend(full, partial, 0.4);
    const auto closeStart = blend(interrupted, full, 0);
    const auto closeEnd = blend(interrupted, full, 1);
    assert(closeStart.width == interrupted.width && closeStart.cy == interrupted.cy);
    assert(closeEnd.width == full.width && closeEnd.tilt == 0);
    for (int i = 0; i <= 100; ++i) {
        const auto c = blend(partial, full, i / 100.0);
        assert(c.row(1).y > c.row(0).y && c.row(0).right > c.row(0).left);
    }
    const auto front = card(0, 0, 10, 1920, 1080);
    assert(front.tilt == 0 && front.depth == 1 && front.visible);
    assert(card(1, 0, 10, 1920, 1080).cy > front.cy);
    assert(card(9, 0, 10, 1920, 1080).cy < front.cy);
    assert(!card(5, 0, 10, 1920, 1080).visible);
    std::cout << "RolodexLayoutTests passed\n";
}
