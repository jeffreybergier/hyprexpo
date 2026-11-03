#define WLR_USE_UNSTABLE

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/event/EventBus.hpp>

#include "Dispatchers.hpp"
#include "globals.hpp"
#include "IOverviewSession.hpp"
#include "OverviewCapture.hpp"
#include "PluginConfig.hpp"
#include "RolodexActivation.hpp"
#include "HyprlandConfigCompat.hpp"
#include "ScrollingLayoutAdapter.hpp"
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/SessionLockManager.hpp>
#include <hyprland/src/protocols/ShortcutsInhibit.hpp>
#include <hyprland/src/protocols/InputCapture.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/layout/supplementary/DragController.hpp>
#include <stdexcept>
#include <string>

#ifndef HYPREXPO_VERSION
#define HYPREXPO_VERSION "v0.0.0-dev"
#endif

// Methods
inline CFunctionHook* g_pRenderWorkspaceHook = nullptr;
inline CFunctionHook* g_pAddDamageHookA      = nullptr;
inline CFunctionHook* g_pAddDamageHookB      = nullptr;
typedef void (*origRenderWorkspace)(void*, PHLMONITOR, PHLWORKSPACE, const Time::steady_tp&, const CBox&);
typedef void (*origAddDamageA)(void*, const CBox&);
typedef void (*origAddDamageB)(void*, const pixman_region32_t*);

static void hkRenderWorkspace(void* thisptr, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now, const CBox& geometry) {
    auto* const OV = overviewForMonitor(pMonitor);

    if (!OV || isRenderingOverview() || OV->blocksOverviewRendering() || !OV->shouldRenderOverviewForMonitor(pMonitor))
        ((origRenderWorkspace)(g_pRenderWorkspaceHook->m_original))(thisptr, pMonitor, pWorkspace, now, geometry);
    else
        OV->render();
}

static void hkAddDamageA(void* thisptr, const CBox& box) {
    const auto PMONITOR   = (Monitor::CMonitor*)thisptr;
    const auto PMONITORSP = PMONITOR ? PMONITOR->m_self.lock() : PHLMONITOR{};

    auto* const OV = overviewForMonitor(PMONITORSP);

    if (!OV || !OV->shouldRenderOverviewForMonitor(PMONITORSP) || OV->blocksDamageReporting()) {
        ((origAddDamageA)g_pAddDamageHookA->m_original)(thisptr, box);
        return;
    }

    OV->onDamageReported();
}

static void hkAddDamageB(void* thisptr, const pixman_region32_t* rg) {
    const auto PMONITOR   = (Monitor::CMonitor*)thisptr;
    const auto PMONITORSP = PMONITOR ? PMONITOR->m_self.lock() : PHLMONITOR{};

    auto* const OV = overviewForMonitor(PMONITORSP);

    if (!OV || !OV->shouldRenderOverviewForMonitor(PMONITORSP) || OV->blocksDamageReporting()) {
        ((origAddDamageB)g_pAddDamageHookB->m_original)(thisptr, rg);
        return;
    }

    OV->onDamageReported();
}

