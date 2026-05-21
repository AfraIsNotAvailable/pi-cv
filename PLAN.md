# Plan: Contour-Based Similarity Measurement (Assignment)

## Context

The assignment requires: label black objects in a binary image → trace their extended contours → build 4-direction chain codes → compute pairwise cross-correlation similarity → draw cyan lines between similar objects above a user threshold.

The code must live in a **separate file** (not buried in main.cpp) so it's clearly distinguishable from existing lab effects. The assignment's interactive loop (key-press pause, typed threshold) gets adapted to the trackbar framework: threshold becomes a slider, similarity matrix prints once to the console, re-render happens on every slider move.

---

## File Structure

```
src/effects/
  assignment.h      ← declares void contour_similarity(src, outputs, controls)
  assignment.cpp    ← implements all 6 phases
main.cpp            ← #include "src/effects/assignment.h" + one SliderEntry added
```

**Why separate file:** CMakeLists.txt already does `file(GLOB_RECURSE Sources "src/*.cpp")` — any `.cpp` under `src/` is compiled automatically. No CMake edits needed. Keeping it in `src/effects/` makes it immediately obvious this is assignment code, not core lab scaffolding.

---

## Implementation Walkthrough (step by step, with code to write yourself)

### Step 1 — Create the header (`src/effects/assignment.h`)

**Why:** The function must be visible to main.cpp's Slider registration. A header provides the declaration without coupling the two translation units.

```cpp
#pragma once
#include <opencv2/opencv.hpp>
#include "src/common/output_images.h"
#include "src/controls/controls_manager.h"

void contour_similarity(const cv::Mat& src,
                        OutputImages& outputs,
                        ControlsManager& controls);
```

---

### Step 2 — Phase 1: Connected-Component Labeling (`assignment.cpp`)

**Why BFS with 4-connectivity:** The assignment explicitly forbids diagonal neighbors. BFS naturally processes all pixels in a connected region before moving on. Two-pass also works but BFS is simpler to verify.

**Why compute centroid here:** We need centroids for Phase 6's cyan line drawing. Computing during labeling avoids a second pass.

```cpp
// label image: 0 = background, 1..N = object labels
cv::Mat labels(src.size(), CV_32S, cv::Scalar(0));
std::vector<cv::Point2d> centroids; // index 0 = object label 1

int nextLabel = 1;
for (int i = 0; i < src.rows; ++i) {
    for (int j = 0; j < src.cols; ++j) {
        if (src.at<uchar>(i,j) == 0 && labels.at<int>(i,j) == 0) {
            // BFS
            std::queue<cv::Point> q;
            q.push({j, i});
            labels.at<int>(i,j) = nextLabel;
            double sumI = 0, sumJ = 0;
            int count = 0;
            while (!q.empty()) {
                auto [x, y] = q.front(); q.pop();
                sumI += y; sumJ += x; ++count;
                // 4-neighbors only
                for (auto [dy, dx] : std::array<std::pair<int,int>,4>{{{-1,0},{1,0},{0,-1},{0,1}}}) {
                    int ny = y+dy, nx = x+dx;
                    if (ny>=0 && ny<src.rows && nx>=0 && nx<src.cols
                        && src.at<uchar>(ny,nx)==0
                        && labels.at<int>(ny,nx)==0) {
                        labels.at<int>(ny,nx) = nextLabel;
                        q.push({nx, ny});
                    }
                }
            }
            centroids.push_back({sumJ/count, sumI/count}); // (x,y)
            ++nextLabel;
        }
    }
}
int numObjects = nextLabel - 1;
```

---

### Step 3 — Phase 2: Extended Contour Tracing

**Why extended contour:** A standard pixel-border contour creates adjacency problems between regions. The extended boundary is shifted half a pixel — it lives on the *edges between pixels*, not on pixels themselves. This gives each object a unique, non-overlapping boundary.

**The algorithm (5.8) uses a 2×2 sliding window.** The "position" being traced is a *corner* between four pixels. At corner (i,j), the four surrounding pixels are:

```
(i-1, j-1) | (i-1, j)
------------+----------
  (i,   j-1)|  (i,   j)
```

Encode which of these 4 pixels belong to the current object as a 4-bit number:
- bit 3 = (i-1, j-1), bit 2 = (i-1, j), bit 1 = (i, j-1), bit 0 = (i, j)
- 1 = inside object, 0 = background

