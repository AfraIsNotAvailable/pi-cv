# Lab 8 - Statistical Properties of Intensity Images (Code Walkthrough)

Companion document to the four `lab8_*` effects added to [main.cpp](../main.cpp).
Read this alongside the lab spec in [lab8.md](../lab8.md). Every formula below is reproduced from
the spec and tied to the code that implements it.

## Table of contents

1. [Shared utilities](#shared-utilities)
2. [Exercise 1 - Statistical properties display](#exercise-1---statistical-properties-display)
3. [Exercise 2 - Automatic iterative binarization](#exercise-2---automatic-iterative-binarization)
4. [Exercise 3 - Pointwise transforms](#exercise-3---pointwise-transforms)
5. [Exercise 4 - Histogram equalization](#exercise-4---histogram-equalization)
6. [How to run it](#how-to-run-it)

---

## Shared utilities

Added near the existing histogram helpers in `main.cpp`.

### `computeHistogram(gray, bins)`

Pre-existing helper. Produces `std::vector<int>` of length `bins` (use 256 for this lab). Bucket `i` counts pixels with intensity `floor(value * bins / 256)`.

### `computeCumulativeHistogram(hist)` - new

Returns `C(k) = sum_{i=0..k} h(i)`. It is the running sum of the histogram. `C(255)` always equals `W * H`.

### `computeMeanStdDev(hist, totalPixels, &mean, &stddev)` - new

Computes

```
mu = (1 / (W*H)) * sum_{i=0..255} i * h(i)
sigma = sqrt( (1 / (W*H)) * sum_{i=0..255} (i - mu)^2 * h(i) )
```

directly from the histogram, not by re-scanning the pixels.

### `drawCumulativeHistogram(cdf)` - new

Renders the cumulative histogram as a filled-column plot normalized by `cdf.back()`.

### `drawHistogramIntWithOverlay(hist, mean, stddev)` - new

Same plot as `drawHistogramInt`, but outputs a BGR image with `mu=... sigma=...` printed in red in the top-left corner.

---

## Exercise 1 - Statistical properties display

**Effect:** `lab8_statistics` in `main.cpp`.

**What the UI shows:** the source image plus three chart panels - histogram (with mu / sigma overlay), cumulative histogram, PDF.

**What the terminal prints:** `[Lab8] mean=... stddev=...`.

**Algorithm** (literally the spec):

| Step | Formula | Code |
|---|---|---|
| 1 | Convert source to grayscale | `toGray8U(src)` |
| 2 | `h(i)` = histogram | `computeHistogram(gray, 256)` |
| 3 | `C(k) = sum_{i=0..k} h(i)` | `computeCumulativeHistogram(hist)` |
| 4 | `p(i) = h(i) / (W*H)` | `computePDF(hist, W*H)` |
| 5 | mu, sigma from `h` | `computeMeanStdDev(hist, ...)` |
| 6 | Render the three charts | `drawHistogramIntWithOverlay`, `drawCumulativeHistogram`, `drawHistogramFloat` |

**Reference values.** For `assets/images/cameraman.bmp` (and similar low-key portraits) expect `mu ~= 113.72, sigma ~= 48.38` - matching the PDF example `mu=113.723, sigma=48.3751`.

---

## Exercise 2 - Automatic iterative binarization

**Effect:** `lab8_auto_binarize`.

**Control:** `Epsilon x100` slider, representing epsilon as `slider / 100`. Slider default 10 -> epsilon = 0.10 (matches the PDF example).

**Algorithm.** Classic isodata / iterative thresholding:

```
T_0 = (I_min + I_max) / 2
loop:
    mu_1 = (sum_{i <= T_k} i * h(i)) / N_1 ,  N_1 = sum_{i <= T_k} h(i)
    mu_2 = (sum_{i >  T_k} i * h(i)) / N_2 ,  N_2 = sum_{i > T_k} h(i)
    T_{k+1} = (mu_1 + mu_2) / 2
until |T_{k+1} - T_k| < epsilon
```

Each iteration scans the 256-bin histogram, not the image, so convergence takes microseconds regardless of image size.

**Edge cases handled in code:**
- All pixels same intensity (`I_min == I_max`) -> emit all-zero mask.
- Either group ends up empty -> fall back to that group's endpoint intensity so the mean is still defined.
- Hard cap of 1000 iterations as a safety rail.

**Visualization.** Output panel shows the thresholded binary image annotated with `T = <value>  (iter <n>)` in the top-left.

---

## Exercise 3 - Pointwise transforms

**Effect:** `lab8_transforms`.

**Controls:**

| Slider | Raw range | Real meaning |
|---|---|---|
| `Iout Min` | 0..255 | lower output bound of contrast stretch |
| `Iout Max` | 0..255 | upper output bound of contrast stretch |
| `Gamma x10` | 1..30 | gamma = raw / 10 (so 0.1..3.0, default 1.0 = identity) |
| `Brightness Offset` | 0..255 | effective offset = raw - 128 (so -128..+127) |

All four transforms are implemented as **256-entry LUTs**, rebuilt on every update, then applied pixel-by-pixel.

### Negative

```
I_out = 255 - I_in
```

### Contrast stretch

```
I_out = I_out_MIN + (I_in - I_in_MIN) * (I_out_MAX - I_out_MIN) / (I_in_MAX - I_in_MIN)
```

`I_in_MIN` / `I_in_MAX` come from `cv::minMaxLoc` on the actual image. This is what makes the transform adaptive.

**Effect on histogram:**
- `I_out_MAX - I_out_MIN < I_in_MAX - I_in_MIN` -> histogram shrinks (contrast decreases).
- `I_out_MAX - I_out_MIN > I_in_MAX - I_in_MIN` -> histogram stretches (contrast increases).

### Gamma correction

```
I_out = 255 * (I_in / 255) ^ gamma
```

- gamma < 1 -> encoding / compression; mid-tones brighten.
- gamma > 1 -> decoding / decompression; mid-tones darken.

### Brightness offset

```
I_out = clamp(I_in + offset, 0, 255)
```

The grid shows the four transformed images followed by their histograms plus the original histogram, so the shrink/stretch visual is directly next to the input.

---

## Exercise 4 - Histogram equalization

**Effect:** `lab8_histogram_equalization`.

**Principle.** Use the normalized cumulative histogram as the intensity-remapping curve:

```
FDPC(k) = (1 / (W*H)) * sum_{i=0..k} h(i)
I_out   = round( 255 * FDPC(I_in) )
```

This spreads input intensities so the output histogram is as uniform as the discrete quantization allows. Equivalent to using the CDF itself as the transfer function.

**Implementation tactics:**
- Build `FDPC` in one pass as a running sum; multiply by 255 and round to produce a `uchar` LUT entry.
- Apply the LUT to each pixel (O(W * H), no per-pixel arithmetic).

**Visualization.** Two histograms side by side, each annotated with its own mu / sigma. Usually the equalized sigma >= original sigma because intensities are spread wider.

---

## How to run it

1. First-time build + run:
   ```bash
   ./run.sh dependencies build execute
   ```
2. Subsequent runs:
   ```bash
   ./run.sh build execute
   ```
3. In the window:
   - Use the effect list (left / right arrows, number keys, or the radio buttons in the control panel) to reach `Lab8 Statistics`, `Lab8 Auto Binarize`, `Lab8 Transforms`, `Lab8 Equalize`.
   - Press `O` to load a different image via file dialog.
   - Press `Enter` to dump all currently displayed outputs to `assets/export/`.
   - Press `ESC` or close the window to exit.
