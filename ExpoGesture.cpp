#include "ExpoGesture.hpp"

#include "IOverviewSession.hpp"
#include "HyprlandConfigCompat.hpp"
#include "ScrollingLayoutAdapter.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/SessionLockManager.hpp>
#include <hyprland/src/protocols/ShortcutsInhibit.hpp>
#include <hyprland/src/protocols/InputCapture.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/layout/supplementary/DragController.hpp>

static bool rolodexInputBlocked() {
    return g_pSessionLockManager->isSessionLocked() || PROTO::shortcutsInhibit->isInhibited() ||
        PROTO::inputCapture->isCaptured() || g_layoutManager->dragController()->target();
}

void CExpoGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e);

    m_lastDelta   = 0.F;
    m_firstUpdate = true;
    m_monitor.reset();
    m_sessionGeneration = 0;

    const auto monitor = State::monitorState()->query().vec(g_pInputManager->getMouseCoordsInternal()).run();
    if (!monitor || !monitor->m_activeWorkspace)
        return;

    m_monitor = monitor;

    auto* const OV = overviewForMonitor(monitor);
    if (m_action == EExpoGestureAction::Rolodex) {
        if (!e.swipe || rolodexInputBlocked())
            return;
        const auto submap = g_pKeybindManager->getCurrentSubmap().name;
        if (!submap.empty() && !(OV && submap == "hyprexpo"))
            return;
        if (!OV && (overviewOpen() || CompatHyprlandAPI::intValue("plugin:hyprexpo:rolodex") == 0 ||
                    Hyprexpo::Scrolling::workspaceUsesScrollingLayout(monitor->m_activeWorkspace)))
            return;

        // Normal animated entry, not the grid's interactive open/close swipe.
        // Hyprland delivers the recognition event to update() immediately after
        // begin(): process it there exactly once, without dropping the handoff.
        auto* const TARGET = OV ? OV : createOverview(monitor);
        if (TARGET && !TARGET->closeCommitted() && TARGET->beginRolodexGesture())
            m_sessionGeneration = TARGET->sessionGeneration();
        return;
    }

    m_sessionGeneration = OV ? OV->sessionGeneration() : 0;
    if (m_action == EExpoGestureAction::Cancel) {
        if (!OV || OV->closeCommitted())
            return;

        OV->beginCancelSwipe();
        return;
    }

    if (!OV) {
        if (auto* const CREATED = createOverview(monitor, true))
            m_sessionGeneration = CREATED->sessionGeneration();
    }
    else if (!OV->closeCommitted()) {
        OV->selectHoveredWorkspace();
        OV->setClosing(true);
    }
}

IOverviewSession* CExpoGesture::overview() const {
    return overviewForSession(overviewMonitorKey(m_monitor.lock()), m_sessionGeneration);
}

void CExpoGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    auto* const OV = overview();
    if (!OV || OV->closeCommitted())
        return;

    if (m_action == EExpoGestureAction::Rolodex) {
        if (rolodexInputBlocked()) {
            OV->endRolodexGesture();
            m_sessionGeneration = 0;
            return;
        }
        // Raw signed vertical displacement keeps reversals continuous. Up moves
        // forward, matching natural two-finger scrolling; distance(e) would
        // normalize both initial directions to positive movement.
        if (e.swipe)
            OV->updateRolodexGesture(-e.swipe->delta.y * m_scale);
        return;
    }

    if (m_firstUpdate) {
        m_firstUpdate = false;
        return;
    }

    m_lastDelta += distance(e);

    if (m_lastDelta <= 0.01) // plugin will crash if swipe ends at <= 0
        m_lastDelta = 0.01;

    OV->onSwipeUpdate(m_lastDelta);
}

void CExpoGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    auto* const OV = overview();
    if (!OV || OV->closeCommitted())
        return;

    if (m_action == EExpoGestureAction::Rolodex) {
        // Both a lift and a cancelled swipe settle without committing a workspace.
        OV->endRolodexGesture();
        m_sessionGeneration = 0;
        return;
    }

    OV->setClosing(false);
    OV->onSwipeEnd(m_action == EExpoGestureAction::Expo);
    // onSwipeEnd can tear the overview down, so re-resolve before touching it.
    if (auto* const STILL_ALIVE = overview())
        STILL_ALIVE->resetSwipe();
}