static void failNotif(const std::string& reason) {
    HyprlandAPI::addNotification(PHANDLE, "[hyprexpo] Failure in initialization: " + reason, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != __hyprland_api_get_client_hash()) {
        failNotif("Version mismatch (headers ver is not equal to running hyprland ver)");
        throw std::runtime_error("[he] Version mismatch");
    }

    auto FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "renderWorkspace");
    if (FNS.empty()) {
        failNotif("no fns for hook renderWorkspace");
        throw std::runtime_error("[he] No fns for hook renderWorkspace");
    }

    g_pRenderWorkspaceHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkRenderWorkspace);

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "addDamageEPK15pixman_region32");
    if (FNS.empty()) {
        failNotif("no fns for hook addDamageEPK15pixman_region32");
        throw std::runtime_error("[he] No fns for hook addDamageEPK15pixman_region32");
    }

    g_pAddDamageHookB = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkAddDamageB);

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "_ZN7Monitor8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE");
    if (FNS.empty()) {
        failNotif("no fns for hook Monitor::CMonitor::addDamage(CBox)");
        throw std::runtime_error("[he] No fns for hook Monitor::CMonitor::addDamage(CBox)");
    }

    g_pAddDamageHookA = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkAddDamageA);

    bool success = g_pRenderWorkspaceHook->hook();
    success      = success && g_pAddDamageHookA->hook();
    success      = success && g_pAddDamageHookB->hook();

    if (!success) {
        failNotif("Failed initializing hooks");
        throw std::runtime_error("[he] Failed initializing hooks");
    }

    static auto P = Event::bus()->m_events.render.pre.listen([](PHLMONITOR pMonitor) {
        if (auto* const OV = overviewForMonitor(pMonitor))
            OV->onPreRender();
    });

    static auto PMONITORREMOVED = Event::bus()->m_events.monitor.removed.listen([](PHLMONITOR monitor) {
        if (auto* const OV = overviewForMonitor(monitor))
            destroyOverview(OV);
    });

    static Hyprexpo::RolodexActivation activation;
    static PHLMONITORREF activationMonitor;
    static auto PAXIS = Event::bus()->m_events.input.mouse.axis.listen([](const IPointer::SAxisEvent& event, Event::SCallbackInfo& info) {
        const auto monitor = State::monitorState()->query().vec(g_pInputManager->getMouseCoordsInternal()).run();
        if (activationMonitor.lock() != monitor)
            activation.reset();
        activationMonitor = monitor;
        const bool eligible = !info.cancelled && !overviewOpen() && monitor && monitor->m_activeWorkspace &&
            CompatHyprlandAPI::intValue("plugin:hyprexpo:rolodex") != 0 &&
            event.source == WL_POINTER_AXIS_SOURCE_FINGER && event.axis == WL_POINTER_AXIS_VERTICAL_SCROLL &&
            g_pInputManager->getModsFromAllKBs() == g_pKeybindManager->stringToModMask("SUPER") &&
            g_pKeybindManager->getCurrentSubmap().name.empty() && !g_pSessionLockManager->isSessionLocked() &&
            !PROTO::shortcutsInhibit->isInhibited() && !PROTO::inputCapture->isCaptured() && !g_layoutManager->dragController()->target() &&
            !Hyprexpo::Scrolling::workspaceUsesScrollingLayout(monitor->m_activeWorkspace);
        const auto remainder = activation.update(event.delta,
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(), eligible);
        if (!eligible)
            return;
        // Consume before creation: a newly registered overview listener must not
        // process this same event again, nor may the underlying client see it.
        info.cancelled = true;
        if (remainder) {
            if (auto* overview = createOverview(monitor))
                overview->seedRolodexScroll(*remainder);
        }
    });

    static auto PKEY = Event::bus()->m_events.input.keyboard.key.listen([](IKeyboard::SKeyEvent event, Event::SCallbackInfo& info) {
        activation.reset();
        if (shouldCancelOverview(event)) {
            info.cancelled = true;
            closeOverviews(false);
            return;
        }

        if (shouldSelectWorkspaceFromKey(event))
            info.cancelled = true;
    });

    static auto PCFG = Event::bus()->m_events.config.reloaded.listen([]() {
        activation.reset();
        Hyprexpo::Capture::notifyOverviewCaptureConfigReload();
        forEachOverview([](IOverviewSession& overview) { overview.onConfigReload(); });
        syncExpoGestureFromConfig();
    });

    registerHyprexpoDispatchers();

    registerHyprexpoConfigValues();

    HyprlandAPI::reloadConfig();

    syncExpoGestureFromConfig();

    return {"hyprexpo", "hyprexpo+ with keyboard selection, labels, and borders", "sandwich", HYPREXPO_VERSION};
}

APICALL EXPORT void PLUGIN_EXIT() {
    disableExpoGestureRegistration();

    destroyAllOverviews();
    g_pHyprRenderer->m_renderPass.removeAllOfType("COverviewPassElement");

    Config::mgr()->reload();
    resetDispatcherRuntime();
}
