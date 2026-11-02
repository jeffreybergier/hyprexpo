# Vertical Rolodex prototype

Local branch `prototype/vertical-rolodex`, based on sandwichfarm/hyprexpo
`5891014c611e1bd56d0121143f0221d46b5c0967`, matching Hyprland 0.56.2.
Locally maintained experiment; no remote fork or publication. The user approved
installing and keeping this prototype after the isolated interactive test.

## What it does

`plugin:hyprexpo:rolodex = 1` opts new grid overview sessions into a vertical
workspace stack. Default 0 preserves the grid. Native scrolling-layout sessions
remain their own existing mode. The workspace list still comes from the normal
columns/rows/workspace_method settings (use 5 columns, 2 rows for ten desktops).

- Uses the existing compositor-side workspace captures, not text placeholders.
- Perspective tilt around a horizontal axis; up to five cards are visible.
- 240ms eased rotation, interruptible and wrapping at either end.
- Wheel scrolling rotates one slot per event; finger-axis scrolling accumulates
  a 24-unit threshold. Works without holding a modifier once opened.
- Up/down (or left/right) keyboard navigation rotates.
- Click a rear card to bring it forward; click the selected card or Enter to
  select. Right-click or Escape cancels without switching workspaces.
- Window drag/drop is deliberately disabled in this mode.
- Captures are capped at 1920 pixels wide, preserving aspect ratio. This avoids
  allocating ten full 6K workspace buffers. Final drawing uses output scale.

This is not a finished renderer. Perspective is approximated with 96 UV-mapped
horizontal strips per tilted card, using Hyprland's color-managed rendering
path. It avoids custom GL state but increases draw calls and needs performance
profiling before daily use. The selected card has a single texture draw.
The previews inherit HyprExpo's capture/refresh behavior; they are not promised
to be continuous video streams of all inactive applications.

Entry/exit uses a basic fade rather than a seamless desktop-to-card transition.
No release-Super-to-confirm, continuous finger-following rotation, or global
two-finger-scroll activation is implemented. Rotated outputs and simultaneous
mixed-scale multi-monitor Rolodex sessions remain unverified.

## Build and test

Use the native Hyprland headers matching the running compositor, not the parent
Developer directory's Apple cross-compilation Compose image.

```sh
make test
make all
```

The header-only geometry tests cover wraparound, negative positions, projection,
monotonic strip boundaries, hit testing, animation endpoints, multiple workspace
counts, and proportional scaling. Source tests ensure the wheel hook is gated
on Rolodex mode, so ordinary grid sessions do not acquire axis ownership.
CMake and Meson also include the geometry test target.

Do not rebuild a `.so` while it is mapped by a running compositor. Close its
overview and unload it, or stop that disposable compositor before rebuilding.
Do not use `make install` for this experiment.

## Disposable interactive test

From a terminal inside the main graphical session:

```sh
HYPREXPO_DEV_LAYOUT=rolodex ./scripts/run-nested.sh
```

This opens a separate Hyprland desktop in a window and loads the local plugin
there. It does not replace the main session's hyprpm-managed plugin. F10 opens
its overview; wheel/two-finger scroll rotates; Enter/click selects; right-click
or Escape cancels. The nested desktop starts empty; its Super+Return opens a
terminal. Host global shortcuts may intercept Super chords, so F10 and plain
scroll are preferred when testing nested. Close the nested desktop when done.
The launcher suppresses systemd-session environment export from the nested
compositor. It writes its temporary configuration/plugin to the existing
`~/.cache/hyprexpo*` development paths, as upstream's launcher does.

For a future mouse-only main-session setup, both Super+mouse_down and
Super+mouse_up can open `hyprexpo.expo("on")`; the overview itself then owns wheel
input. The first wheel event opens it, subsequent events rotate. This must
replace the earlier Quickshell carousel bindings when deliberately deployed.
These main-session bindings were subsequently enabled during the approved
deployment described below.

## Approved laptop deployment

Installed the tested binary at `/var/cache/hyprpm/me/hyprexpo/hyprexpo.so` after
closing the overview and unloading the old plugin. Loaded it successfully,
enabled `rolodex = 1` in `~/.config/hypr/hyprland.lua`, and verified opening and
cancelling the native overview on the real display. Super+Tab, the Mission
Control key, and the desktop bar overview button now open the native version.
Super+mouse-wheel opens it for a mouse; a trackpad still requires a key/button
to open, then finger-axis scrolling works inside. The Quickshell mockup was
removed. The existing login `hyprpm reload` loads the installed replacement.

**A future `hyprpm update` or Hyprland upgrade can overwrite or invalidate this
local build.** Revalidate/rebuild against the new compositor before reloading it.
The displayed plugin version still uses the upstream base revision baked into
the tested binary; it is not a unique fork version yet.

Original compatible binary retained at
`~/.local/share/hyprexpo-rolodex/upstream-5891014.so`. For a quick visual fallback,
set `rolodex = 0` and reload Hyprland; the grid exists in the same binary.
For full binary rollback, first remove the `rolodex` config key, close the
overview, unload the installed plugin, use `sudo install -m 755` to restore that
original binary to the hyprpm path, load it, and reload the configuration. Never
overwrite a mapped plugin in place. Sync config edits with chezmoi.

## Smoke-test results

Tested on this laptop against Hyprland 0.56.2 in a disposable Wayland-backed
session, then on its headless output:

- Plugin build/load and config validation succeeded.
- Native workspace snapshots visually inspected at 1280x800.
- Virtual-pointer protocol injection: wheel +15 advanced workspace 1 selection
  to 2; keyboard confirmation switched to 2.
- Finger-axis -24 returned selection to 1; a pointer click selected it.
- Wheel -15 from workspace 1 wrapped to 10; confirmation created empty workspace
  10. Right-click after changing selection preserved workspace 10.
- 6016x3384 headless output at scale 2 rendered and was captured successfully;
  no physical 6K monitor or performance/latency claim is implied.
- Setting rolodex back to 0 restored the original grid, visually inspected.
- Disposable session exited; main session plugin and session environment unchanged.

An initial headless-only compositor launch failed during backend initialization,
before plugin testing. Wayland-backed startup with a headless output worked.
Removing outputs mid-test orphaned fixture workspaces; reassigning them to the
test output allowed navigation tests to proceed. Hot-unplug behavior is not
claimed as validated by this prototype.
