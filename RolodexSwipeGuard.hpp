#pragma once

#include <cmath>

namespace Hyprexpo {
// Decide before Hyprland can start its workspace gesture. Match 0.56.2's
// cumulative 5-unit recognition threshold and vertical-on-ties axis choice.
// Latch for the entire stroke: closing the overview or changing direction
// must not leak a previously suppressed swipe into the desktop.
class RolodexSwipeGuard {
  public:
    void begin(unsigned fingers, bool rolodexOpen) {
        m_route = fingers == 3 && rolodexOpen ? Route::Pending : Route::Pass;
        m_x = m_y = 0;
    }

    bool suppress(double dx, double dy) {
        if (m_route != Route::Pending)
            return m_route == Route::Block;

        m_x += dx;
        m_y += dy;
        if (std::abs(m_x) < 5 && std::abs(m_y) < 5)
            return false;

        m_route = std::abs(m_x) > std::abs(m_y) ? Route::Block : Route::Pass;
        return m_route == Route::Block;
    }

    void end() { begin(0, false); }

  private:
    enum class Route { Pending, Pass, Block };
    Route m_route = Route::Pass;
    double m_x = 0, m_y = 0;
};
}
