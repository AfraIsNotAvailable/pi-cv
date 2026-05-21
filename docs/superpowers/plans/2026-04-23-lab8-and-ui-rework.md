# Lab 8 Effects, Educational Write-up, and Controls-Panel UI Rework

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Lab 8 (statistical properties of intensity images) as four new Slider effects with heavily-commented, education-oriented code; produce a companion Markdown walkthrough (`docs/lab8_explained.md`) that re-derives the math next to the implementation; then rework the OpenCV Qt control panel so the effect carousel becomes a clear vertical radio-button list with per-lab section separators and the current effect's controls rendered directly underneath.

**Architecture:**
- **Phase 1 (effects + docs)** lives entirely in `main.cpp`: four new helpers near the existing histogram utilities, four new effect functions, four new `SliderEntry` registrations. Inline comments explain the statistical formulas step by step. One new Markdown file `docs/lab8_explained.md` re-derives the four algorithms with cross-references to `main.cpp` line numbers.
- **Phase 2 (UI rework)** is confined to [src/slider/slider.cpp](src/slider/slider.cpp) and [src/slider/slider.h](src/slider/slider.h), plus a single string change in [src/controls/controls_manager.cpp](src/controls/controls_manager.cpp) for the separator bars. The design stays inside OpenCV's Qt HighGUI (no QtWidgets link): effect buttons switch from `QT_PUSH_BUTTON` to a `QT_RADIOBOX` group so the visible widget looks like a selection list with a filled-circle marker on the active item. `QT_NEW_BUTTONBAR` breaks the list into named sections ("Lab 3", "Lab 4", "Lab 7", "Lab 8"), and the existing per-effect controls still render below — automatically, since `ControlsManager::activate()` already rebuilds them on every switch.

**Tech Stack:** C++17, OpenCV 4 (Qt HighGUI build), spdlog via project's `INFO/DEBUG` macros, CMake + Conan build system unchanged.

This plan supersedes the earlier, narrower plan `2026-04-23-lab8-statistical-properties.md`. You can delete that file at the end of the session if you want.

---

## Verification Strategy

The repo has **no test runner** (CLAUDE.md: "There are no tests or lint steps configured."). Verification for each task is a checklist of build + visual inspection steps, spelled out explicitly. Each task ends with a "Run & verify" step listing exactly what to look for and what to reject. The terminal `INFO` log lines are the only numeric oracle we have — they are cross-checked against reference values printed in the lab PDF (e.g. `mu=113.723`, `sigma=48.3751`, `T=165`).

---

## File Structure

**Modified:**
- `main.cpp` — all four helpers, four effects, and four Slider registrations added in place. Educational inline comments keep the math next to the code.
- `EFFECTS.md` — appended Lab 8 section + a short "Controls Panel Layout" note at the end.
- `src/slider/slider.h` — add `groupLabel` field to `SliderEntry` (used by the new section separators).
- `src/slider/slider.cpp` — swap `QT_PUSH_BUTTON` → `QT_RADIOBOX`, add `QT_NEW_BUTTONBAR` separators, track & update the "selected" radio.

**Created:**
- `docs/lab8_explained.md` — standalone educational walkthrough.

**Not touched:**
- `CMakeLists.txt`, `conanfile.py`, `run.sh` — no dependency changes, no new source files.
- `src/controls/controls_manager.*` — stays as-is; already rebuilds the trackbars/checkboxes/radios for the active effect on every switch, which is what the reworked UI needs.

