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
    assert(interpolate(0, 1, 0) == 0 && interpolate(0, 1, 0.24) == 1);
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
    const auto front = card(0, 0, 10, 1920, 1080);
    assert(front.tilt == 0 && front.depth == 1 && front.visible);
    assert(card(1, 0, 10, 1920, 1080).cy > front.cy);
    assert(card(9, 0, 10, 1920, 1080).cy < front.cy);
    assert(!card(5, 0, 10, 1920, 1080).visible);
    std::cout << "RolodexLayoutTests passed\n";
}
