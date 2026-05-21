# Lab 8 – Statistical Properties of Intensity Images Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add four new Slider effects to the image-processing playground covering Lab 8 exercises: (1) statistical properties display, (2) automatic iterative binarization, (3) pointwise transforms (negative, contrast stretch, gamma correction, brightness offset) with histograms, and (4) histogram equalization.

**Architecture:** Each exercise becomes a single `SliderEntry` registered in the `Slider` constructor inside `main()`. Computation helpers (mean, std-dev, cumulative histogram, histogram-to-image with overlaid stats) are added alongside the existing `computeHistogram` / `drawHistogramInt` utilities in `main.cpp`. All effects operate on grayscale input (`toGray8U`) and push their results through `OutputImages`, letting `renderGrid` lay them out. User interaction (epsilon, output range, gamma, offset) goes through `ControlsManager` trackbars — never `cv::createTrackbar` — with integer sliders mapped to the required real values by dividing by a fixed scale (gamma ×10, epsilon ×100).

**Tech Stack:** C++17, OpenCV 4 (Qt HighGUI build), the repo's own `ControlsManager` / `Slider` / `OutputImages` wrappers, spdlog-backed `INFO` / `DEBUG` macros for logging computed values.

---

## Verification Strategy (replaces traditional unit tests)

The project has **no test runner** (CLAUDE.md: "There are no tests or lint steps configured."). Verification for every task is:

1. `./run.sh build` — must succeed with no warnings in newly-added code.
2. `./run.sh build execute` — launches the Qt window.
3. Visually confirm the new effect appears in the carousel (arrow keys / number keys / Qt buttons).
4. Switch to the effect, exercise each control through its full range, confirm the grid updates.
5. Where an effect logs numeric stats (mean, stddev, threshold), read the terminal output to confirm the values are in the expected physical range.

Each task ends with a "Run & verify" step that lists exactly what to look for. Tasks commit only after that verification passes.

---

## File Structure

Only two files change:

- **`main.cpp`** — one modify-in-place operation per task:
  - Task 1 adds three static helpers near the existing histogram utilities (`computeCumulativeHistogram`, `computeMeanStdDev`, `drawCumulativeHistogram`, `drawHistogramIntWithOverlay`).
  - Tasks 2–5 each add one `void ..._lab8(const cv::Mat& src, OutputImages& outputs, ControlsManager& controls)` effect function plus a corresponding `SliderEntry` inside the `Slider slider(...)` initializer list in `main()`.
- **`EFFECTS.md`** — Task 6 appends a new "L8 / Statistical Properties" section documenting the four new effects.

No new source files, no `CMakeLists.txt` edits (globbing handles `src/` but everything here stays in `main.cpp`).