**Placement inside `main.cpp`:**
- Helpers: immediately after `drawHistogramFloat` (ends at [main.cpp:1629](main.cpp#L1629)), before the `// New effects:` divider at [main.cpp:1631](main.cpp#L1631).
- Effect functions: after `hsv_hue_quantization` (~[main.cpp:1810](main.cpp#L1810)), before the Lab 4 geometric block at [main.cpp:1939](main.cpp#L1939). Group under a `// Lab 8 ---` banner.
- Registrations: inside `Slider slider({ ... })` initializer (~[main.cpp:2275](main.cpp#L2275)), appended after `"Labeling Compare"` entry.

---

# PHASE 1 — Lab 8 effects, educational comments, explanation doc

## Task 1: Shared Lab 8 Utilities (with educational inline comments)

**Files:**
- Modify: `main.cpp` — insert between [main.cpp:1629](main.cpp#L1629) and [main.cpp:1631](main.cpp#L1631).

- [ ] **Step 1: Add cumulative histogram + renderer**

Insert immediately after the closing brace of `drawHistogramFloat` at [main.cpp:1629](main.cpp#L1629):

```cpp
/**
 * @brief Computes the cumulative histogram C(k) = Σ_{i=0..k} h(i).
 *
 * This is the running sum of the histogram. C(255) always equals the total
 * number of pixels W*H. The shape of C is the "where most of the pixels live"
 * curve: a steep rise means many pixels in that intensity range, a flat
 * stretch means few. Equalization (Ex. 4) is literally "use C normalized by
 * W*H as the intensity-remapping curve".
 */
static std::vector<int> computeCumulativeHistogram(const std::vector<int>& hist) {
    std::vector<int> cdf(hist.size(), 0);
    if (hist.empty()) return cdf;
    // Seed the recurrence: cdf[0] = h[0].
    cdf[0] = hist[0];
    // Accumulate: cdf[i] = cdf[i-1] + h[i].
    for (size_t i = 1; i < hist.size(); ++i) {
        cdf[i] = cdf[i - 1] + hist[i];
    }
    return cdf;
}

/**
 * @brief Draws a cumulative histogram as a filled-column plot.
 *
 * Normalizes by cdf.back() (the total pixel count) so the curve always
 * reaches full height at the right edge — that's the visual signature of
 * a cumulative histogram.
 */
static cv::Mat drawCumulativeHistogram(const std::vector<int>& cdf,
                                       int width = 256, int height = 200) {
    int bins = static_cast<int>(cdf.size());
    cv::Mat img(height, width, CV_8UC1, cv::Scalar(255));
    if (bins == 0) return img;
    // cdf.back() is monotonically the largest element, so use it as the denom.
    int maxv = cdf.back();
    if (maxv == 0) maxv = 1;
    float binW = static_cast<float>(width) / bins;
    for (int i = 0; i < bins; ++i) {
        float hval = static_cast<float>(cdf[i]) / static_cast<float>(maxv);
        int barH = static_cast<int>(hval * height);
        cv::rectangle(img,
                      cv::Point(static_cast<int>(i * binW), height - barH),
                      cv::Point(static_cast<int>((i + 1) * binW), height),
                      cv::Scalar(0),
                      cv::FILLED);
    }
    return img;
}
```

- [ ] **Step 2: Add mean / stddev computation**

Directly below:

```cpp
/**
 * @brief Computes mean and standard deviation from a 256-bin histogram.
 *
 * Textbook moments, computed from the histogram instead of raw pixels
 * (cheaper when you already have the histogram):
 *
 *   μ     = (1 / (W·H)) · Σ_{i=0..255} i · h(i)
 *   σ     = sqrt( (1 / (W·H)) · Σ_{i=0..255} (i − μ)² · h(i) )
 *
 * `hist` must be the raw counts (not the PDF). `totalPixels` must equal
 * Σ h(i); pass W·H for a standard image.
 */
static void computeMeanStdDev(const std::vector<int>& hist,
                              int totalPixels,
                              double& outMean,
                              double& outStdDev) {
    outMean = 0.0;
    outStdDev = 0.0;
    if (totalPixels <= 0 || hist.size() < 256) return;

    // First moment — sum of (intensity · count), then divide by N.
    double mean = 0.0;
    for (int i = 0; i < 256; ++i) {
        mean += static_cast<double>(i) * hist[i];
    }
    mean /= static_cast<double>(totalPixels);

    // Second central moment — variance — then sqrt for stddev.
    double var = 0.0;
    for (int i = 0; i < 256; ++i) {
        double d = static_cast<double>(i) - mean;
        var += d * d * hist[i];
    }
    var /= static_cast<double>(totalPixels);

    outMean = mean;
    outStdDev = std::sqrt(var);
}
```

- [ ] **Step 3: Add histogram-with-stats renderer**

Directly below:

```cpp
/**
 * @brief Renders a histogram with μ / σ printed in the top-left corner.
 *        Output is BGR (3 channels) because text is drawn in red.
 */
static cv::Mat drawHistogramIntWithOverlay(const std::vector<int>& hist,
                                           double mean,
                                           double stddev,
                                           int width = 256,
                                           int height = 220) {
    // Reuse the monochrome renderer, then convert to BGR for coloured text.
    cv::Mat gray = drawHistogramInt(hist, width, height);
    cv::Mat bgr;
    cv::cvtColor(gray, bgr, cv::COLOR_GRAY2BGR);

    char buf[96];
    std::snprintf(buf, sizeof(buf), "mu=%.2f  sigma=%.2f", mean, stddev);
    cv::putText(bgr, buf, cv::Point(6, 16),
                cv::FONT_HERSHEY_SIMPLEX, 0.45,
                cv::Scalar(0, 0, 200), 1, cv::LINE_AA);
    return bgr;
}
```

- [ ] **Step 4: Build**

Run: `./run.sh build`
Expected: success. No warnings in any of the four new helpers.

- [ ] **Step 5: Commit**

```bash
git add main.cpp
git commit -m "feat(lab8): add cumulative histogram, mean/stddev, stats-overlay helpers"
```

---

## Task 2: Effect — Lab 8 Exercise 1 (Statistical Properties)

**Files:**
- Modify: `main.cpp` — new function after `hsv_hue_quantization`; new `SliderEntry`.

- [ ] **Step 1: Add the effect function**

After the closing brace of `hsv_hue_quantization` (~[main.cpp:1810](main.cpp#L1810)), insert:

```cpp
// ---------------------------------------------------------------------------
// Lab 8 – Statistical properties of intensity images
// ---------------------------------------------------------------------------

/**
 * @brief Lab 8, Exercise 1 — statistical properties display.
 *
 * Pipeline:
 *   1. Convert src to single-channel grayscale (toGray8U).
 *   2. Build the 256-bin histogram h(i).
 *   3. Derive the cumulative histogram C(k) = Σ_{i=0..k} h(i).
 *   4. Derive the PDF  p(i) = h(i) / (W·H).
 *   5. Compute μ and σ from h (see computeMeanStdDev).
 *   6. Emit three charts: histogram (with μ/σ overlay), cumulative, PDF.
 *
 * The terminal log line gives a precise numeric reading; the overlay on
 * the histogram image gives an at-a-glance one.
 */
void lab8_statistics(const cv::Mat& src, OutputImages& outputs, ControlsManager& /*controls*/) {
    // Step 1 — ensure grayscale so our 256-bin math is meaningful.
    cv::Mat gray = toGray8U(src);

    // Steps 2–4 — histogram, cumulative, PDF.
    auto hist = computeHistogram(gray, 256);
    auto cdf  = computeCumulativeHistogram(hist);
    auto pdf  = computePDF(hist, gray.rows * gray.cols);

    // Step 5 — first/second moments.
    double mean = 0.0;
    double stddev = 0.0;
    computeMeanStdDev(hist, gray.rows * gray.cols, mean, stddev);

    // Trace the computed stats so they are reproducible from the log.
    INFO("[Lab8] mean={:.4f} stddev={:.4f}", mean, stddev);

    // Step 6 — push three named outputs; renderGrid lays them out.
    outputs.push_back({"Histogram", drawHistogramIntWithOverlay(hist, mean, stddev)});
    outputs.push_back({"Cumulative", drawCumulativeHistogram(cdf)});
    outputs.push_back({"PDF", drawHistogramFloat(pdf)});
}
```

- [ ] **Step 2: Register in the Slider**

Inside `Slider slider({ ... })` in `main()`, after the `"Labeling Compare"` entry (~[main.cpp:2378](main.cpp#L2378)), append:

```cpp
                        { "Lab8 Statistics",
                            EffectFn3(lab8_statistics),
                                                {} /* no trackbars */,
                                                "Histogram, cumulative histogram, PDF, mean, stddev" },
```

- [ ] **Step 3: Build**

Run: `./run.sh build`
Expected: success.

- [ ] **Step 4: Run & verify**

Run: `./run.sh execute`
Expected:
- `"Lab8 Statistics"` appears in the carousel.
- Grid shows source + histogram (with `mu=… sigma=…` overlay) + cumulative + PDF.
- Terminal line `[Lab8] mean=113.72 stddev=48.38` (or close) appears on `assets/images/cameraman.bmp`. The lab PDF cites `μ=113.723 σ=48.3751` as the reference for that kind of image.
- Pressing `O` and loading a different image refreshes every number.

- [ ] **Step 5: Commit**

```bash
git add main.cpp
git commit -m "feat(lab8): add ex1 statistics effect (hist + CDF + PDF + mu/sigma)"
```

---

## Task 3: Effect — Lab 8 Exercise 2 (Automatic Iterative Binarization)

**Files:**
- Modify: `main.cpp` — new function + new `SliderEntry`.

- [ ] **Step 1: Add the effect function**

Append below `lab8_statistics`:

```cpp
/**
 * @brief Lab 8, Exercise 2 — automatic iterative binarization.
 *
 * Algorithm (converges in ~3–8 iterations on typical images):
 *   T_0 = (I_min + I_max) / 2
 *   loop:
 *     μ_1 = mean of pixels with I ≤ T_k
 *     μ_2 = mean of pixels with I >  T_k
 *     T_{k+1} = (μ_1 + μ_2) / 2
 *   stop when |T_{k+1} − T_k| < ε   (ε user-configurable)
 *
 * The means are computed directly from the histogram (no second pass over
 * pixels), so each iteration is O(256).
 *
 * Slider "Epsilon x100" supplies ε as (raw value / 100). Raw 10 → ε = 0.10
 * (matches the example in the lab PDF); clamped to ≥ 1 to avoid ε = 0.
 */
void lab8_auto_binarize(const cv::Mat& src, OutputImages& outputs, ControlsManager& controls) {
    // Map slider int to the real-valued ε the algorithm actually uses.
    int epsInt = controls.getEffective("Epsilon x100");
    if (epsInt < 1) epsInt = 1;
    double epsilon = static_cast<double>(epsInt) / 100.0;

    cv::Mat gray = toGray8U(src);
    auto hist = computeHistogram(gray, 256);

    // Find I_min and I_max from the histogram (cheaper than minMaxLoc
    // for already-built hist, and it's what the PDF's pseudocode uses).
    int iMin = 0;
    int iMax = 255;
    while (iMin < 256 && hist[iMin] == 0) ++iMin;
    while (iMax > 0   && hist[iMax] == 0) --iMax;

    // Degenerate image — a single intensity. Emit an all-zero mask and exit.
    if (iMin >= iMax) {
        cv::Mat bin(gray.size(), CV_8UC1, cv::Scalar(0));
        outputs.push_back({"Binary", bin});
        return;
    }

    // T_0.
    double T = 0.5 * (iMin + iMax);
    // prevT seeded so the first |T-prevT| is always ≥ ε, forcing at least
    // one iteration to run even for pathological inputs.
    double prevT = T + 10.0 * epsilon + 1.0;
    int iterations = 0;
    const int maxIterations = 1000;  // safety cap — never hit on normal images.

    while (std::abs(T - prevT) >= epsilon && iterations < maxIterations) {
        prevT = T;
        long long n1 = 0, n2 = 0;  // pixel counts in each group
        double s1 = 0.0, s2 = 0.0; // weighted sums (Σ i·h(i)) in each group
        int Ti = static_cast<int>(std::floor(T));
        for (int i = iMin; i <= iMax; ++i) {
            if (i <= Ti) {
                n1 += hist[i];
                s1 += static_cast<double>(i) * hist[i];
            } else {
                n2 += hist[i];
                s2 += static_cast<double>(i) * hist[i];
            }
        }
        // If one group is empty, fall back to its endpoint to keep T well-defined.
        double mu1 = (n1 > 0) ? (s1 / static_cast<double>(n1)) : static_cast<double>(iMin);
        double mu2 = (n2 > 0) ? (s2 / static_cast<double>(n2)) : static_cast<double>(iMax);
        T = 0.5 * (mu1 + mu2);
        ++iterations;
    }

    int thresh = std::clamp(static_cast<int>(std::round(T)), 0, 255);
    INFO("[Lab8] auto-binarize: Imin={} Imax={} T={:.4f} iter={} eps={:.4f}",
         iMin, iMax, T, iterations, epsilon);

    // Apply the threshold with a straight pixel-wise comparison.
    cv::Mat bin(gray.size(), CV_8UC1);
    for (int r = 0; r < gray.rows; ++r) {
        const uchar* s = gray.ptr<uchar>(r);
        uchar* d = bin.ptr<uchar>(r);
        for (int c = 0; c < gray.cols; ++c) {
            d[c] = (s[c] > thresh) ? 255 : 0;
        }
    }

    // Annotate the binary output with the chosen threshold for convenience.
    cv::Mat binAnnotated;
    cv::cvtColor(bin, binAnnotated, cv::COLOR_GRAY2BGR);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "T = %d  (iter %d)", thresh, iterations);
    cv::putText(binAnnotated, buf, cv::Point(8, 20),
                cv::FONT_HERSHEY_SIMPLEX, 0.55,
                cv::Scalar(0, 0, 220), 1, cv::LINE_AA);

    outputs.push_back({"Binary", binAnnotated});
}
```

- [ ] **Step 2: Register**

Append after the `"Lab8 Statistics"` entry:

```cpp
                        { "Lab8 Auto Binarize",
                            EffectFn3(lab8_auto_binarize),
                                                { {"Epsilon x100", 10, 500, 10, true, 1} },
                                                "Iterative automatic thresholding (T stops when |dT|<eps)" },
```

Slider semantics: raw 1..500 → ε 0.01..5.0; default 10 → ε = 0.10; neutralValue 10 keeps that behaviour when the ON/OFF toggle is off.

- [ ] **Step 3: Build**

Run: `./run.sh build`
Expected: success.

- [ ] **Step 4: Run & verify**

Run: `./run.sh execute`
Expected:
- `"Lab8 Auto Binarize"` available.
- `cameraman.bmp` → binary cleanly separates foreground/background; `T = …` text overlay reflects a reasonable threshold.
- On images whose PDF example cites `T = 165`, terminal log shows `T` within ±1 of 165.
- Raising `"Epsilon x100"` reduces iteration count; ε = 0.01 (raw 1) still converges well under 1000 iterations.
- Toggling the slider OFF reverts ε to 0.10 and still produces a valid result.

- [ ] **Step 5: Commit**

```bash
git add main.cpp
git commit -m "feat(lab8): add ex2 iterative auto-binarization effect"
```

---

## Task 4: Effect — Lab 8 Exercise 3 (Negative, Contrast Stretch, Gamma, Brightness)

**Files:**
- Modify: `main.cpp` — new function + new `SliderEntry`.

Controls:
- `"Iout Min"` int `[0, 255]`, default 30, neutral 0.
- `"Iout Max"` int `[0, 255]`, default 220, neutral 255.
- `"Gamma x10"` int `[1, 30]`, default 10 (→ γ = 1.0, identity), neutral 10.
- `"Brightness Offset"` int `[0, 255]`, default 128, neutral 128; effective offset = raw − 128, so the slider spans −128..+127.

- [ ] **Step 1: Add the effect function**

Append below `lab8_auto_binarize`:

```cpp
/**
 * @brief Lab 8, Exercise 3 — pointwise intensity transforms.
 *
 * Four transforms, all applied as 256-entry lookup tables (LUTs):
 *
 *   Negative:            I_out = 255 − I_in
 *   Contrast stretch:    I_out = outMin + (I_in − inMin) · (outMax − outMin) / (inMax − inMin)
 *                        (inMin, inMax = actual image min/max; outMin, outMax = slider values)
 *   Gamma correction:    I_out = 255 · (I_in / 255) ^ γ
 *                        γ < 1 → brighten mid-tones (encoding);
 *                        γ > 1 → darken mid-tones (decoding).
 *   Brightness offset:   I_out = clamp(I_in + offset, 0, 255)
 *                        offset = slider − 128 → range −128..+127.
 *
 * For each transform the corresponding histogram is also drawn, which is
 * where the contrast-stretch "stretch" / "shrink" effect becomes obvious.
 */
void lab8_transforms(const cv::Mat& src, OutputImages& outputs, ControlsManager& controls) {
    // Read and sanitize the controls.
    int outMin = std::clamp(controls.getEffective("Iout Min"), 0, 255);
    int outMax = std::clamp(controls.getEffective("Iout Max"), 0, 255);
    if (outMax < outMin) std::swap(outMin, outMax);

    int gammaInt = controls.getEffective("Gamma x10");
    if (gammaInt < 1) gammaInt = 1;
    double gamma = static_cast<double>(gammaInt) / 10.0;

    int brightnessRaw = controls.getEffective("Brightness Offset");
    int offset = brightnessRaw - 128;  // shift 0..255 slider to -128..+127.

    cv::Mat gray = toGray8U(src);
    auto histOriginal = computeHistogram(gray, 256);

    // Input dynamic range — needed by contrast stretch.
    double inMinD = 0.0, inMaxD = 0.0;
    cv::minMaxLoc(gray, &inMinD, &inMaxD);
    int inMin = static_cast<int>(inMinD);
    int inMax = static_cast<int>(inMaxD);
    double inSpan = std::max(1.0, static_cast<double>(inMax - inMin));  // avoid /0

    cv::Mat negative(gray.size(), CV_8UC1);
    cv::Mat contrast(gray.size(), CV_8UC1);
    cv::Mat gammaImg(gray.size(), CV_8UC1);
    cv::Mat brightness(gray.size(), CV_8UC1);

    // Build the four LUTs once; applying them to the image is O(W·H).
    uchar lutNeg[256];
    uchar lutCon[256];
    uchar lutGam[256];
    uchar lutBri[256];
    for (int v = 0; v < 256; ++v) {
        // Negative — simple reflection around 127.5.
        lutNeg[v] = static_cast<uchar>(255 - v);

        // Contrast stretch — linearly map [inMin, inMax] onto [outMin, outMax].
        double stretched = outMin + (v - inMin) * (outMax - outMin) / inSpan;
        lutCon[v] = static_cast<uchar>(std::clamp(stretched, 0.0, 255.0));

        // Gamma — working in [0, 1] floats, then rescaling to [0, 255].
        double g = 255.0 * std::pow(static_cast<double>(v) / 255.0, gamma);
        lutGam[v] = static_cast<uchar>(std::clamp(g, 0.0, 255.0));

        // Brightness — straight additive with clamp to keep the result in range.
        int b = v + offset;
        lutBri[v] = static_cast<uchar>(std::clamp(b, 0, 255));
    }

    auto applyLut = [&](const uchar lut[256], cv::Mat& dst) {
        for (int r = 0; r < gray.rows; ++r) {
            const uchar* s = gray.ptr<uchar>(r);
            uchar* d = dst.ptr<uchar>(r);
            for (int c = 0; c < gray.cols; ++c) d[c] = lut[s[c]];
        }
    };
    applyLut(lutNeg, negative);
    applyLut(lutCon, contrast);
    applyLut(lutGam, gammaImg);
    applyLut(lutBri, brightness);

    auto histNeg = computeHistogram(negative, 256);
    auto histCon = computeHistogram(contrast, 256);
    auto histGam = computeHistogram(gammaImg, 256);
    auto histBri = computeHistogram(brightness, 256);

    // Image outputs first, histogram outputs after — so the grid pairs them visually.
    outputs.push_back({"Negative", negative});
    outputs.push_back({"Contrast", contrast});
    outputs.push_back({"Gamma", gammaImg});
    outputs.push_back({"Brightness", brightness});

    outputs.push_back({"Hist Original", drawHistogramInt(histOriginal)});
    outputs.push_back({"Hist Negative", drawHistogramInt(histNeg)});
    outputs.push_back({"Hist Contrast", drawHistogramInt(histCon)});
    outputs.push_back({"Hist Gamma", drawHistogramInt(histGam)});
    outputs.push_back({"Hist Brightness", drawHistogramInt(histBri)});

    INFO("[Lab8] transforms: outMin={} outMax={} gamma={:.2f} offset={}",
         outMin, outMax, gamma, offset);
}
```

- [ ] **Step 2: Register**

Append after the `"Lab8 Auto Binarize"` entry:

```cpp
                        { "Lab8 Transforms",
                            EffectFn3(lab8_transforms),
                                                {
                                                        {"Iout Min", 30, 255, 0, true, 0},
                                                        {"Iout Max", 220, 255, 255, true, 0},
                                                        {"Gamma x10", 10, 30, 10, true, 1},
                                                        {"Brightness Offset", 128, 255, 128, true, 0}
                                                },
                                                "Negative, contrast stretch, gamma, brightness + histograms" },
```

- [ ] **Step 3: Build**

Run: `./run.sh build`
Expected: success.

- [ ] **Step 4: Run & verify**

Run: `./run.sh execute`
Expected:
- Grid shows four transformed images + five histograms.
- Pulling `"Iout Min"` up / `"Iout Max"` down narrows the "Hist Contrast" bars (shrink).
- Pushing `"Iout Min"` → 0, `"Iout Max"` → 255 widens "Hist Contrast" beyond the original (stretch).
- `"Gamma x10"` < 10 brightens mid-tones; > 10 darkens them.
- `"Brightness Offset"` = 128 is identity; 255 saturates white; 0 pushes dark.
- Each slider toggled OFF reverts to its identity behaviour.

- [ ] **Step 5: Commit**

```bash
git add main.cpp
git commit -m "feat(lab8): add ex3 transforms (negative, contrast, gamma, brightness)"
```

---

## Task 5: Effect — Lab 8 Exercise 4 (Histogram Equalization)

**Files:**
- Modify: `main.cpp` — new function + new `SliderEntry`.

- [ ] **Step 1: Add the effect function**

Append below `lab8_transforms`:

```cpp
/**
 * @brief Lab 8, Exercise 4 — histogram equalization.
 *
 * Principle: use the normalized cumulative histogram as the intensity
 * remapping curve.  Let
 *     FDPC(k) = (1 / (W·H)) · Σ_{i=0..k} h(i)
 * which is a monotonic function from [0, 255] to [0, 1]. Then
 *     I_out = round( 255 · FDPC(I_in) )
 * spreads the input intensities so that the output histogram is as
 * uniform as possible (subject to the discrete quantization).
 *
 * Visual signature: the equalized histogram is flatter and wider; σ
 * usually increases.
 */
void lab8_histogram_equalization(const cv::Mat& src,
                                 OutputImages& outputs,
                                 ControlsManager& /*controls*/) {
    cv::Mat gray = toGray8U(src);
    int total = gray.rows * gray.cols;
    if (total <= 0) {
        outputs.push_back({"Equalized", gray});
        return;
    }

    auto hist = computeHistogram(gray, 256);

    // Build the remapping LUT in one pass.
    // `running` holds FDPC(i); multiplying by 255 and rounding gives I_out(i).
    double invTotal = 1.0 / static_cast<double>(total);
    uchar lut[256];
    double running = 0.0;
    for (int i = 0; i < 256; ++i) {
        running += static_cast<double>(hist[i]) * invTotal;
        int mapped = static_cast<int>(std::round(255.0 * running));
        lut[i] = static_cast<uchar>(std::clamp(mapped, 0, 255));
    }

    // Apply the LUT to every pixel.
    cv::Mat equalized(gray.size(), CV_8UC1);
    for (int r = 0; r < gray.rows; ++r) {
        const uchar* s = gray.ptr<uchar>(r);
        uchar* d = equalized.ptr<uchar>(r);
        for (int c = 0; c < gray.cols; ++c) d[c] = lut[s[c]];
    }

    auto histEq = computeHistogram(equalized, 256);

    // μ / σ before and after — the numeric way to see "σ_eq ≥ σ_orig".
    double muOrig = 0.0, sigOrig = 0.0;
    double muEq   = 0.0, sigEq   = 0.0;
    computeMeanStdDev(hist,   total, muOrig, sigOrig);
    computeMeanStdDev(histEq, total, muEq,   sigEq);

    INFO("[Lab8] equalize: orig mu={:.2f} sigma={:.2f}  eq mu={:.2f} sigma={:.2f}",
         muOrig, sigOrig, muEq, sigEq);

    outputs.push_back({"Equalized", equalized});
    outputs.push_back({"Hist Original", drawHistogramIntWithOverlay(hist,   muOrig, sigOrig)});
    outputs.push_back({"Hist Equalized", drawHistogramIntWithOverlay(histEq, muEq,   sigEq)});
}
```

- [ ] **Step 2: Register**

Append after the `"Lab8 Transforms"` entry:

```cpp
                        { "Lab8 Equalize",
                            EffectFn3(lab8_histogram_equalization),
                                                {} /* no trackbars */,
                                                "Histogram equalization (original + equalized histograms)" },
```

- [ ] **Step 3: Build**

Run: `./run.sh build`
Expected: success.

- [ ] **Step 4: Run & verify**

Run: `./run.sh execute`
Expected:
- `"Lab8 Equalize"` visible.
- On a low-contrast image (e.g. `bonemarr.bmp`), equalized output shows visibly more tonal separation.
- `"Hist Equalized"` chart is flatter / wider than `"Hist Original"`.
- Terminal logs both `mu`/`sigma` pairs; equalized σ ≥ original σ for typical images.
- `Enter` saves all three outputs to `assets/export/`.

- [ ] **Step 5: Commit**

```bash
git add main.cpp
git commit -m "feat(lab8): add ex4 histogram equalization effect"
```

---

## Task 6: Documentation — `docs/lab8_explained.md` (educational walkthrough)

**Files:**
- Create: `docs/lab8_explained.md`.

This is the companion write-up to the code. It targets a student who has read the lab PDF once and wants the algorithms re-explained next to the actual implementation in `main.cpp`.

- [ ] **Step 1: Create the file with the following content**

```markdown
# Lab 8 — Statistical Properties of Intensity Images (Code Walkthrough)

Companion document to the four `lab8_*` effects added to [main.cpp](../main.cpp).
Read this alongside the lab spec in [lab8.md](../lab8.md). Every formula below is reproduced from
the spec and tied to the code that implements it.

## Table of contents

1. [Shared utilities](#shared-utilities)
2. [Exercise 1 — Statistical properties display](#exercise-1--statistical-properties-display)
3. [Exercise 2 — Automatic iterative binarization](#exercise-2--automatic-iterative-binarization)
4. [Exercise 3 — Pointwise transforms](#exercise-3--pointwise-transforms)
5. [Exercise 4 — Histogram equalization](#exercise-4--histogram-equalization)
6. [How to run it](#how-to-run-it)

---

## Shared utilities

Added near the existing histogram helpers in `main.cpp`.

### `computeHistogram(gray, bins)`

Pre-existing helper. Produces `std::vector<int>` of length `bins` (use 256 for this lab). Bucket `i` counts pixels with intensity `floor(value · bins / 256)`.

### `computeCumulativeHistogram(hist)` — new

Returns `C(k) = Σ_{i=0..k} h(i)`. It is the running sum of the histogram. `C(255)` always equals `W · H`.

### `computeMeanStdDev(hist, totalPixels, &mean, &stddev)` — new

Computes

```
μ = (1 / (W·H)) · Σ_{i=0..255} i · h(i)
σ = sqrt( (1 / (W·H)) · Σ_{i=0..255} (i − μ)² · h(i) )
```

directly from the histogram, not by re-scanning the pixels.

### `drawCumulativeHistogram(cdf)` — new

Renders the cumulative histogram as a filled-column plot normalized by `cdf.back()`.

### `drawHistogramIntWithOverlay(hist, mean, stddev)` — new

Same plot as `drawHistogramInt`, but outputs a BGR image with `mu=…  sigma=…` printed in red in the top-left corner.

---

## Exercise 1 — Statistical properties display

**Effect:** `lab8_statistics` in `main.cpp`.

**What the UI shows:** the source image plus three chart panels — histogram (with μ / σ overlay), cumulative histogram, PDF.

**What the terminal prints:** `[Lab8] mean=… stddev=…`.

**Algorithm** (literally the spec):

| Step | Formula | Code |
|---|---|---|
| 1 | Convert source to grayscale | `toGray8U(src)` |
| 2 | `h(i)` = histogram | `computeHistogram(gray, 256)` |
| 3 | `C(k) = Σ_{i=0..k} h(i)` | `computeCumulativeHistogram(hist)` |
| 4 | `p(i) = h(i) / (W·H)` | `computePDF(hist, W·H)` |
| 5 | μ, σ from `h` | `computeMeanStdDev(hist, …)` |
| 6 | Render the three charts | `drawHistogramIntWithOverlay`, `drawCumulativeHistogram`, `drawHistogramFloat` |

**Reference values.** For `assets/images/cameraman.bmp` (and similar low-key portraits) expect `μ ≈ 113.72, σ ≈ 48.38` — matching the PDF example `μ=113.723, σ=48.3751`.

---

## Exercise 2 — Automatic iterative binarization

**Effect:** `lab8_auto_binarize`.

**Control:** `"Epsilon x100"` slider, representing ε as `slider / 100`. Slider default 10 → ε = 0.10 (matches the PDF example).

**Algorithm.** Classic isodata / iterative thresholding:

```
T_0 = (I_min + I_max) / 2
loop:
    μ_1 = (Σ_{i ≤ T_k} i · h(i)) / N_1 ,  N_1 = Σ_{i ≤ T_k} h(i)
    μ_2 = (Σ_{i >  T_k} i · h(i)) / N_2 ,  N_2 = Σ_{i > T_k} h(i)
    T_{k+1} = (μ_1 + μ_2) / 2
until |T_{k+1} − T_k| < ε
```

Each iteration scans the 256-bin histogram, not the image, so convergence takes microseconds regardless of image size.

**Edge cases handled in code:**
- All pixels same intensity (`I_min == I_max`) → emit all-zero mask.
- Either group ends up empty → fall back to that group's endpoint intensity so the mean is still defined.
- Hard cap of 1000 iterations as a safety rail.

**Visualization.** Output panel shows the thresholded binary image annotated with `T = <value>  (iter <n>)` in the top-left.

---

## Exercise 3 — Pointwise transforms

**Effect:** `lab8_transforms`.

**Controls:**

| Slider | Raw range | Real meaning |
|---|---|---|
| `Iout Min` | 0..255 | lower output bound of contrast stretch |
| `Iout Max` | 0..255 | upper output bound of contrast stretch |
| `Gamma x10` | 1..30 | γ = raw / 10 (so 0.1..3.0, default 1.0 = identity) |
| `Brightness Offset` | 0..255 | effective offset = raw − 128 (so −128..+127) |

All four transforms are implemented as **256-entry LUTs**, rebuilt on every update, then applied pixel-by-pixel.

### Negative
```
I_out = 255 − I_in
```

### Contrast stretch
```
I_out = I_out_MIN + (I_in − I_in_MIN) · (I_out_MAX − I_out_MIN) / (I_in_MAX − I_in_MIN)
```
`I_in_MIN` / `I_in_MAX` come from `cv::minMaxLoc` on the actual image. This is what makes the transform adaptive.

**Effect on histogram:**
- `I_out_MAX − I_out_MIN < I_in_MAX − I_in_MIN` → histogram **shrinks** (contrast decreases).
- `I_out_MAX − I_out_MIN > I_in_MAX − I_in_MIN` → histogram **stretches** (contrast increases).

### Gamma correction
```
I_out = 255 · (I_in / 255) ^ γ
```
- γ < 1 → "encoding" / compression; mid-tones brighten.
- γ > 1 → "decoding" / decompression; mid-tones darken.

### Brightness offset
```
I_out = clamp(I_in + offset, 0, 255)
```

The grid shows the four transformed images followed by their histograms plus the original histogram, so the "shrink"/"stretch" visual is directly next to the input.

---

## Exercise 4 — Histogram equalization

**Effect:** `lab8_histogram_equalization`.

**Principle.** Use the normalized cumulative histogram as the intensity-remapping curve:

```
FDPC(k) = (1 / (W·H)) · Σ_{i=0..k} h(i)
I_out   = round( 255 · FDPC(I_in) )
```

This spreads input intensities so the output histogram is as uniform as the discrete quantization allows. Equivalent to "use the CDF itself as the transfer function."

**Implementation tactics:**
- Build `FDPC` in one pass as a running sum; multiply by 255 and round to produce a `uchar` LUT entry.
- Apply the LUT to each pixel (O(W · H), no per-pixel arithmetic).

**Visualization.** Two histograms side by side, each annotated with its own μ / σ. Usually the equalized σ ≥ original σ because intensities are spread wider.

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
```

- [ ] **Step 2: Commit**

```bash
git add docs/lab8_explained.md
git commit -m "docs(lab8): add educational walkthrough for the lab 8 effects"
```

---

## Task 7: EFFECTS.md — append Lab 8 section

**Files:**
- Modify: `EFFECTS.md`.

- [ ] **Step 1: Append to the end of `EFFECTS.md`**

```markdown

## L8 / Statistical Properties

Effects added for Lab 8 (statistical properties of intensity images). All operate on grayscale input; full walk-through in [docs/lab8_explained.md](docs/lab8_explained.md).

* **Lab8 Statistics** — histogram (with μ / σ overlaid), cumulative histogram, PDF. Terminal logs `μ` and `σ`.
* **Lab8 Auto Binarize** — iterative automatic thresholding. `Epsilon x100` controls the termination error `ε = slider / 100`.
* **Lab8 Transforms** — negative, contrast stretch (`Iout Min`, `Iout Max`), gamma correction (`Gamma x10 / 10`), brightness offset (`Brightness Offset − 128`), with histograms for all four plus the original.
* **Lab8 Equalize** — histogram equalization (`I_out = 255 · FDPC(I_in)`) with μ / σ on both histograms.

Helpers introduced in `main.cpp`: `computeCumulativeHistogram`, `computeMeanStdDev`, `drawCumulativeHistogram`, `drawHistogramIntWithOverlay`.
```

- [ ] **Step 2: Commit**

```bash
git add EFFECTS.md
git commit -m "docs(lab8): document new statistical-properties effects in EFFECTS.md"
```

---

# PHASE 2 — Controls-panel UI rework

The existing control panel stacks a `QT_PUSH_BUTTON` per effect on the left and the current effect's trackbars / checkboxes / radios on the right. The rework converts the effect buttons into a single `QT_RADIOBOX` group — visually a vertical list with a filled circle next to the active effect — and inserts lab-named separators so the list is easier to scan. The controls for the active effect still appear below, unchanged.

OpenCV's Qt HighGUI gives us only `QT_PUSH_BUTTON`, `QT_CHECKBOX`, `QT_RADIOBOX`, and the `QT_NEW_BUTTONBAR` flag. No QListWidget, no QGroupBox. Within those limits, radio buttons + buttonbar separators give the cleanest "list with selection indicator" experience without linking QtWidgets directly. (A full QtWidgets rewrite would be a much larger change and is deferred.)

## Task 8: Add a `groupLabel` field to `SliderEntry`

**Files:**
- Modify: [src/slider/slider.h](src/slider/slider.h)

- [ ] **Step 1: Extend the struct**

In `src/slider/slider.h`, modify the `SliderEntry` struct and both constructors. Replace:

```cpp
struct SliderEntry {
    std::string name;
    EffectFn3 process;
    std::vector<TrackbarSpec> trackbars;
    std::vector<RadioGroupSpec> radioGroups;
    std::string description;

    // Construct with full 3-arg effect function (src, outputs, controls)
    SliderEntry(std::string name, EffectFn3 fn,
                std::vector<TrackbarSpec> trackbars, std::string description,
                std::vector<RadioGroupSpec> radioGroups = {})
        : name(std::move(name)),
          process(std::move(fn)),
          trackbars(std::move(trackbars)),
          radioGroups(std::move(radioGroups)),
          description(std::move(description)) {}

    // Construct with 2-arg effect function (src, outputs) — controls ignored
    SliderEntry(std::string name, EffectFn2 fn,
                std::vector<TrackbarSpec> trackbars, std::string description,
                std::vector<RadioGroupSpec> radioGroups = {})
        : name(std::move(name)),
          process([f = std::move(fn)](const cv::Mat& src, OutputImages& out, ControlsManager&) {
              f(src, out);
          }),
          trackbars(std::move(trackbars)),
          radioGroups(std::move(radioGroups)),
          description(std::move(description)) {}
};
```

with:

```cpp
struct SliderEntry {
    std::string name;
    EffectFn3 process;
    std::vector<TrackbarSpec> trackbars;
    std::vector<RadioGroupSpec> radioGroups;
    std::string description;
    std::string groupLabel;  // Optional section label shown above this entry in the effect list.

    // Construct with full 3-arg effect function (src, outputs, controls)
    SliderEntry(std::string name, EffectFn3 fn,
                std::vector<TrackbarSpec> trackbars, std::string description,
                std::vector<RadioGroupSpec> radioGroups = {},
                std::string groupLabel = {})
        : name(std::move(name)),
          process(std::move(fn)),
          trackbars(std::move(trackbars)),
          radioGroups(std::move(radioGroups)),
          description(std::move(description)),
          groupLabel(std::move(groupLabel)) {}

    // Construct with 2-arg effect function (src, outputs) — controls ignored
    SliderEntry(std::string name, EffectFn2 fn,
                std::vector<TrackbarSpec> trackbars, std::string description,
                std::vector<RadioGroupSpec> radioGroups = {},
                std::string groupLabel = {})
        : name(std::move(name)),
          process([f = std::move(fn)](const cv::Mat& src, OutputImages& out, ControlsManager&) {
              f(src, out);
          }),
          trackbars(std::move(trackbars)),
          radioGroups(std::move(radioGroups)),
          description(std::move(description)),
          groupLabel(std::move(groupLabel)) {}
};
```

`groupLabel` is optional — only the first entry in each logical group needs it set. It is a pure visual hint; no code reads it outside `slider.cpp` (Task 10).

- [ ] **Step 2: Build**

Run: `./run.sh build`
Expected: success. The existing `{ "name", fn, trackbars, description }` and `{ "name", fn, trackbars, description, radioGroups }` initializer-list entries all still compile because `groupLabel` is defaulted.

- [ ] **Step 3: Commit**

```bash
git add src/slider/slider.h
git commit -m "feat(ui): add optional groupLabel field to SliderEntry"
```

---

## Task 9: Tag existing effects with their lab groups in the Slider initializer

**Files:**
- Modify: `main.cpp` — inside the `Slider slider({ ... })` call.

Only the **first entry in each logical lab group** needs a `groupLabel`; leave the rest empty. This keeps the list diff small.

- [ ] **Step 1: Tag "Bi-Level" as the start of the Lab 3 section**

Change (~[main.cpp:2277](main.cpp#L2277)):

```cpp
            { "Bi-Level",
              EffectFn3(bi_level_color_map),
                        { {"Threshold", 127, 255, 127, true, 0} },
                        "Binary thresholding" },
```

to:

```cpp
            { "Bi-Level",
              EffectFn3(bi_level_color_map),
                        { {"Threshold", 127, 255, 127, true, 0} },
                        "Binary thresholding",
                        {} /* radio groups */,
                        "Lab 3 — intensity & colour" },
```

- [ ] **Step 2: Tag "Histogram & PDF" as the start of the histogram sub-section**

Change (~[main.cpp:2309](main.cpp#L2309)):

```cpp
            { "Histogram & PDF",
              EffectFn3(histogram_and_pdf),
                        {} /* no trackbars */,
                        "Compute and display histogram + PDF" },
```

to:

```cpp
            { "Histogram & PDF",
              EffectFn3(histogram_and_pdf),
                        {} /* no trackbars */,
                        "Compute and display histogram + PDF",
                        {} /* radio groups */,
                        "Lab 3 — histogram & quantization" },
```

- [ ] **Step 3: Tag "Morphology Lab7" as the start of the Lab 7 section**

Change (~[main.cpp:2335](main.cpp#L2335)):

```cpp
                        { "Morphology Lab7",
                            EffectFn3(morphology_lab7_combined),
                                                { {"Iterations (N)", 1, 15, 1, true, 1} },
                                                "N8-only morphology: dilation, erosion, opening, closing" },
```

to:

```cpp
                        { "Morphology Lab7",
                            EffectFn3(morphology_lab7_combined),
                                                { {"Iterations (N)", 1, 15, 1, true, 1} },
                                                "N8-only morphology: dilation, erosion, opening, closing",
                                                {} /* radio groups */,
                                                "Lab 7 — morphology & contours" },
```

- [ ] **Step 4: Tag "Selected Object Features" as the start of the Lab 4 section**

Change (~[main.cpp:2357](main.cpp#L2357)):

```cpp
                        { "Selected Object Features",
                            EffectFn3(selected_object_features),
                                                {} /* no trackbars */,
                                                "Click-select object and compute geometric features" },
```

to:

```cpp
                        { "Selected Object Features",
                            EffectFn3(selected_object_features),
                                                {} /* no trackbars */,
                                                "Click-select object and compute geometric features",
                                                {} /* radio groups */,
                                                "Lab 4 — geometric features" },
```

- [ ] **Step 5: Tag "Lab8 Statistics" as the start of the Lab 8 section**

Change the `"Lab8 Statistics"` entry (added in Task 2):

```cpp
                        { "Lab8 Statistics",
                            EffectFn3(lab8_statistics),
                                                {} /* no trackbars */,
                                                "Histogram, cumulative histogram, PDF, mean, stddev" },
```

to:

```cpp
                        { "Lab8 Statistics",
                            EffectFn3(lab8_statistics),
                                                {} /* no trackbars */,
                                                "Histogram, cumulative histogram, PDF, mean, stddev",
                                                {} /* radio groups */,
                                                "Lab 8 — statistical properties" },
```

- [ ] **Step 6: Build**

Run: `./run.sh build`
Expected: success; behaviour unchanged at runtime (the label is only read in Task 10).

- [ ] **Step 7: Commit**

```bash
git add main.cpp
git commit -m "feat(ui): annotate effect groups with lab-section labels"
```

---

## Task 10: Rework effect buttons as a grouped radio list in `slider.cpp`

**Files:**
- Modify: [src/slider/slider.cpp](src/slider/slider.cpp)
- Modify: [src/slider/slider.h](src/slider/slider.h) — one extra binding to keep Qt radio callbacks stable.

The key insight: OpenCV's `cv::createButton` with `cv::QT_RADIOBOX` creates a radio button. Successive `QT_RADIOBOX` buttons *within the same buttonbar* behave as one mutually-exclusive group. We insert a `QT_NEW_BUTTONBAR` before each new lab group (using either a dummy disabled push button or the first radio of that group, whichever Qt accepts). Qt HighGUI only fires `state != 0` for the newly-selected radio — same pattern the existing `ControlsManager::onRadioChanged` uses.

- [ ] **Step 1: Remove the old push-button path, add the new radio-list path**

Replace the body of `Slider::ensureEffectButtons()` (currently [src/slider/slider.cpp:98-113](src/slider/slider.cpp#L98-L113)) with:

```cpp
void Slider::ensureEffectButtons()
{
    if (entries_.empty()) {
        return;
    }

    // Rebuild every activation: the controls window was destroyed/recreated,
    // so all buttons are gone. Allocate enough room so the stable pointers
    // we hand Qt don't get invalidated mid-loop.
    effectButtonBindings_.reserve(entries_.size());
    effectButtonBindings_.clear();

    bool firstInGroup = true;  // force QT_NEW_BUTTONBAR before the very first entry

    for (std::size_t index = 0; index < entries_.size(); ++index) {
        const auto& entry = entries_[index];

        // When a groupLabel is present, emit a dedicated buttonbar header:
        //   1) a new button row (QT_NEW_BUTTONBAR) visually separates groups,
        //   2) a disabled push button carrying the group label acts as a section heading.
        if (!entry.groupLabel.empty() || firstInGroup) {
            std::string header = entry.groupLabel.empty()
                                   ? std::string("Effects")
                                   : entry.groupLabel;
            // Push-button on a new buttonbar — clicking it does nothing (callback
            // is installed but the binding is unused; the button is just a label).
            cv::createButton("— " + header + " —",
                             &Slider::onGroupHeaderPressed,
                             nullptr,
                             cv::QT_PUSH_BUTTON | cv::QT_NEW_BUTTONBAR,
                             0);
            firstInGroup = false;
        }

        // The actual effect selector — one radio per effect.
        effectButtonBindings_.push_back(EffectButtonBinding{this, index});
        int initialState = (index == currentIndex_) ? 1 : 0;
        cv::createButton(entry.name,
                         &Slider::onEffectButtonPressed,
                         &effectButtonBindings_.back(),
                         cv::QT_RADIOBOX,
                         initialState);
    }
}
```

- [ ] **Step 2: Update the radio-selection callback to the new fire semantics**

Replace `Slider::onEffectButtonPressed` (currently [src/slider/slider.cpp:115-123](src/slider/slider.cpp#L115-L123)) with:

```cpp
void Slider::onEffectButtonPressed(int state, void* userdata)
{
    auto* binding = static_cast<EffectButtonBinding*>(userdata);
    if (!binding || !binding->self) {
        return;
    }

    // QT_RADIOBOX fires state=1 for the newly-selected option and state=0
    // for the one being deselected. Only schedule a change for the former.
    if (state == 0) {
        return;
    }

    // Don't self-schedule when the callback fires as a side effect of the
    // radio group being (re)initialised to the current selection.
    if (binding->index == binding->self->currentIndex_) {
        return;
    }

    binding->self->pendingIndex_ = binding->index;
}
```

- [ ] **Step 3: Add a no-op callback for the group-header buttons**

Still in `slider.cpp`, append near the other static callbacks:

```cpp
void Slider::onGroupHeaderPressed(int /*state*/, void* /*userdata*/)
{
    // Intentional no-op: group-header buttons are decorative labels only.
}
```

- [ ] **Step 4: Declare `onGroupHeaderPressed` in `slider.h`**

In `src/slider/slider.h`, inside the `class Slider` body, add next to the existing `onEffectButtonPressed` declaration:

```cpp
    static void onGroupHeaderPressed(int state, void* userdata);
```

- [ ] **Step 5: Build**

Run: `./run.sh build`
Expected: success.

- [ ] **Step 6: Run & verify (the core UI change)**

Run: `./run.sh execute`
Expected:
- The control panel shows a vertical list of **radio buttons**, one per effect, with a filled circle next to the active effect.
- Section headers appear as greyed-out `— Lab 3 — intensity & colour —`, `— Lab 3 — histogram & quantization —`, `— Lab 7 — morphology & contours —`, `— Lab 4 — geometric features —`, `— Lab 8 — statistical properties —`. Clicking a header does nothing.
- Clicking a different radio switches the active effect; the trackbars / checkboxes / radios below update automatically (existing behaviour — `ControlsManager::activate` still runs).
- Arrow keys / number keys / `O` still work.
- Switching effects no longer spawns duplicate buttons.

**Rejection criteria** — if any of these occur, revert and debug:
- Duplicate effect buttons accumulating after a switch.
- The active effect losing its radio highlight after switch.
- The controls (trackbars/checkboxes) for the non-selected effect persisting after switch.

- [ ] **Step 7: Commit**

```bash
git add src/slider/slider.cpp src/slider/slider.h
git commit -m "feat(ui): render effect list as grouped radio buttons with lab section headers"
```

---

## Task 11: Document the new UI in `EFFECTS.md`

**Files:**
- Modify: `EFFECTS.md` — append a new final section.

- [ ] **Step 1: Append to the end of `EFFECTS.md`**

```markdown

## Controls panel layout

The control panel on the right of the main window is organized as:

1. **Effect list (top)** — one radio button per effect, grouped by lab with a heading line such as `— Lab 3 — intensity & colour —`. A filled circle marks the active effect. Click any radio to switch.
2. **Trackbars + ON/OFF checkboxes (middle)** — one per `TrackbarSpec` of the active effect; each checkbox toggles the slider against its `neutralValue`.
3. **Radio groups (bottom)** — one per `RadioGroupSpec` of the active effect.

Section 1 is rebuilt every time the active effect changes (because OpenCV's Qt HighGUI destroys and recreates the whole panel); sections 2 and 3 come from `ControlsManager::activate` called during that rebuild. Arrow keys, `1..9`, and `O` still work alongside the radios.

To add a new "section" (e.g. a Lab 9 group), set `groupLabel` on the first `SliderEntry` of that section inside the `Slider slider({ … })` initializer. Subsequent entries that leave `groupLabel` empty belong to the same group.
```

- [ ] **Step 2: Commit**

```bash
git add EFFECTS.md
git commit -m "docs(ui): document the new grouped-radio controls panel layout"
```

---

## Self-Review Checklist

**Spec coverage (Phase 1):**
- Ex 1 (mean, stddev, histogram, cumulative) → Task 2 using helpers from Task 1. ✔
- Ex 2 (iterative auto-binarize with user ε) → Task 3. ✔
- Ex 3 (`Iout_min`, `Iout_max`, γ, brightness offset + five histograms) → Task 4. ✔
- Ex 4 (histogram equalization + both histograms) → Task 5. ✔
- "Se revine la selectarea unei noi imagini" — satisfied by the existing `O` shortcut at [main.cpp:2441](main.cpp#L2441). ✔
- Educational code comments — baked into Tasks 1–5 as inline explanations of every formula. ✔
- Standalone Markdown explanation → `docs/lab8_explained.md` in Task 6. ✔

**Spec coverage (Phase 2):**
- "List so I can see all the effects names" → Task 10 converts effect buttons to a `QT_RADIOBOX` list. ✔
- "Based on the selected effect show the available settings" → already done by `ControlsManager::activate`, preserved by Tasks 8–10. ✔
- "More appealing and easy to use" → section headers (Tasks 9–10) and the radio-indicator for the active effect. ✔

**Placeholder scan:** no TBDs, no "similar to Task N" shortcuts, no steps without code where code is needed.

**Type consistency:**
- Helper names used in Tasks 2–5 match their definitions in Task 1: `computeCumulativeHistogram`, `computeMeanStdDev`, `drawCumulativeHistogram`, `drawHistogramIntWithOverlay`.
- Trackbar names consistent between registration and `controls.getEffective` calls: `"Epsilon x100"` (Task 3), `"Iout Min"` / `"Iout Max"` / `"Gamma x10"` / `"Brightness Offset"` (Task 4).
- Effect function names match their `EffectFn3(...)` registrations across all five effects.
- `SliderEntry` constructor parameter order matches how Task 9 passes `groupLabel` as the sixth argument: `(name, fn, trackbars, description, radioGroups, groupLabel)`.
- `Slider::onGroupHeaderPressed` declared in `.h` (Task 10 Step 4) and defined in `.cpp` (Task 10 Step 3).