**Why a lookup table:** There are 16 possible 2×2 patterns × 4 possible previous directions = 64 entries. A lookup table makes it fast and avoids a giant if-else chain.

The lookup table maps `(pattern4bit, prevDir)` → `(nextDir, deltaRow, deltaCol)`. Directions: 0=Right, 1=Up, 2=Left, 3=Down.

**Finding the start:** Scan top-to-bottom, left-to-right for first pixel in region. Start corner is at (row, col+1) — the top-right corner of that pixel. Initial direction: Down (3).

**Termination:** Stop when you return to the start corner moving in the start direction.

```cpp
// for each object label:
for (int lbl = 1; lbl <= numObjects; ++lbl) {
    // 1. find start pixel
    int startRow = -1, startCol = -1;
    for (int i = 0; i < src.rows && startRow < 0; ++i)
        for (int j = 0; j < src.cols && startRow < 0; ++j)
            if (labels.at<int>(i,j) == lbl) { startRow=i; startCol=j; }

    // start corner: top-right of start pixel
    int ci = startRow, cj = startCol + 1;
    int dir = 3; // Down
    std::vector<int> chainCode;
    std::vector<cv::Point> contourCorners;

    do {
        contourCorners.push_back({cj, ci});

        // read 2×2 window around corner (ci, cj)
        auto inside = [&](int r, int c) -> int {
            if (r<0||r>=src.rows||c<0||c>=src.cols) return 0;
            return labels.at<int>(r,c)==lbl ? 1 : 0;
        };
        int p = (inside(ci-1,cj-1)<<3)|(inside(ci-1,cj)<<2)
               |(inside(ci,  cj-1)<<1)|(inside(ci,  cj));

        // lookup table: [pattern][prevDir] → {nextDir, dRow, dCol}
        // (fill this table per Algorithm 5.8 from lecture notes)
        static const int LUT[16][4][3] = { /* ... */ };

        int nd = LUT[p][dir][0];
        ci += LUT[p][dir][1];
        cj += LUT[p][dir][2];
        chainCode.push_back(nd);
        dir = nd;

    } while (ci != startRow || cj != startCol+1 || dir != 3);

    chainCodes[lbl] = chainCode;
    contours[lbl]   = contourCorners;
}
```

**The LUT itself** — you'll fill this in based on the lecture notes' table for Algorithm 5.8. Each of the 16 patterns (0b0000 through 0b1111) has specific rules for each incoming direction. Patterns 0 (all background) and 15 (all foreground) are degenerate — the algorithm shouldn't reach them.

---

### Step 4 — Phase 3: Chain Code

Already captured in `chainCode.push_back(nd)` above. Directions match: 0=Right, 1=Up, 2=Left, 3=Down as required.

---

### Step 5 — Phase 4: Visualization

**Why random colors per object:** Distinguishes separate objects visually. The assignment specifies using `<random>`, not `rand()`.

```cpp
cv::Mat vis(src.size(), CV_8UC3, cv::Scalar(255,255,255)); // white bg

std::default_random_engine gen(42); // fixed seed for reproducibility
std::uniform_int_distribution<int> d(0, 255);

for (int lbl = 1; lbl <= numObjects; ++lbl) {
    cv::Vec3b color(d(gen), d(gen), d(gen));

    // fill object interior black
    for (int i = 0; i < src.rows; ++i)
        for (int j = 0; j < src.cols; ++j)
            if (labels.at<int>(i,j) == lbl)
                vis.at<cv::Vec3b>(i,j) = {0,0,0};

    // draw extended contour corners (corner = edge between pixels, draw at floor)
    for (auto& pt : contours[lbl]) {
        int r = pt.y, c = pt.x;
        // corner (r,c) is top-left of pixel (r,c), draw that pixel
        if (r>=0 && r<vis.rows && c>=0 && c<vis.cols)
            vis.at<cv::Vec3b>(r,c) = color;
    }
}
outputs.push_back({"Contours", vis});
```

**Note on "pause":** The assignment says pause here for a keypress. In the effect framework, `outputs.push_back` already shows the image. The user can see it and adjust the threshold slider at leisure — no artificial blocking is needed. If you want strict compliance, add `cv::waitKey(0)` before computing similarity, but this will freeze the UI until a key is pressed every render.

---

### Step 6 — Phase 5: Cross-Correlation Similarity

