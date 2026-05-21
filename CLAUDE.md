# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

Conan 2 + CMake. Qt HighGUI OpenCV build required (`WITH_QT=ON`) — app exits early otherwise.

```bash
./run.sh dependencies build execute   # first-time: install deps, build, run
./run.sh build execute                # fast rebuild + run
./run.sh clean                        # wipe build/
./log_run.sh <args>                   # same as run.sh, tees to global.log
```

Executable: `./build/opengl_template`. Working dir at runtime must be repo root (asset paths are relative).

There are no tests or lint steps configured.

## Architecture

Single-executable playground. All processing code lives in [main.cpp](main.cpp); reusable scaffolding sits in [src/](src/).

**Runtime loop** ([main.cpp:2240](main.cpp#L2240)): one Qt window (`"Image Processing"`) hosts both the output grid and the controls. A `Slider` owns a list of `SliderEntry` effects; arrow keys / `1..9` / Qt push buttons switch between them. On each iteration the current effect runs, fills an `OutputImages` vector, and [`renderGrid`](main.cpp#L2125) composites source + outputs into a uniform-cell canvas with label bars.

**Three collaborating pieces** — understand these before modifying the effect pipeline:

- [src/slider/slider.h](src/slider/slider.h) — `Slider` + `SliderEntry`. Entry holds the effect fn (`EffectFn2` or `EffectFn3`), its `TrackbarSpec`s, and its `RadioGroupSpec`s. Two overloaded constructors make the 2-arg form auto-wrap into the 3-arg form.
- [src/controls/controls_manager.h](src/controls/controls_manager.h) — `ControlsManager` rebuilds trackbars + ON/OFF checkboxes + radio groups in the main window whenever the active effect changes. Lookups are by name string.
- [src/common/output_images.h](src/common/output_images.h) — `OutputImages = vector<pair<string, cv::Mat>>`. Insertion order = grid order. Grayscale `Mat`s are auto-converted to BGR by `ensureBgr` during render.

**Selection state for click-driven effects**: [main.cpp:2110](main.cpp#L2110) registers `mainWindowMouseCallback`, which maps a click on the `Source` panel back to source-image coords and stashes the result in the global `g_selectionState`. Effects like `selected_object_features` read that struct.

## Adding a New Effect

The full walkthrough is in [EFFECTS.md](EFFECTS.md). Essentials:

1. Write a function in `main.cpp` with one of:
   ```cpp
   void my_effect(const cv::Mat& src, OutputImages& outputs);                            // no controls
   void my_effect(const cv::Mat& src, OutputImages& outputs, ControlsManager& controls); // with controls
   ```
   Push result(s) via `outputs.push_back({"Label", mat})`. Any number of outputs are supported — they all land in the grid.

2. Register a `SliderEntry` in the `Slider slider(...)` initializer list inside `main()` ([main.cpp:2275](main.cpp#L2275)). Wrap the function with `EffectFn2(fn)` or `EffectFn3(fn)` matching its signature. No other files change.

**Interactive controls — the rule for this codebase**: never roll your own sliders/buttons. Use the `ControlsManager` primitives:

- Continuous numeric parameter → `TrackbarSpec{name, default, max, neutral, enabled, min}` in the entry's `trackbars`. Read inside the effect with `controls.getEffective("name")` (honors the ON/OFF checkbox — returns `neutralValue` when OFF) or `controls.get("name")` (raw value, ignores toggle). Prefer `getEffective`.
- Discrete mutually-exclusive choice → `RadioGroupSpec{name, {"Opt A","Opt B",...}, defaultIdx}` in the entry's `radioGroups` (5th ctor arg). Read with `controls.getRadio("group name")` → 0-based index. Multiple groups per effect are fine.

Bounds are auto-clamped; `neutralValue` is what the effect sees when the user unchecks the parameter. Changing any control triggers re-render via the update callback wired in `main()`.

## Conventions

- Default image is loaded via `cv::IMREAD_GRAYSCALE` ([src/common/misc.h](src/common/misc.h) / loader). Color effects must `cvtColor(..., COLOR_GRAY2BGR)` first. `O` opens a file dialog at runtime.
- `Enter` saves every current output to `assets/export/` as `<epoch>_<label>.bmp` via `saveAllOutputs`.
- Asset paths go through the `IMAGE("subdir/name.bmp")` and `EXPORT(...)` macros in [src/common/paths.h](src/common/paths.h).
- Logging uses the `DEBUG/INFO/WARN/ERROR` macros from [src/common/logger/logger.h](src/common/logger/logger.h) (spdlog wrapper). Call `Logger::init()` / `Logger::destroy()` is handled in `main()`.
- `CMakeLists.txt` globs `*.cpp` under `src/` and builds `main.cpp` into target `opengl_template`. New files under `src/` are picked up automatically — do not append them manually to `add_executable`.

## Do Not

- Don't rename/remove `assets/` subfolders without being asked — paths are hard-coded via macros.
- Don't bypass `ControlsManager` (no raw `cv::createTrackbar` / `cv::createButton`); the slider re-activates controls on every effect switch and will clobber manual widgets.
- Don't break the `min/max/default/neutral/enabled` semantics of `TrackbarSpec` — other effects rely on the ON/OFF toggle matching `neutralValue`.
