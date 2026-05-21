# Image Processing Laboratory — Documentation

C++20 / OpenCV playground for the UTCN PI (Image Processing) course. Single-window Qt application where you cycle through image-processing effects with keyboard shortcuts and interact via trackbars, radio buttons, and mouse clicks.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Build & Run](#build--run)
3. [VSCode / Clangd Setup](#vscode--clangd-setup)
4. [Application Flow](#application-flow)
5. [Architecture](#architecture)
6. [Effects Reference](#effects-reference)
7. [Adding a New Effect](#adding-a-new-effect)
8. [Helper Modules](#helper-modules)
9. [Configuration](#configuration)
10. [Keyboard & Mouse Controls](#keyboard--mouse-controls)

---

## Prerequisites

- **CMake ≥ 3.21**
- **OpenCV** built with Qt backend (`WITH_QT=ON`). On Arch Linux: `sudo pacman -S opencv` (check that the package was compiled with Qt — the AUR `opencv-qt` is safer if not)
- **spdlog**: `sudo pacman -S spdlog`
- **C++20-capable compiler** (GCC 10+ or Clang 13+)
- Qt5 or Qt6 (pulled in transitively by OpenCV-Qt)

Verify OpenCV has Qt support at runtime — the app exits immediately with an error message if it does not.

---

## Build & Run

```bash
# Configure (Debug build, exports compile_commands.json for clangd)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
cmake --build build -j$(nproc)

# Run (must be from repo root — asset paths are relative)
./build/opengl_template
```

For a Release build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j$(nproc)
./build/opengl_template
```

The executable name is `opengl_template` (legacy project name, not indicative of content).

---

## VSCode / Clangd Setup

Clangd needs `compile_commands.json` to resolve OpenCV include paths. After any CMake configure:

```bash
ln -sf build/compile_commands.json compile_commands.json
```

Recommended `.vscode/settings.json`:

```json
{
  "clangd.arguments": [
    "--compile-commands-dir=${workspaceFolder}",
    "--background-index",
    "--clang-tidy"
  ]
}
```

Without the symlink, clangd will show `'opencv2/opencv.hpp' file not found` on every file — these are IDE-only errors and do not affect the build.

---

## Application Flow

```
main()
  │
  ├─ Logger::init()
  ├─ hasQtBackend() check            ← exits if OpenCV lacks Qt
  ├─ cv::namedWindow(MAIN_WINDOW_NAME)
  ├─ loadImage(default_path, img)    ← loads assets/images/PI-L4/trasaturi_geom.bmp
  ├─ ControlsManager controls(...)   ← owns the trackbar/radio panel
  ├─ Slider slider({...}, controls)  ← owns all SliderEntry effects
  │
  └─ while (running):
       ├─ slider.applyPendingSelection()   ← Qt button press deferred to main thread
       ├─ if effect changed → updateMainWindowTitle(name)
       │                    → g_selectionState.dirty = true
       ├─ lastOutputs = slider.exec(img)   ← calls current effect fn
       ├─ renderGrid(img, lastOutputs)     ← composites source + outputs into canvas
       ├─ cv::setMouseCallback(...)        ← registers click handler every frame
       ├─ keyCode = WaitKey(30)            ← 30ms poll
       └─ switch(keyCode):
            LEFT/RIGHT  → slider.previous() / slider.next()
            1..9        → slider.selectByIndex(n)
            O           → file dialog → loadImage()
            Enter       → saveAllOutputs(lastOutputs)
            Space       → slider.reactivateControls()
            ESC / X     → running = false
```

### Effect Execution

`slider.exec(img)` calls the current `SliderEntry::process` function, which has the internal signature:

```cpp
void(const cv::Mat& src, OutputImages& outputs, ControlsManager& controls)
```

Effects append named images to `outputs`. `renderGrid` then lays them out in a grid alongside the source image. Every call to `exec` re-runs the effect from scratch — there is no incremental update.

### Grid Rendering

`renderGrid` computes a square grid large enough to hold `1 + outputs.size()` panels. All panels are resized to the same cell dimension (the maximum width and height across all images). The first panel is always "Source". Each panel has a 30px label bar at the top. The canvas is displayed via `cv::imshow(MAIN_WINDOW_NAME, canvas)`.

The `RenderContext` struct (in `g_renderContext`) is updated every frame with the current grid geometry so that mouse clicks can be mapped back to source-image coordinates.

### Click-to-Source Mapping

`mainWindowMouseCallback` fires on left-click anywhere in the window. It calls `mapMainWindowPointToSource` which uses `g_renderContext` to convert the click pixel into source-image coordinates, then stores the result in `g_selectionState`. Click-driven effects (`selected_object_features`, `contour_and_region_fill_lab7`) read `g_selectionState` each frame.

---

## Architecture

```
pi-cv/
├── main.cpp                         ← entry point, Slider registration, key loop
├── CMakeLists.txt                   ← globs src/**/*.cpp automatically
├── assets/
│   ├── images/                      ← input images (PI-L3/, PI-L4/, PI-L6/, ...)
│   └── export/                      ← Enter-key output BMP dumps
└── src/
    ├── common/
    │   ├── common.h                 ← umbrella include for all common headers
    │   ├── paths.h                  ← IMAGE(), EXPORT() macros
    │   ├── misc.h                   ← KEY enum, resolvedKey(), WaitKey macro
    │   ├── output_images.h          ← OutputImages = vector<pair<string, Mat>>
    │   ├── logger/logger.h          ← INFO/DEBUG/WARN/ERROR spdlog macros
    │   └── file/file_utils.h        ← FileUtils::readImage/saveImage/readFile/openFileDialog
    ├── controls/
    │   └── controls_manager.h/.cpp  ← trackbar + radio-button panel
    ├── slider/
    │   └── slider.h/.cpp            ← Slider + SliderEntry carousel
    ├── color_spaces/
    │   └── spaces.h/.cpp            ← HSV/RGB conversion used by hsv_hue_quantization
    ├── helpers/                     ← domain-logic utilities, no effects
    │   ├── types.h                  ← shared enums, constants, structs
    │   ├── global_state.h/.cpp      ← g_renderContext, g_selectionState, g_needsUpdate
    │   ├── image_utils.h/.cpp       ← grayscale/BGR conversion, foreground masks
    │   ├── labeling.h/.cpp          ← connected-component labeling, colorization
    │   ├── morphology.h/.cpp        ← dilation, erosion, opening, closing, fill
    │   ├── chain_code.h/.cpp        ← chain code encode/decode/log/parse
    │   ├── histogram.h/.cpp         ← histogram, PDF, CDF, mean/stddev, draw
    │   ├── geometry.h/.cpp          ← object stats, projections, angle math
    │   └── rendering.h/.cpp         ← renderGrid, loadImage, mouse callback, MAIN_WINDOW_NAME
    └── effects/                     ← one .h + .cpp per effect function
        ├── assignment.h/.cpp        ← contour_similarity (do not modify)
        ├── negative.h/.cpp
        ├── bi_level_color_map.h/.cpp
        └── ... (23 total)
```

### Key Types

| Type | Defined in | Purpose |
|------|-----------|---------|
| `OutputImages` | `src/common/output_images.h` | `vector<pair<string, Mat>>` — named outputs |
| `EffectFn2` | `src/slider/slider.h` | `void(const Mat&, OutputImages&)` |
| `EffectFn3` | `src/slider/slider.h` | `void(const Mat&, OutputImages&, ControlsManager&)` |
| `TrackbarSpec` | `src/controls/controls_manager.h` | One numeric slider definition |
| `RadioGroupSpec` | `src/controls/controls_manager.h` | One set of mutually-exclusive buttons |
| `SliderEntry` | `src/slider/slider.h` | Named effect + its controls |
| `NeighborhoodType` | `src/helpers/types.h` | `N4` or `N8` neighborhood |
| `SelectionState` | `src/helpers/types.h` | Last mouse click in source-image coords |
| `ObjectStats` | `src/helpers/types.h` | Geometric properties of one labeled region |
| `RenderContext` | `src/helpers/types.h` | Grid geometry for click mapping |
| `ChainCodeTrace` | `src/helpers/types.h` | Chain code + derivative + traced points |

### Global State

Three mutable globals defined in `src/helpers/global_state.cpp`:

| Global | Type | Written by | Read by |
|--------|------|-----------|---------|
| `g_renderContext` | `RenderContext` | `renderGrid` | `mapMainWindowPointToSource` |
| `g_selectionState` | `SelectionState` | `mainWindowMouseCallback`, `loadImage` | Click-driven effects |
| `g_needsUpdate` | `bool` | `mainWindowMouseCallback`, `loadImage`, `main` | `main` loop |

Effects that need click data include `src/helpers/global_state.h`.

---

## Effects Reference

Effects are grouped by lab. Use Left/Right arrows or `1`–`9` to navigate. The window title shows the active effect name.

### Lab 3 — Intensity & Colour

#### Bi-Level (`bi_level_color_map`)
Converts to grayscale, then thresholds: pixels > T → 255, else 0.

| Control | Range | Default | Meaning |
|---------|-------|---------|---------|
| Threshold | 0–255 | 127 | Threshold value T |

Output: `Bi-Level`

---

#### Negative (`negative`)
Inverts each pixel: `dst = 255 - src`. Grayscale input only.

No controls. Output: `Negative`

---

#### Additive Factor (`additive_factor`)
Adds a signed offset to each pixel with saturation clamping to [0, 255].

| Control | Range | Default | Meaning |
|---------|-------|---------|---------|
| Additive Factor | 0–255 | 128 | Added as-is (set to 128 for zero net change) |

Output: `Adjusted`

---

#### Multiplicative (`multiplicative`)
Multiplies each pixel by a scalar using OpenCV saturating arithmetic.

| Control | Range | Default | Meaning |
|---------|-------|---------|---------|
| Multiplicative | 0–35 | 1 | Scale factor |

Output: `Multiplied`

---

#### View Placeholder (`view_placeholder`)
Static 256×256 four-quadrant reference image: white / red / green / yellow. Useful as a sanity check that the rendering pipeline is working.

No controls. Output: `Placeholder`

---

### Lab 3 — Histogram & Quantization

#### RGB Channels (`rgb_channels`)
Splits the source into B, G, R channels and displays each.

| Control | Options | Default | Meaning |
|---------|---------|---------|---------|
| Channel Mode | Colored / Grayscale | Grayscale | Colored: channel tinted with its own color; Grayscale: raw channel values |

Outputs: `Blue`, `Green`, `Red`

---

#### Histogram & PDF (`histogram_and_pdf`)
Computes the 256-bin grayscale histogram and its normalized probability density function (PDF = histogram / pixel count).

No controls. Outputs: `Histogram`, `PDF`

---

#### Histogram (m bins) (`histogram_m_bins`)
Same as above but with a user-selected number of bins m. Bins are formed by mapping `[0, 255]` uniformly into m buckets.

| Control | Range | Default | Meaning |
|---------|-------|---------|---------|
| Bins (m) | 2–256 | 256 | Number of histogram bins |

Output: `Histogram (m bins)`

---

#### Gray Level Reduction (`gray_level_reduction`)
Uniform quantization: maps each pixel to one of WL evenly-spaced output levels. Formula: `q = floor(g * WL / 256)`, output = `q * 255 / (WL - 1)`.

| Control | Range | Default | Meaning |
|---------|-------|---------|---------|
| Levels (WL) | 2–128 | 4 | Number of output gray levels |

Outputs: `Reduced`, `Histogram`

---

#### Floyd-Steinberg (`floyd_steinberg`)
Error-diffusion dithering. Quantizes each pixel to the nearest of WL levels and propagates the quantization error to right and lower-right neighbors using the Floyd-Steinberg kernel (7/16, 3/16, 5/16, 1/16).

| Control | Range | Default | Meaning |
|---------|-------|---------|---------|
| Levels (WL) | 2–128 | 4 | Number of output gray levels |

Outputs: `Floyd-Steinberg`, `Histogram`

---

#### HSV Hue Quantization (`hsv_hue_quantization`)
Converts BGR pixels to HSV, quantizes the hue channel to `levels` evenly-spaced hue values (round to nearest multiple of 360/levels), then converts back to BGR.

| Control | Range | Default | Meaning |
|---------|-------|---------|---------|
| Hue Levels | 2–128 | 6 | Number of distinct hue values |
| S/V Mode | Keep Original / Set to Max | Keep Original | When "Set to Max": forces S=100% and V=100%, showing pure saturated colors |

Output: `HSV Quantized`

---

### Lab 7 — Morphology & Contours

#### Morphology Lab7 (`morphology_lab7_combined`)
Applies morphological operations with an 8-neighborhood structuring element. The input is auto-binarized via `makeLab7ForegroundMask` (Otsu threshold, polarity chosen to minimize border-touching foreground pixels).

| Control | Range | Default | Meaning |
|---------|-------|---------|---------|
| Iterations (N) | 1–15 | 1 | N applications of each operation |

Outputs:
- `Binary (Lab7)` — binarized input used as baseline
- `Dilate xN` — binary dilated N times
- `Erode xN` — binary eroded N times
- `Open xN` — opening (erode then dilate) repeated N times
- `Close xN` — closing (dilate then erode) repeated N times
- `Open x1` — single opening pass (idempotence reference)
- `Close x1` — single closing pass (idempotence reference)

---

#### Contour + Fill Lab7 (`contour_and_region_fill_lab7`)
Extracts the boundary of foreground objects via `β(A) = A − (A ⊖ B)` (boundary = object minus eroded object). Then performs morphological region filling from a user-clicked seed point using `Xk = (Xk−1 ⊕ B) ∩ Aᶜ` until convergence.

| Control | Range | Default | Meaning |
|---------|-------|---------|---------|
| Max Fill Iter | 1–50000 | 3000 | Safety cap on filling iterations |

**Mouse**: left-click inside a background hole/region to set the fill seed. Clicking on a foreground pixel is rejected.

Outputs:
- `Contour` — source image with boundary pixels overlaid in red
- `Region Fill` — color-coded fill result (blue = original object, green = filled area, yellow marker = seed)
- `Filled Mask` — binary mask of fully filled object (only when fill succeeds)

---

#### Boundary Margin Code (`boundary_margin_code`)
Traces the boundary of every black-foreground object using `cv::findContours`, then encodes the contour as a chain code (N4 or N8). Prints the code, its circular derivative, and the closed versions to stdout.

| Control | Options | Default |
|---------|---------|---------|
| Neighborhood | N4 / N8 | N8 |

Output: `Boundary Margin` — source image with traced contour overlaid in magenta.

Console output per component:
```
[Boundary Code] Component=1 Neighborhood=N8 Start=(x, y) Length=N
[Boundary Code] Code=0 1 2 ...
[Boundary Code] ClosedCode=0 1 2 ... 0 1
[Boundary Code] Derivative=1 3 2 ...
[Boundary Code] ClosedDerivative=1 3 2 ... 1 3
```

---

#### Boundary Reconstruct (`boundary_reconstruct_from_code`)
Reads a chain-code file from `assets/images/PI-L6/reconstruct.txt` and reconstructs the polyline.

File format:
```
<start_x> <start_y>
<declared_length>
<code_value_0> <code_value_1> ...
```

| Control | Options | Default |
|---------|---------|---------|
| Neighborhood | N4 / N8 | N8 |

Output: `Reconstructed Boundary` — white canvas with the reconstructed polyline in magenta and a green cross at the start point.

---

### Lab 4 — Geometric Features

#### Selected Object Features (`selected_object_features`)
Labels all objects in the source image, then computes geometric properties for the object under the mouse click.

No controls. **Mouse**: left-click on a foreground object in the Source panel.

Computed properties (printed to stdout and overlaid):
- **Area** — pixel count (zeroth moment)
- **Centroid** (x̄, ȳ) — first-order moments
- **Orientation** — principal axis angle in degrees, normalized to [−90°, 90°)
- **Perimeter** — summed contour arc length
- **Thinness ratio** — `4π × area / perimeter²` (circle = 1.0)
- **Aspect ratio** — major axis / minor axis
- **Major/minor axis length** — derived from second-order moments

Outputs:
- `Labels` — all objects colorized by label
- `Selected Object` — source with contour points in red, centroid cross in green, orientation axis in blue, label text
- `Projections` — combined X/Y row and column projection histograms of the selected object

---

#### Filter Area+Orientation (`filter_objects_by_area_orientation`)
Keeps only objects satisfying both an area constraint and an orientation constraint. Useful for isolating objects by shape.

| Control | Range | Default | Meaning |
|---------|-------|---------|---------|
| TH_area | 1–200000 | 1000 | Keep objects with area < TH_area |
| phi_LOW | 0–180 | 0 | Lower orientation bound (mapped to [−90°, 90°)) |
| phi_HIGH | 0–180 | 180 | Upper orientation bound (mapped to [−90°, 90°)) |

Wrapping: if `phi_LOW > phi_HIGH` after normalization, the range wraps (e.g. 80° to −80° keeps objects near ±90°).

Outputs: `Filtered Objects`, `Filtered Labels`

---

#### Labeling Compare (`labeling_compare`)
Runs two connected-component labeling algorithms on the same binary image and displays their results side by side for comparison.

| Control | Options | Default |
|---------|---------|---------|
| Neighborhood | N4 / N8 | N8 |
| Traversal | BFS / DFS | BFS |

Algorithms:
- **Traversal** (BFS or DFS): flood-fill from each unlabeled foreground pixel
- **Two-Pass**: first pass assigns provisional labels and builds an equivalence table; second pass resolves equivalences and compacts label IDs

Outputs: `Binary`, `Traversal Labels`, `Two-Pass First Pass`, `Two-Pass Final`

---

### Lab 8 — Statistical Properties

#### Lab8 Statistics (`lab8_statistics`)
Computes and displays the full statistical profile of the grayscale image.

No controls.

Outputs:
- `Histogram` — 256-bin bar chart with μ and σ printed in red
- `Cumulative` — cumulative distribution function (CDF) normalized to [0, 1]
- `PDF` — probability density function bar chart

Mean and stddev are also logged: `[Lab8] mean=... stddev=...`

---

#### Lab8 Auto Binarize (`lab8_auto_binarize`)
Iterative automatic thresholding (bisection method):
1. Set T = (Imin + Imax) / 2
2. Split histogram at T; compute means μ1 (below T) and μ2 (above T)
3. Update T = (μ1 + μ2) / 2
4. Repeat until |ΔT| < ε

| Control | Range | Default | Meaning |
|---------|-------|---------|---------|
| Epsilon x100 | 1–500 | 10 | ε × 100 (default → ε = 0.10) |

Output: `Binary` — binarized image with T value and iteration count annotated.

Console: `[Lab8] auto-binarize: Imin=... Imax=... T=... iter=... eps=...`

---

#### Lab8 Transforms (`lab8_transforms`)
Applies four pointwise intensity transforms simultaneously, each with its own control, and shows the resulting histogram for each.

| Control | Range | Default | Meaning |
|---------|-------|---------|---------|
| Iout Min | 0–255 | 30 | Output minimum for contrast stretch |
| Iout Max | 0–255 | 220 | Output maximum for contrast stretch |
| Gamma x10 | 1–30 | 10 | Gamma value × 10 (default → γ = 1.0) |
| Brightness Offset | 0–255 | 128 | Added offset − 128 (128 → no change) |

Transforms:
- **Negative**: `dst = 255 − src`
- **Contrast stretch**: linear remap from `[Imin_src, Imax_src]` → `[Iout Min, Iout Max]`
- **Gamma**: `dst = 255 × (src/255)^γ`
- **Brightness**: `dst = clamp(src + offset, 0, 255)`

Outputs: `Negative`, `Contrast`, `Gamma`, `Brightness`, `Hist Original`, `Hist Negative`, `Hist Contrast`, `Hist Gamma`, `Hist Brightness`

---

#### Lab8 Equalize (`lab8_histogram_equalization`)
Histogram equalization using the CDF as a remapping curve:
`lut[i] = round(255 × CDF(i))` where `CDF(i) = Σ h(j)/N` for j ≤ i.

No controls.

Outputs:
- `Equalized`
- `Hist Original` — annotated with μ and σ
- `Hist Equalized` — annotated with μ and σ

Console: `[Lab8] equalize: orig mu=... sigma=...  eq mu=... sigma=...`

---

### Assignment

#### Contour Similarity (`contour_similarity`)
Labels all black-foreground objects, traces extended contours using a corner-following LUT (square-tracing algorithm), and computes pairwise similarity between all chain codes via circular cross-correlation with rotational invariance.

| Control | Range | Default | Meaning |
|---------|-------|---------|---------|
| Threshold x100 | 0–100 | 80 | Similarity threshold × 100 (draw link if sim > thresh) |

Results are cached — the heavy computation only reruns when the source image changes.

Outputs:
- `Contours` — objects black, contour pixels colored randomly per label
- `Similarity Links` — same image with yellow lines connecting similar object centroids

Similarity matrix is printed to stdout.

---

### Spatial Filter (`general_filter`)

Incomplete stub for a DFT-based spatial filter. Currently outputs a raw `Fourier` panel. Not functional.

---

## Adding a New Effect

### 1. Create the header — `src/effects/my_effect.h`

```cpp
#pragma once
#include "src/common/output_images.h"
#include "src/controls/controls_manager.h"
#include <opencv2/opencv.hpp>

// Use this signature if you need controls:
void my_effect(const cv::Mat &src, OutputImages &outputs, ControlsManager &controls);

// Or this if you don't:
// void my_effect(const cv::Mat &src, OutputImages &outputs);
```

### 2. Create the implementation — `src/effects/my_effect.cpp`

```cpp
#include "my_effect.h"
// Include any helpers you need:
#include "src/helpers/image_utils.h"
#include "src/helpers/histogram.h"
// etc.

void my_effect(const cv::Mat &src, OutputImages &outputs, ControlsManager &controls) {
    int threshold = controls.getEffective("My Threshold");

    cv::Mat gray = toGray8U(src);
    // ... process ...

    outputs.push_back({"Result", gray});          // grayscale — auto-converted to BGR by renderGrid
    outputs.push_back({"Histogram", histImg});     // multiple outputs are all shown in the grid
}
```

**Rules:**
- Never call `cv::createTrackbar` or `cv::createButton` directly — use `ControlsManager`
- Push every output you want displayed via `outputs.push_back({"Label", mat})`
- Grayscale mats are fine — `renderGrid` converts them to BGR for display
- Order of `push_back` calls determines left-to-right, top-to-bottom grid order

### 3. Register in `main.cpp`

Add the include:
```cpp
#include "src/effects/my_effect.h"
```

Add an entry to the `Slider slider({...})` initializer list:

```cpp
// No controls (2-arg effect):
{"My Effect Name",
 EffectFn2(my_effect),
 {} /* no trackbars */,
 "Short description of what this does"},

// With trackbars and a radio group (3-arg effect):
{"My Effect Name",
 EffectFn3(my_effect),
 {{"My Threshold", /*default*/ 100, /*max*/ 255, /*neutral*/ 0, /*enabled*/ true, /*min*/ 0}},
 "Short description",
 {{"Mode", {"Option A", "Option B"}, /*defaultIndex*/ 0}},
 "Lab X — group label"},   // optional: groups effects under a header in the sidebar
```

No CMakeLists change — `src/**/*.cpp` is globbed automatically.

### TrackbarSpec fields

```cpp
struct TrackbarSpec {
    std::string name;       // lookup key for controls.getEffective("name")
    int defaultValue;       // initial trackbar position
    int maxValue;           // maximum trackbar position
    int neutralValue = 0;   // value returned by getEffective() when toggle is OFF
    bool enabled = true;    // initial toggle state (ON/OFF checkbox)
    int minValue = 0;       // minimum trackbar position (value is shifted internally)
};
```

Read values in your effect:
```cpp
int val = controls.getEffective("My Threshold");  // returns neutralValue when toggled OFF
int raw = controls.get("My Threshold");           // always returns the trackbar position
int mode = controls.getRadio("Mode");             // 0-based index of selected radio option
```

### Using Click State

If your effect needs a mouse-click position (like `selected_object_features`):

```cpp
#include "src/helpers/global_state.h"

void my_effect(const cv::Mat &src, OutputImages &outputs, ControlsManager &) {
    if (!g_selectionState.hasClick) {
        // show prompt
        return;
    }
    cv::Point p = g_selectionState.imagePoint;  // in source-image coordinates
    // use p...
    g_selectionState.dirty = false;  // reset after consuming the click info
}
```

---

## Helper Modules

### `src/helpers/types.h`
Header-only. All shared types:
- `NeighborhoodType` — `N4` or `N8`
- `kNeighbors4`, `kNeighbors8` — 4/8-neighborhood offset arrays
- `kChainDirections4`, `kChainDirections8` — chain-code direction offset arrays
- `RenderContext` — grid geometry for click mapping
- `ObjectStats` — geometric properties (area, centroid, orientation, perimeter, thinness ratio, aspect ratio, axis lengths)
- `SelectionState` — last click coordinates, selected label, dirty flag
- `ChainCodeTrace` — start point, code, derivative, traced points
- `ChainCodeFileData` — parsed chain-code file contents
- `TwoPassLabelingResult` — first-pass and final label images

---

### `src/helpers/image_utils.h`
Basic image format conversions and mask generation.

| Function | Description |
|----------|-------------|
| `toGray8U(src)` | Convert any image to 8-bit single-channel. If already grayscale: noop. If multi-channel: cvtColor. If non-8U: normalize and convert. |
| `ensureBgr(image)` | Return a 3-channel BGR image. Clones if already BGR, converts if grayscale. |
| `makeBinaryForeground(src)` | Otsu-threshold to binary (white = foreground). |
| `makeBlackForegroundMask(src)` | Inverted Otsu threshold — black pixels become 255 (foreground). |
| `countBorderForeground(binary)` | Count non-zero pixels on the outer border of a binary image. Used to detect polarity. |
| `makeLab7ForegroundMask(src)` | Auto-detect polarity: applies Otsu in both polarities, picks the one with fewer border-touching foreground pixels (ties broken by total area). |

---

### `src/helpers/labeling.h`
Connected-component labeling and label utilities.

| Function | Description |
|----------|-------------|
| `buildLabelsByConnectedComponents(gray)` | Otsu threshold → `cv::connectedComponents` with 8-connectivity. |
| `buildLabelsFromGrayValues(gray)` | Treat pixel intensity directly as label ID (for images where each gray level is a distinct object). |
| `buildLabelsFromColorValues(bgr)` | Treat each unique BGR color as a label. Black is background (label 0). |
| `buildLabelImage(src)` | Smart dispatcher: if input is already `CV_32SC1` → clone; if grayscale with 2–64 unique values → `buildLabelsFromGrayValues`; if BGR with 2–128 unique colors → `buildLabelsFromColorValues`; otherwise → Otsu + connected components. |
| `labelComponentsByTraversal(binary, neighborhood, useDfs)` | BFS or DFS flood-fill labeling. `useDfs=true` switches from `std::queue` to `std::stack`. |
| `collectPreviousNeighborLabels(labels, x, y, neighborhood, out)` | Collect positive labels from W, N (and NW, NE for N8) neighbors. Helper for two-pass algorithm. |
| `labelComponentsTwoPass(binary, neighborhood)` | Two-pass labeling with union-find. First pass assigns provisional labels with equivalence tracking; second pass compacts to `{1, 2, ...}`. Returns `TwoPassLabelingResult` with both passes. |
| `collectLabels(labels)` | Return sorted vector of all unique positive label values. |
| `maskForLabel(labels, label)` | Binary mask where pixels equal to `label` are 255. Thin wrapper over `cv::compare`. |
| `colorizeLabels(labels)` | Pseudo-color each label using deterministic hash: `b = label*67 % 256`, `g = label*131 % 256`, `r = label*197 % 256`. Background (label ≤ 0) stays black. |

---

### `src/helpers/morphology.h`
Manual 8-neighborhood morphological operations (do not use OpenCV's `cv::dilate`/`cv::erode` — these are the hand-rolled implementations required by the lab).

| Function | Description |
|----------|-------------|
| `dilate8Once(binary)` | Set every 8-neighbor of every foreground pixel to 255. |
| `erode8Once(binary)` | Keep a foreground pixel only if all 8 neighbors are also foreground. |
| `applyMorphIterations(binary, n, useDilation)` | Apply dilation or erosion n times. |
| `opening8Once(binary)` | Erode then dilate (one pass each). |
| `closing8Once(binary)` | Dilate then erode (one pass each). |
| `repeatOpening(binary, n)` | Opening applied n times (demonstrates idempotence). |
| `repeatClosing(binary, n)` | Closing applied n times. |
| `boundaryExtraction8(binary)` | `β(A) = A − (A ⊖ B)` where B is 8-neighborhood element. |
| `morphologicalRegionFill8(mask, seed, maxIter, &usedIter, &converged)` | Iterative fill: `Xk = (Xk−1 ⊕ B) ∩ Aᶜ`. Starts from seed point, expands through background, stops when no change. Returns filled region or empty Mat if seed is invalid. |

---

### `src/helpers/chain_code.h`
Chain code encoding, decoding, analysis, and file I/O.

| Function | Description |
|----------|-------------|
| `chainDirectionCount(neighborhood)` | Returns 4 for N4, 8 for N8. |
| `chainDirectionOffset(neighborhood, direction)` | Direction index → `cv::Point` offset from `kChainDirections4/8`. |
| `deltaToDirection(delta, neighborhood)` | `cv::Point` step → direction index, or -1 if not a valid step. |
| `expandDiagonalToN4(delta, out)` | Decompose a diagonal step into two cardinal N4 steps. |
| `joinCode(code)` | Format a direction sequence as space-separated string. |
| `computeChainDerivative(code, neighborhood)` | Circular first difference: `d[i] = (code[i+1] − code[i]) mod M`. |
| `appendContourStep(from, to, neighborhood, code, points)` | Encode one contour segment into chain-code and reconstructed points. Handles diagonal decomposition for N4. |
| `buildChainCodeTrace(contour, neighborhood)` | Encode an ordered `vector<Point>` contour into a `ChainCodeTrace`. |
| `drawPolyline(image, points, color)` | Draw connected line segments on an image in-place. |
| `logChainCodeTrace(componentIndex, neighborhood, trace)` | Print code, closed code, derivative, closed derivative to stdout. |
| `parseChainCodeFile(text, data)` | Parse the reconstruct.txt format: `start_x start_y\nlength\ncode...`. |
| `reconstructFromChainCode(startPoint, code, neighborhood)` | Follow chain code from start point, returning the polyline. |

---

### `src/helpers/histogram.h`
Histogram computation and rendering.

| Function | Description |
|----------|-------------|
| `computeHistogram(gray, bins=256)` | Count pixels per bin. Bin index: `floor(value * bins / 256)`. |
| `computePDF(hist, total)` | Normalize histogram by total pixel count. |
| `computeCumulativeHistogram(hist)` | Running sum: `cdf[i] = cdf[i−1] + hist[i]`. |
| `computeMeanStdDev(hist, total, &mean, &stddev)` | Compute μ = Σ(i·h[i])/N and σ = sqrt(Σ((i−μ)²·h[i])/N) from histogram. |
| `drawHistogramInt(hist, w=256, h=200)` | Render integer histogram as white-background bar chart (black bars). Returns `CV_8UC1`. |
| `drawHistogramFloat(pdf, w=256, h=200)` | Same as above for float PDF. |
| `drawCumulativeHistogram(cdf, w=256, h=200)` | Same for CDF — bar height reaches full height at right edge. |
| `drawHistogramIntWithOverlay(hist, mean, stddev, w=256, h=220)` | Histogram with μ and σ text overlaid in red. Returns `CV_8UC3`. |

---

### `src/helpers/geometry.h`
Object geometry and projection utilities.

| Function | Description |
|----------|-------------|
| `normalizeAngleDeg90(deg)` | Map angle to [−90°, 90°) by adding/subtracting 180° repeatedly. |
| `computeObjectStats(mask, label)` | Compute all `ObjectStats` from a binary object mask using `cv::moments`, `cv::findContours`, and `cv::arcLength`. |
| `computeRowProjection(mask)` | Count foreground pixels per row → `vector<int>` of length `rows`. |
| `computeColProjection(mask)` | Count foreground pixels per column → `vector<int>` of length `cols`. |
| `drawCombinedXYProjections(canvas, xProj, yProj, srcH)` | Render both projections on a shared canvas: X (column) as vertical bars in orange, Y (row) as horizontal bars in green. Includes labeled axes. |

---

### `src/helpers/rendering.h`
Application-level rendering and I/O.

| Symbol | Description |
|--------|-------------|
| `MAIN_WINDOW_NAME` | `constexpr const char*` = `"Image Processing"`. Used for window creation, title, mouse callback registration. |
| `renderGrid(src, outputs)` | Composite source + all outputs into a labeled grid canvas and call `cv::imshow`. Updates `g_renderContext`. |
| `saveAllOutputs(outputs)` | Write every output image to `assets/export/<epoch>_<label>.bmp`. |
| `loadImage(path, img)` | Load image via `FileUtils::readImage`, reset `g_selectionState`, set `g_needsUpdate = true`. |
| `hasQtBackend()` | Check `cv::getBuildInformation()` for "GUI:" and "QT". |
| `updateMainWindowTitle(name)` | Set window title to `"Image Processing — <name>"`. |
| `mapMainWindowPointToSource(windowPoint, &sourcePoint)` | Use `g_renderContext` to map a canvas pixel to source-image coordinates. Returns false if the point is outside an image panel. |
| `mainWindowMouseCallback(event, x, y, ...)` | Registered with `cv::setMouseCallback`. On left-click: maps to source coords and updates `g_selectionState`. |

---

## Configuration

### Default Image
Edit `main.cpp`, line:
```cpp
if (!loadImage(IMAGE("PI-L4/trasaturi_geom.bmp"), img)) {
```

`IMAGE("subdir/file.bmp")` expands to `"./assets/images/subdir/file.bmp"`.

### Asset Paths
All path macros are in `src/common/paths.h`:

```cpp
#define ImageFolder  "./assets/images"
#define ExportFolder "./assets/export"

#define IMAGE(PATH)  ImageFolder  "/" PATH
#define EXPORT(PATH) ExportFolder "/" PATH
```

The working directory must be the repo root when running (`./build/opengl_template`).

### Chain Code Reconstruct File
`boundary_reconstruct_from_code` reads from `assets/images/PI-L6/reconstruct.txt`. Format:
```
<start_x> <start_y>
<declared_length>
<dir_0> <dir_1> ... <dir_N>
```

### Adding Image Assets
Drop images into `assets/images/` (any subdirectory). Press `O` at runtime to open a file dialog and load any image.

---

## Keyboard & Mouse Controls

| Key | Action |
|-----|--------|
| `←` / `→` | Previous / next effect |
| `1` – `9` | Jump to effect by index |
| `O` | Open file dialog, load new image |
| `Enter` | Save all current outputs to `assets/export/` |
| `Space` | Reactivate controls (use after window resize) |
| `Esc` | Exit |
| Window `×` | Exit |

| Mouse | Action |
|-------|--------|
| Left-click | Map click to source-image coordinates, update `g_selectionState`. Used by `selected_object_features`, `contour_and_region_fill_lab7`, and any click-driven effect. |

### Saved File Format
`assets/export/<unix_timestamp>_<label>.bmp` — one BMP per output panel per save.