**Why this formula:** Cross-correlation measures how well two sequences match at different offsets. Taking the max over all offsets makes it shift-invariant. Iterating over 4 rotations (add k mod 4) makes it rotation-invariant. The `cos(min(|a-b|, 4-|a-b|) * π/2)` term converts direction differences into a similarity score: same direction → 1, opposite → -1.

```cpp
auto similarity = [&](const std::vector<int>& ca, const std::vector<int>& cb) -> double {
    // ensure a is shorter
    const auto& a = (ca.size() <= cb.size()) ? ca : cb;
    const auto& b = (ca.size() <= cb.size()) ? cb : ca;
    int n = a.size(), m = b.size();

    double maxS = -1.0;
    for (int k = 0; k < 4; ++k) { // rotation of a
        for (int j = 0; j < m; ++j) { // shift of b
            double sum = 0;
            for (int i = 0; i < n; ++i) {
                int ai = (a[i] + k) % 4;
                int bi = b[(i + j) % m];
                int diff = std::abs(ai - bi);
                int d = std::min(diff, 4 - diff);
                sum += std::cos(d * M_PI / 2.0);
            }
            maxS = std::max(maxS, sum / n);
        }
    }
    return maxS;
};

// compute full matrix and log it
std::vector<std::vector<double>> simMatrix(numObjects+1,
                                           std::vector<double>(numObjects+1, 0.0));
for (int a = 1; a <= numObjects; a++)
    for (int b = a+1; b <= numObjects; b++)
        simMatrix[a][b] = simMatrix[b][a] = similarity(chainCodes[a], chainCodes[b]);

// print to console once
for (int a = 1; a <= numObjects; a++) {
    for (int b = 1; b <= numObjects; b++)
        printf("%6.3f ", simMatrix[a][b]);
    printf("\n");
}
```

---

### Step 7 — Phase 6: Threshold + Cyan Lines

**Why trackbar instead of typed input:** The framework re-calls the effect on every control change. A trackbar gives real-time feedback as you drag the threshold. The assignment's "type a negative number to exit" loop is replaced by: just switch to another effect via arrow keys.

**Trackbar spec:** `{"Threshold x100", 80, 100, 0, true, 0}` → divide by 100.0 to get 0.00–1.00.

```cpp
double thresh = controls.getEffective("Threshold x100") / 100.0;

cv::Mat result = vis.clone(); // vis built in Phase 4
for (int a = 1; a <= numObjects; a++) {
    for (int b = a+1; b <= numObjects; b++) {
        if (simMatrix[a][b] > thresh) {
            cv::Point pa(centroids[a-1].x, centroids[a-1].y);
            cv::Point pb(centroids[b-1].x, centroids[b-1].y);
            cv::line(result, pa, pb, cv::Scalar(255, 255, 0), 1); // cyan = BGR(255,255,0)
        }
    }
}
outputs.push_back({"Similarity Links", result});
```

---

### Step 8 — Register in main.cpp

**Why only main.cpp needs touching:** The Slider constructor owns the list of effects. We add one entry and include the header — no other framework files change.

At top of main.cpp, after existing includes:
```cpp
#include "src/effects/assignment.h"
```

In the Slider initializer list, before the closing `}`, after `"Spatial Filter"` (main.cpp:3017):
```cpp
{"Contour Similarity",
 EffectFn3(contour_similarity),
 {{"Threshold x100", 80, 100, 0, true, 0}},
 "Label objects, trace extended contours, cross-correlation similarity",
 {} /* radio groups */,
 "Assignment — contour similarity"},
```

---

## Critical Files

| File | Action |
|------|--------|
| `src/effects/assignment.h` | **Create** — function declaration |
| `src/effects/assignment.cpp` | **Create** — all 6 phases |
| `main.cpp` (top) | **Edit** — add `#include "src/effects/assignment.h"` |
| `main.cpp:3017` | **Edit** — insert SliderEntry before closing `}` of Slider |

No CMake changes. No other `src/` changes.

---

## Verification

1. `./run.sh build` — must compile clean
2. `./run.sh execute` — load a binary image (black objects on white background, objects not touching margins)
3. Arrow-key to `"Contour Similarity"` effect
4. Check console for similarity matrix printout
5. Drag `Threshold x100` slider — cyan lines appear/disappear between similar objects
6. `Enter` key saves output images to `assets/export/`
