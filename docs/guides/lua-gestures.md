# Lua Gestures

HyprExpo exposes helpers under `hl.plugin.hyprexpo` when Hyprland is using Lua config support.

```lua
hl.plugin.hyprexpo.expo("toggle")
hl.plugin.hyprexpo.expo("cancel")
hl.plugin.hyprexpo.kb_focus("left")
hl.plugin.hyprexpo.kb_confirm()
hl.plugin.hyprexpo.kb_selecti(1)
hl.plugin.hyprexpo.kb_selectn(1)
hl.plugin.hyprexpo.kb_select("1")
```

## Loading a Local Build

If you load a locally built `hyprexpo.so` directly from a Lua config, declare
its path on every config pass:

```lua
local plugin_path = "/absolute/path/to/hyprexpo.so"

-- This declares the desired plugin; it does not load the library immediately.
hl.plugin.load(plugin_path)

local hyprexpo_loaded = false
for _, plugin in ipairs(hl.get_loaded_plugins()) do
    if plugin.name == "hyprexpo" then
        hyprexpo_loaded = true
        break
    end
end

if hyprexpo_loaded then
    -- Put hl.config(), bindings, and immediate hl.plugin.hyprexpo calls here.
end
```

`hl.plugin.load()` is declarative. Hyprland loads the plugin after the first
config pass and reloads the config so its options and Lua functions are
available. Do not wrap `hl.plugin.load()` itself in the loaded-plugin check.
Omitting the path on the second pass tells Hyprland that the config no longer
wants the plugin, which can cause a repeating load/reload/unload cycle.

This is only needed for a local build loaded from the Lua config. `hyprpm`
manages its enabled plugin separately.

## Plugin Configuration

Configure HyprExpo options with `hl.config()` and a nested `plugin.hyprexpo`
table:

```lua
hl.config({
    plugin = {
        hyprexpo = {
            columns = 3,
            gaps_in = 5,
            gaps_out = 0,
            bg_col = "rgb(111111)",
            workspace_method = "center current",
            gesture_distance = 200,
            cancel_key = "escape",
            show_cursor = 1,
        },
    },
})
```

The `hl.plugin.hyprexpo` table shown above is for helper functions such as
`expo`, `kb_focus`, and `gesture`. Calling it as a function, or passing the
configuration table to `hl.plugin.hyprexpo.expo`, will raise a Lua type error.

## Keyboard Submap

Lua configs define submaps with Hyprland's Lua API. If you want modifierless
navigation while the overview is open, define the `hyprexpo` submap in
`hyprland.lua`:

```lua
hl.define_submap("hyprexpo", function()
    hl.bind("h",      function() hl.plugin.hyprexpo.kb_focus("left") end)
    hl.bind("l",      function() hl.plugin.hyprexpo.kb_focus("right") end)
    hl.bind("k",      function() hl.plugin.hyprexpo.kb_focus("up") end)
    hl.bind("j",      function() hl.plugin.hyprexpo.kb_focus("down") end)
    hl.bind("return", function() hl.plugin.hyprexpo.kb_confirm() end)
    hl.bind("escape", function() hl.plugin.hyprexpo.expo("cancel") end)
end)
```

HyprExpo still enters the submap named `hyprexpo` automatically when
`plugin:hyprexpo:keynav_enable` is enabled. The important part for Lua users is
that the submap's binds are registered with `hl.define_submap`, not with a
hyprlang `submap = hyprexpo` block.

## Gesture Setup

Gestures are configured from Lua:

```lua
hl.plugin.hyprexpo.gesture({
    fingers = 4,
    direction = "up",
    action = "expo",
})

hl.plugin.hyprexpo.gesture({
    fingers = 4,
    direction = "down",
    action = "cancel",
})
```

The `expo` action keeps the existing follow-your-finger behavior: it opens the
overview when closed. In grid mode, an expo close gesture selects the hovered
workspace. In native scrolling mode, both expo-close and cancel swipes keep
the opening workspace; pointer selection and keyboard confirmation commit the
chosen target separately.
The `cancel` action is inert while the overview is closed. While it is open, a
cancel swipe starts an interactive close without selecting the hovered tile. A
completed cancel swipe returns to the workspace where the overview opened; an
incomplete swipe restores the still-open overview. Each cancel swipe targets
the opening workspace again, including after an incomplete expo close was
previously aimed at another tile.

### Continuous Rolodex browsing (local build)

With `plugin.hyprexpo.rolodex = 1`, register a native vertical browsing gesture:

```lua
hl.plugin.hyprexpo.gesture({
    fingers = 3,
    direction = "vertical",
    action = "rolodex",
    scale = 1.25, -- 25% faster than the default; affects only this gesture.
})
```

Unlike `expo`, this action opens the overview and feeds the **same swipe** into
its continuous rotation handler. Up advances, down reverses, and changing direction
mid-swipe retraces the movement. The recognition update is processed once; no lift
or second swipe is needed. Holding still with fingers down does not trigger the
axis-scroll idle snap. **Lifting selects the nearest workspace card and closes
the overview**, without an extra click or keypress. A compositor-cancelled or
inhibited gesture settles without selecting. Escape/right-click cancels, and
late release events cannot select a cancelled or replacement overview.

For manual selection, open with the Mission Control/F3 key or the overview
button. That entry remains open for two-finger scrolling, wheel or arrow-key
navigation, then click/Enter selection. A three-finger vertical swipe inside a
manually opened overview still opts into selection on lift.

`rolodex_scroll_distance` controls speed (shared with two-finger scrolling);
`scale` can tune this gesture independently: `1.25` produces 25% more movement
for the same swipe, without changing pointer speed or two-finger scrolling.
Only `vertical`, `up`, and `down`
are accepted for this action. It does not open grid/native scrolling-layout
sessions, bypass locks/inhibition/input capture, or activate inside unrelated
submaps. Three-finger horizontal strokes are ignored while any Rolodex overview
is open (including its closing animation), preventing workspace switches behind
the cards. A blocked stroke stays blocked until lift/cancellation, even if the
overview closes first. Vertical browsing still accepts sideways drift after
recognition. Horizontal workspace swipes work normally on the desktop again;
Super+scroll is unaffected.

`gesture` accepts this table shape:

| field | type | required | description |
| --- | --- | --- | --- |
| `fingers` | integer | yes | number of fingers |
| `direction` | string | yes | swipe direction |
| `action` | string | no | `expo`, `cancel`, `rolodex` (local build), or `unset`; defaults to `expo` |
| `mods` | string | no | modifier expression passed to Hyprland |
| `scale` | number | no | gesture scale; defaults to `1.0` |
| `disable_inhibit` | boolean | no | whether to bypass inhibit handling |

Use `unset` with the same fingers, direction, modifiers, scale, and inhibit
settings to remove a registered gesture.

## Argument Validation

Lua arguments are validated strictly:

| function | accepted arguments |
| --- | --- |
| `expo` | string, defaulting to `"toggle"` when omitted |
| `kb_focus` | string |
| `kb_confirm` | none |
| `kb_selecti` | Lua integer or exact integer string |
| `kb_selectn` | Lua integer or exact integer string |
| `kb_select` | string |

Fractional numbers, booleans, tables where strings are expected, and partial numeric strings are rejected instead of being silently coerced.