Placement inside `main.cpp`:
- Helpers: immediately after `drawHistogramFloat` (currently ends at [main.cpp:1629](main.cpp#L1629)), before the `// New effects:` divider at [main.cpp:1631](main.cpp#L1631).
- Effect functions: after `hsv_hue_quantization` (currently ends ~[main.cpp:1810](main.cpp#L1810)) and before the Lab 4 geometric-feature block at [main.cpp:1939](main.cpp#L1939). Group them under a new `// Lab 8 ---` banner for searchability.
- Registrations: inside the `Slider slider({ ... })` initializer list near [main.cpp:2275](main.cpp#L2275), appended after the `"Labeling Compare"` entry so the carousel order is chronological.

---

## Task 1: Lab 8 Shared Utilities (cumulative histogram, mean/stddev, stats overlay)

**Files:**
- Modify: `main.cpp` — insert helpers between [main.cpp:1629](main.cpp#L1629) and [main.cpp:1631](main.cpp#L1631).

These helpers are consumed by Tasks 2 and 5. Introducing them first keeps later diffs focused.

- [ ] **Step 1: Add cumulative histogram computation and renderer**

Insert the following block immediately after the closing brace of `drawHistogramFloat` at [main.cpp:1629](main.cpp#L1629):

```cpp
/**
 * @brief Computes the cumulative histogram C(k) = sum_{i=0..k} h(i).
 * @param hist Input histogram counts (typically length 256).
 * @return Cumulative histogram, same length as @p hist.
 */
static std::vector<int> computeCumulativeHistogram(const std::vector<int>& hist) {
    std::vector<int> cdf(hist.size(), 0);
    if (hist.empty()) return cdf;
    cdf[0] = hist[0];
    for (size_t i = 1; i < hist.size(); ++i) {
        cdf[i] = cdf[i - 1] + hist[i];
    }
    return cdf;
}

/**
 * @brief Draws a cumulative histogram as a filled-area line plot.
 * @param cdf Cumulative histogram counts.
 * @param width Output image width.
 * @param height Output image height.
 * @return Plot image (CV_8UC1, white background, black fill).
 */
static cv::Mat drawCumulativeHistogram(const std::vector<int>& cdf,
                                       int width = 256, int height = 200) {
    int bins = static_cast<int>(cdf.size());
    cv::Mat img(height, width, CV_8UC1, cv::Scalar(255));
    if (bins == 0) return img;
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

- [ ] **Step 2: Add mean / stddev computation from histogram**

Directly below the block from Step 1, insert:

```cpp
/**
 * @brief Computes mean and standard deviation from a 256-bin histogram.
 *
 * mu    = (1 / (W*H)) * sum(i * h(i))
 * sigma = sqrt( (1 / (W*H)) * sum( (i - mu)^2 * h(i) ) )
 *
 * @param hist 256-bin histogram counts.
 * @param totalPixels W*H (sum of histogram).
 * @param outMean Output: mean intensity.
 * @param outStdDev Output: standard deviation.
 */
static void computeMeanStdDev(const std::vector<int>& hist,
                              int totalPixels,
                              double& outMean,
                              double& outStdDev) {
    outMean = 0.0;
    outStdDev = 0.0;
    if (totalPixels <= 0 || hist.size() < 256) return;
    double mean = 0.0;
    for (int i = 0; i < 256; ++i) {
        mean += static_cast<double>(i) * hist[i];
    }
    mean /= static_cast<double>(totalPixels);

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

- [ ] **Step 3: Add histogram renderer with overlaid stats text**

Directly below the block from Step 2, insert:

```cpp
/**
 * @brief Draws a histogram (as drawHistogramInt does) and overlays
 *        mu / sigma text in the top-left corner.
 * @param hist Histogram counts.
 * @param mean Mean to overlay.
 * @param stddev Standard deviation to overlay.
 * @param width Output width.
 * @param height Output height.
 * @return CV_8UC3 BGR image (colored text requires 3 channels).
 */
static cv::Mat drawHistogramIntWithOverlay(const std::vector<int>& hist,
                                           double mean,
                                           double stddev,
                                           int width = 256,
                                           int height = 220) {
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

- [ ] **Step 4: Build and confirm helpers compile**

Run: `./run.sh build`
Expected: build succeeds, no new warnings referencing any of the four new helpers.

- [ ] **Step 5: Commit**

```bash
git add main.cpp
git commit -m "feat(lab8): add cumulative histogram, mean/stddev, stats-overlay helpers"
```

---

## Task 2: Lab 8 Exercise 1 — Statistical Properties effect

Displays: source, histogram (with mu/sigma overlaid), cumulative histogram, PDF. Also logs `mu` and `sigma` through `INFO` on every recompute so the numbers are visible even when the window is small.

**Files:**
- Modify: `main.cpp` — new function after `hsv_hue_quantization`, new `SliderEntry` in the `Slider slider(...)` list.

- [ ] **Step 1: Add the effect function**

After the closing brace of `hsv_hue_quantization` (the last of the existing Lab 3 effects, around [main.cpp:1810](main.cpp#L1810)), insert a clear banner and the new function:

```cpp
// ---------------------------------------------------------------------------
// Lab 8 – Statistical properties of intensity images
// ---------------------------------------------------------------------------

/**
 * @brief Lab 8, Exercise 1. Displays grayscale source together with its
 *        histogram, cumulative histogram, and PDF. Also overlays and logs
 *        the mean (mu) and standard deviation (sigma).
 * @param src Input source image.
 * @param outputs Output image vector.
 * @param controls Unused controls parameter.
 */
void lab8_statistics(const cv::Mat& src, OutputImages& outputs, ControlsManager& /*controls*/) {
    cv::Mat gray = toGray8U(src);

    auto hist = computeHistogram(gray, 256);
    auto cdf  = computeCumulativeHistogram(hist);
    auto pdf  = computePDF(hist, gray.rows * gray.cols);

    double mean = 0.0;
    double stddev = 0.0;
    computeMeanStdDev(hist, gray.rows * gray.cols, mean, stddev);

    INFO("[Lab8] mean={:.4f} stddev={:.4f}", mean, stddev);

    outputs.push_back({"Histogram", drawHistogramIntWithOverlay(hist, mean, stddev)});
    outputs.push_back({"Cumulative", drawCumulativeHistogram(cdf)});
    outputs.push_back({"PDF", drawHistogramFloat(pdf)});
}
```

- [ ] **Step 2: Register the effect**

Inside the `Slider slider({ ... })` initializer list in `main()`, after the `"Labeling Compare"` entry (around [main.cpp:2378](main.cpp#L2378)), append:

```cpp
                        { "Lab8 Statistics",
                            EffectFn3(lab8_statistics),
                                                {} /* no trackbars */,
                                                "Histogram, cumulative histogram, PDF, mean, stddev" },
```

- [ ] **Step 3: Build**

Run: `./run.sh build`
Expected: success, no warnings in `lab8_statistics`.

- [ ] **Step 4: Run & verify**

Run: `./run.sh execute`
Expected:
- A `"Lab8 Statistics"` button appears in the control bar.
- Selecting it shows source + three charts in the grid.
- Terminal prints a line like `[Lab8] mean=113.72 stddev=48.38` for `assets/images/cameraman.bmp` (values should match the PDF examples, which cite e.g. `mu=113.723, sigma=48.3751`).
- Pressing `O` and loading a different image refreshes mu/sigma in both the overlay and the log.

- [ ] **Step 5: Commit**

```bash
git add main.cpp
git commit -m "feat(lab8): add ex1 statistics effect (hist + CDF + PDF + mu/sigma)"
```

---

## Task 3: Lab 8 Exercise 2 — Automatic Iterative Binarization

Iterative threshold: `T_0 = (I_min + I_max) / 2`; each step splits pixels into two groups by the current threshold, computes their mean intensities `mu1, mu2`, and sets `T_{k+1} = (mu1 + mu2) / 2`. Stops when `|T_k - T_{k-1}| < epsilon` (user-configurable).

**Files:**
- Modify: `main.cpp` — new function + new `SliderEntry`.

- [ ] **Step 1: Add the effect function**

Append below `lab8_statistics`:

```cpp
/**
 * @brief Lab 8, Exercise 2. Automatic iterative binarization.
 *        Slider "Epsilon x100" provides the termination error (value / 100).
 * @param src Input source image.
 * @param outputs Output image vector.
 * @param controls Controls manager: reads "Epsilon x100".
 */
void lab8_auto_binarize(const cv::Mat& src, OutputImages& outputs, ControlsManager& controls) {
    int epsInt = controls.getEffective("Epsilon x100");
    if (epsInt < 1) epsInt = 1;
    double epsilon = static_cast<double>(epsInt) / 100.0;

    cv::Mat gray = toGray8U(src);
    auto hist = computeHistogram(gray, 256);

    // Find I_min and I_max from non-empty histogram bins.
    int iMin = 0;
    int iMax = 255;
    while (iMin < 256 && hist[iMin] == 0) ++iMin;
    while (iMax > 0   && hist[iMax] == 0) --iMax;
    if (iMin >= iMax) {
        // Degenerate (single intensity) — produce an all-zero mask.
        cv::Mat bin(gray.size(), CV_8UC1, cv::Scalar(0));
        outputs.push_back({"Binary", bin});
        return;
    }

    double T = 0.5 * (iMin + iMax);
    double prevT = T + 10.0 * epsilon + 1.0;  // guarantee first iteration runs
    int iterations = 0;
    const int maxIterations = 1000;
    while (std::abs(T - prevT) >= epsilon && iterations < maxIterations) {
        prevT = T;
        long long n1 = 0, n2 = 0;
        double s1 = 0.0, s2 = 0.0;
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
        double mu1 = (n1 > 0) ? (s1 / static_cast<double>(n1)) : static_cast<double>(iMin);
        double mu2 = (n2 > 0) ? (s2 / static_cast<double>(n2)) : static_cast<double>(iMax);
        T = 0.5 * (mu1 + mu2);
        ++iterations;
    }

    int thresh = std::clamp(static_cast<int>(std::round(T)), 0, 255);
    INFO("[Lab8] auto-binarize: Imin={} Imax={} T={:.4f} iter={} eps={:.4f}",
         iMin, iMax, T, iterations, epsilon);

    cv::Mat bin(gray.size(), CV_8UC1);
    for (int r = 0; r < gray.rows; ++r) {
        const uchar* s = gray.ptr<uchar>(r);
        uchar* d = bin.ptr<uchar>(r);
        for (int c = 0; c < gray.cols; ++c) {
            d[c] = (s[c] > thresh) ? 255 : 0;
        }
    }

    // Annotate the binary image with the chosen threshold.
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

- [ ] **Step 2: Register the effect**

Inside the `Slider` initializer list, append directly after the `"Lab8 Statistics"` entry added in Task 2:

```cpp
                        { "Lab8 Auto Binarize",
                            EffectFn3(lab8_auto_binarize),
                                                { {"Epsilon x100", 10, 500, 10, true, 1} },
                                                "Iterative automatic thresholding (T stops when |dT|<eps)" },
```

Slider semantics: raw slider 1..500 maps to epsilon 0.01..5.0; default 10 → `epsilon = 0.10` (matches the 0.1 example in the lab PDF); `neutralValue = 10` keeps the same behavior when the ON/OFF toggle is disabled.

- [ ] **Step 3: Build**

Run: `./run.sh build`
Expected: success.

- [ ] **Step 4: Run & verify**

Run: `./run.sh execute`
Expected:
- `"Lab8 Auto Binarize"` appears in the carousel.
- On `cameraman.bmp` the binary image clearly separates foreground and background; the overlaid `T = ...` text reflects a reasonable threshold (~88–130 range is normal).
- Log line prints `T=165.xxxx` (or similar) when loading an image whose PDF example cites `T = 165` (the second PDF screenshot's image).
- Dragging the `"Epsilon x100"` slider up (e.g., to 200 = eps 2.0) finishes in fewer iterations; slider at 1 (eps 0.01) still converges within 1000 iterations.
- Toggling the slider OFF falls back to `epsilon = 0.10` (neutral) and still produces a valid threshold.

- [ ] **Step 5: Commit**

```bash
git add main.cpp
git commit -m "feat(lab8): add ex2 iterative auto-binarization effect"
```

---

## Task 4: Lab 8 Exercise 3 — Pointwise Transforms (negative, contrast stretch, gamma, brightness)

Four output images + four histograms (plus the original) in a single effect.

Controls:
- Trackbar `"Iout Min"` — integer in `[0, 255]`, default 30, neutral 0.
- Trackbar `"Iout Max"` — integer in `[0, 255]`, default 220, neutral 255.
- Trackbar `"Gamma x10"` — integer in `[1, 30]`, default 10 (→ 1.0, identity), neutral 10.
- Trackbar `"Brightness Offset"` — integer in `[0, 255]`, default 128, neutral 128, treated as `raw - 128` so it spans `-128..+127`.

**Files:**
- Modify: `main.cpp` — new function + new `SliderEntry`.

- [ ] **Step 1: Add the effect function**

Append below `lab8_auto_binarize`:

```cpp
/**
 * @brief Lab 8, Exercise 3. Pointwise transforms on grayscale.
 *        Outputs: Negative, ContrastStretch, Gamma, Brightness,
 *        plus histograms for the original and each transform.
 * @param src Input source image.
 * @param outputs Output image vector.
 * @param controls Controls manager: reads
 *   "Iout Min", "Iout Max", "Gamma x10", "Brightness Offset".
 */
void lab8_transforms(const cv::Mat& src, OutputImages& outputs, ControlsManager& controls) {
    int outMin = std::clamp(controls.getEffective("Iout Min"), 0, 255);
    int outMax = std::clamp(controls.getEffective("Iout Max"), 0, 255);
    if (outMax < outMin) std::swap(outMin, outMax);

    int gammaInt = controls.getEffective("Gamma x10");
    if (gammaInt < 1) gammaInt = 1;
    double gamma = static_cast<double>(gammaInt) / 10.0;

    int brightnessRaw = controls.getEffective("Brightness Offset");
    int offset = brightnessRaw - 128;

    cv::Mat gray = toGray8U(src);
    auto histOriginal = computeHistogram(gray, 256);

    // Input range comes from the actual image min/max (contrast stretch).
    double inMinD = 0.0, inMaxD = 0.0;
    cv::minMaxLoc(gray, &inMinD, &inMaxD);
    int inMin = static_cast<int>(inMinD);
    int inMax = static_cast<int>(inMaxD);
    double inSpan = std::max(1.0, static_cast<double>(inMax - inMin));

    cv::Mat negative(gray.size(), CV_8UC1);
    cv::Mat contrast(gray.size(), CV_8UC1);
    cv::Mat gammaImg(gray.size(), CV_8UC1);
    cv::Mat brightness(gray.size(), CV_8UC1);

    // Precompute LUTs for each transform — fast + makes math explicit.
    uchar lutNeg[256];
    uchar lutCon[256];
    uchar lutGam[256];
    uchar lutBri[256];
    for (int v = 0; v < 256; ++v) {
        lutNeg[v] = static_cast<uchar>(255 - v);

        double stretched = outMin + (v - inMin) * (outMax - outMin) / inSpan;
        lutCon[v] = static_cast<uchar>(std::clamp(stretched, 0.0, 255.0));

        double g = 255.0 * std::pow(static_cast<double>(v) / 255.0, gamma);
        lutGam[v] = static_cast<uchar>(std::clamp(g, 0.0, 255.0));

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

- [ ] **Step 2: Register the effect**

Inside the `Slider` initializer list, after the `"Lab8 Auto Binarize"` entry, append:

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
- Selecting `"Lab8 Transforms"` populates the grid with four images + five histograms, laid out by `renderGrid` (labels `"Negative"`, `"Contrast"`, `"Gamma"`, `"Brightness"`, `"Hist Original"`, etc.).
- Dragging `"Iout Min"` up and/or `"Iout Max"` down visibly shrinks the contrast histogram (the "shrink" case described in the lab PDF).
- Dragging `"Iout Min"` to 0 and `"Iout Max"` to 255 produces a wider histogram than the original ("stretch").
- `"Gamma x10"` below 10 brightens mid-tones (gamma encoding, <1); above 10 darkens them (>1).
- `"Brightness Offset"` at 128 leaves the image unchanged; 255 saturates to white; 0 pushes the image dark.
- Toggling any slider OFF returns its neutral identity behavior (e.g. offset-off → unchanged image; gamma-off → unchanged image; outMin-off=0, outMax-off=255 → contrast stretch matches the source range).

- [ ] **Step 5: Commit**

```bash
git add main.cpp
git commit -m "feat(lab8): add ex3 transforms (negative, contrast, gamma, brightness)"
```

---

## Task 5: Lab 8 Exercise 4 — Histogram Equalization

Builds `FDPC(k) = (1 / (W·H)) Σ_{i=0..k} h(i)` and maps `I_out = round(255 · FDPC(I_in))`. No controls.

**Files:**
- Modify: `main.cpp` — new function + new `SliderEntry`.

- [ ] **Step 1: Add the effect function**

Append below `lab8_transforms`:

```cpp
/**
 * @brief Lab 8, Exercise 4. Histogram equalization.
 *        Outputs the equalized image alongside original/equalized histograms
 *        (both with mu/sigma overlaid so the smoothing effect is visible).
 * @param src Input source image.
 * @param outputs Output image vector.
 * @param controls Unused controls parameter.
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

    // Build the cumulative PDF (FDPC) and the 0..255 LUT.
    double invTotal = 1.0 / static_cast<double>(total);
    uchar lut[256];
    double running = 0.0;
    for (int i = 0; i < 256; ++i) {
        running += static_cast<double>(hist[i]) * invTotal;
        int mapped = static_cast<int>(std::round(255.0 * running));
        lut[i] = static_cast<uchar>(std::clamp(mapped, 0, 255));
    }

    cv::Mat equalized(gray.size(), CV_8UC1);
    for (int r = 0; r < gray.rows; ++r) {
        const uchar* s = gray.ptr<uchar>(r);
        uchar* d = equalized.ptr<uchar>(r);
        for (int c = 0; c < gray.cols; ++c) d[c] = lut[s[c]];
    }

    auto histEq = computeHistogram(equalized, 256);

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

- [ ] **Step 2: Register the effect**

Inside the `Slider` initializer list, after the `"Lab8 Transforms"` entry, append:

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
- `"Lab8 Equalize"` appears in the carousel.
- The equalized output on a low-contrast image (e.g. `bonemarr.bmp` or `cell.bmp`) shows visibly more tonal separation than the original.
- The `"Hist Equalized"` chart is visibly flatter and wider than `"Hist Original"`.
- Terminal log line prints both mu/sigma pairs; equalized sigma ≥ original sigma for typical images.
- Pressing `Enter` saves all three outputs to `assets/export/`.

- [ ] **Step 5: Commit**

```bash
git add main.cpp
git commit -m "feat(lab8): add ex4 histogram equalization effect"
```

---

## Task 6: Documentation — update EFFECTS.md

**Files:**
- Modify: `EFFECTS.md` — append a new section at the end.

- [ ] **Step 1: Append the Lab 8 section**

At the end of `EFFECTS.md`, append:

```markdown

## L8 / Statistical Properties

Effects added for Lab 8 (statistical properties of intensity images). All operate on grayscale input.

* **Lab8 Statistics** — displays the 256-bin histogram (with mu / sigma overlaid), the cumulative histogram, and the PDF of the source. Mean and standard deviation are also printed to the terminal.
* **Lab8 Auto Binarize** — iterative automatic thresholding. `Epsilon x100` slider (raw / 100) controls the termination error; the algorithm iterates `T_{k+1} = (mu1 + mu2) / 2` until `|T_k - T_{k-1}| < epsilon`. The final threshold is annotated on the binary output and logged.
* **Lab8 Transforms** — four pointwise transforms plus histograms:
    * Negative (`255 - I`)
    * Contrast stretch into `[Iout Min, Iout Max]` (input range is the image's actual min/max)
    * Gamma correction, `Gamma x10 / 10` (slider 1..30 → 0.1..3.0)
    * Brightness offset, `Brightness Offset - 128` (slider 0..255 → offset -128..+127)

    Histograms of the original and all four transforms are rendered alongside.

* **Lab8 Equalize** — histogram equalization using `I_out = 255 · FDPC(I_in)`. The equalized image and both histograms (with mu / sigma) are displayed.

Implementation helpers shared with these effects: `computeCumulativeHistogram`, `computeMeanStdDev`, `drawCumulativeHistogram`, and `drawHistogramIntWithOverlay` — all in `main.cpp`.
```

- [ ] **Step 2: Commit**

```bash
git add EFFECTS.md
git commit -m "docs(lab8): document new statistical-properties effects in EFFECTS.md"
```

---

## Self-Review Checklist (completed)

**Spec coverage:**
- Ex1 (mean, stddev, histogram, cumulative C) → Task 2 (uses helpers from Task 1). ✔
- Ex2 (iterative auto-binarize, user epsilon) → Task 3. ✔
- Ex3 (Iout_min, Iout_max, gamma, offset; negative + contrast + gamma + brightness + histograms for all) → Task 4. ✔
- Ex4 (histogram equalization + both histograms) → Task 5. ✔
- "Se revine la selectarea unei noi imagini" requirement → already satisfied by the existing `O` shortcut / file dialog (CLAUDE.md, [main.cpp:2441](main.cpp#L2441)).

**Placeholder scan:** No TBDs, no "similar to Task N" shortcuts — every code block is standalone and complete.

**Type consistency:**
- Helper names match their use sites: `computeCumulativeHistogram`, `computeMeanStdDev`, `drawCumulativeHistogram`, `drawHistogramIntWithOverlay` — all consistent across Tasks 1–5.
- Control names match across the definition (Task 4 registration) and the `controls.getEffective(...)` calls in `lab8_transforms`: `"Iout Min"`, `"Iout Max"`, `"Gamma x10"`, `"Brightness Offset"`.
- Control name in Task 3 matches between registration and `lab8_auto_binarize`: `"Epsilon x100"`.
- Effect function names (`lab8_statistics`, `lab8_auto_binarize`, `lab8_transforms`, `lab8_histogram_equalization`) match their `EffectFn3(...)` registrations.
