# Assignment Implementation Q&A

---

## Planning

**Q:** Take `assignment.md` from the root directory and create a plan to write the new effect into this project. I want it to be in a separate file so as to be able to differentiate between it and the normal project. I want you to explain what you did and why you did what you did every step of the way while implementing this and I want to write it myself so don't add the full code into the project but do give me the full code as you explain it.

**A:** See `PLAN.md` in the root for the full implementation walkthrough. Summary:

- Create `src/effects/assignment.h` and `src/effects/assignment.cpp`
- CMakeLists.txt already globs `src/*.cpp` — no CMake changes needed
- Add `#include "src/effects/assignment.h"` to main.cpp
- Register one `SliderEntry` in the Slider initializer list
- Assignment's interactive loop (typed threshold, key-press pause) is adapted to the trackbar framework: threshold becomes a slider, similarity matrix prints to console, re-render on every slider move

---

## Step 1 — Header

**Q:** Why do we use `#pragma once` here? And what does it do?

**A:** `#pragma once` is a header guard. It tells the compiler: include this file's contents only once per translation unit, no matter how many times `#include` appears.

Without it, if two different files both include `assignment.h`, the compiler sees the `contour_similarity` declaration twice → duplicate symbol error.

The old way:
```cpp
#ifndef ASSIGNMENT_H
#define ASSIGNMENT_H
// ... contents ...
#endif
```

`#pragma once` does the same job with less boilerplate. Used in all existing headers in this project (`output_images.h`, `controls_manager.h`, etc.) for the same reason.

`#pragma once` goes **only in `.h` files** — never `.cpp`. A `.cpp` is compiled exactly once and never `#include`d by anything, so there's nothing to guard against.

---

## Step 2 — BFS Labeling

**Q:** What does BFS with 4-connectivity mean? What's the difference from two-pass? What's a centroid? Why do we push to the queue as `{j, i}` instead of `{i, j}`? What is `auto [x, y]`? Does it unpack `q.front()`? What is `count` used for? Explain the whole if statement inside the for, and also what are `sumI` and `sumJ`?

**A:**

**BFS vs 4-connectivity vs two-pass**

4-connectivity means each pixel has exactly 4 neighbors: up, down, left, right. No diagonals. The assignment mandates this.

BFS uses a queue to "flood-fill" a connected region: mark a pixel, push its unlabeled black neighbors, repeat until queue is empty. Everything reachable via 4-neighbors gets the same label. Simple, one pass.

Two-pass: first pass scans left-to-right/top-to-bottom assigning temporary labels, recording "equivalences" when two differently-labeled regions touch. Second pass resolves all equivalences into final labels. Same result, more code to get right.

**Centroid**

Center of mass of the object — the average position of all its pixels.
```
centroid_x = (sum of all pixel column indices) / pixel count
centroid_y = (sum of all pixel row indices)    / pixel count
```

**Why `{j, i}` not `{i, j}`**

`cv::Point` stores `{x, y}`. In image space: `i` = row = vertical = **y**, `j` = column = horizontal = **x**. So `{j, i}` = `{x, y}`. Push it backwards and you'll read neighbors from the wrong axis.

**`auto [x, y]`**

C++17 structured bindings. Yes — it unpacks `q.front()` (a `cv::Point`) into its two members: `x = point.x` (column), `y = point.y` (row). Equivalent to:
```cpp
cv::Point pt = q.front();
int x = pt.x;
int y = pt.y;
```

**`count`**

Counts pixels in the object. Used only to divide the coordinate sums at the end: `sumJ/count` = average x, `sumI/count` = average y. That division is the centroid.

**The neighbor if-statement + sumI/sumJ**

```cpp
if (ny >= 0 && ny < src.rows && nx >= 0 && nx < src.cols   // in bounds
    && src.at<uchar>(ny, nx) == 0                           // pixel is black (object)
    && labels.at<int>(ny, nx) == 0) {                       // not yet labeled
    labels.at<int>(ny, nx) = nextLabel;  // claim it
    q.push({nx, ny});                    // schedule it
}
```

`sumI += y` accumulates row indices (the "I" dimension — vertical). `sumJ += x` accumulates column indices. Every pixel in the object contributes before its neighbors are processed. After the while loop, divide both by `count` → centroid.

---

**Q:** So `sumI` is number of rows in object and `sumJ` is number of columns in object. What's `nx` and `ny`? What's `dx` and `dy`? Can you explain why there is 4 in `std::array<std::pair<int, int>, 4>`? When do we end the BFS? I learn best on practical examples.

**A:**

**First: correction.** `sumI` is not "number of rows" — it's the **sum of row indices** of every pixel visited. Example:
```
Object pixels at rows: 2, 2, 3, 3
sumI = 2 + 2 + 3 + 3 = 10,  count = 4
centroid_y = sumI/count = 10/4 = 2.5   ← middle row
```

**dx/dy, nx/ny — practical example**

Say you're processing pixel at `x=3, y=2` (col 3, row 2). The 4 direction pairs are offsets:
```
{dy=-1, dx=0}  → up    → ny=1, nx=3  → pixel (row=1, col=3)
{dy= 1, dx=0}  → down  → ny=3, nx=3  → pixel (row=3, col=3)
{dy= 0, dx=-1} → left  → ny=2, nx=2  → pixel (row=2, col=2)
{dy= 0, dx= 1} → right → ny=2, nx=4  → pixel (row=2, col=4)
```

`nx` = **n**ew **x** (column of neighbor), `ny` = **n**ew **y** (row of neighbor).

**Why `4` in `std::array<std::pair<int,int>, 4>`**

`std::array` is a fixed-size array. The size is a **template parameter** — it must be known at compile time. You write `4` explicitly to say "this array holds exactly 4 elements."

**When does BFS end — practical example**

```
row:  0   1   2   3   4
    255 255 255 255 255
    255   0   0 255 255   ← pixels at col 1,2
    255   0   0 255 255
    255 255 255 255 255
```

Outer loop hits `i=1, j=1` → black, unlabeled → start BFS:

```
Queue: [{x=1,y=1}]   labels[1][1]=1

Pop {x=1,y=1}. Check 4 neighbors:
  up    (y=0,x=1): white → skip
  down  (y=2,x=1): BLACK, unlabeled → push, labels[2][1]=1
  left  (y=1,x=0): white → skip
  right (y=1,x=2): BLACK, unlabeled → push, labels[1][2]=1

Queue: [{x=1,y=2}, {x=2,y=1}]

... (continues until all 4 pixels labeled)

Queue: []  ← EMPTY → while loop ends
```

BFS ends when queue drains = when every reachable connected pixel has been claimed.

---

## Step 3 — Extended Contour Tracing

**Q:** Why do we start from the top-right corner? Are `ci` and `cj` corner pixels? Is `auto inside = [&](int r, int c) -> int` a lambda? What does it do? What are `r` and `c`? What does it return exactly? Why do we bit-shift what comes out of `inside()`? What is `p`? Can you explain what each value of the lookup table is — 16, 4, 3? What is `nd`? What is `dir`? Also where should I instantiate `chainCodes` and `contours`?

**A:**

**Why top-right corner of the start pixel?**

The start pixel is the topmost-leftmost pixel of the object. It has background above it guaranteed. The extended boundary runs *between* pixels, not on them. A "corner" is an intersection point between 4 pixels. The boundary enters at the right edge of the top side — that's corner `(startRow, startCol+1)`:

```
         ↓ corner (startRow, startCol+1)
. . . | . .
. . X | . .   ← boundary starts here, first move: Down
. . X X .
```

We start moving **Down** because we're tracing clockwise around the object.

**Are `ci` and `cj` corner pixels?**

They're not pixels — they're corner **coordinates**. A corner `(ci, cj)` is the point between 4 actual image pixels:
```
pixel(ci-1, cj-1) | pixel(ci-1, cj)
-------------------+------------------
pixel(ci,   cj-1) | pixel(ci,   cj)
```

**Is `inside` a lambda?**

Yes. `[&]` captures everything in scope by reference. `(int r, int c)` takes a pixel row and column. `-> int` returns an int. Returns `1` if pixel `(r,c)` belongs to the current object, `0` if background or out of bounds.

**Why bit-shift? What is `p`?**

`p` encodes the state of the 4 pixels around the current corner as a single number (0–15):
```
bit 3 = inside(ci-1, cj-1)   ← top-left pixel
bit 2 = inside(ci-1, cj)     ← top-right pixel
bit 1 = inside(ci,   cj-1)   ← bottom-left pixel
bit 0 = inside(ci,   cj)     ← bottom-right pixel
```

Example: TL=1, TR=0, BL=1, BR=1:
```
1<<3 | 0<<2 | 1<<1 | 1<<0 = 8|0|2|1 = 11
```

**LUT dimensions `[16][4][3]`**

- `16` — which 2×2 pattern (p from above, 0–15)
- `4` — which direction you came from (0=Right, 1=Up, 2=Left, 3=Down)
- `3` — the three output values: `{nextDir, deltaRow, deltaCol}`

**`nd` and `dir`**

- `dir` = direction of the last move made. Starts at 3 (Down).
- `nd` = **n**ext **d**irection — what the LUT says to move next.

After each step: `dir = nd`.

**Where to declare `chainCodes` and `contours`**

After `int numOfObjects = nextLabel - 1;`, before the second for loop:
```cpp
std::vector<std::vector<int>>       chainCodes(numOfObjects + 1);
std::vector<std::vector<cv::Point>> contours(numOfObjects + 1);
```

Size is `numOfObjects + 1` so index 1 = object 1. Index 0 goes unused.

---

## LUT Derivation

**Q:** How does the LUT know the next direction? How is it computed?

**A:** Built geometrically. At each corner, an edge out is only valid if the two pixels on either side are **different** (one object, one background):

| Move | Edge separates | Valid when |
|------|---------------|------------|
| Right → (ci, cj+1) | TR vs BR | TR ≠ BR |
| Left  → (ci, cj-1) | TL vs BL | TL ≠ BL |
| Up    → (ci-1, cj) | TL vs TR | TL ≠ TR |
| Down  → (ci+1, cj) | BL vs BR | BL ≠ BR |

For each pattern, figure out which edges are valid, then use previous direction to pick the one that doesn't reverse.

**Single-pixel cases (2 valid edges each):**
- p=1 (BR only): Right→Down, Down→Right
- p=2 (BL only): Left→Down, Down→Left
- p=4 (TR only): Right→Up, Up→Right
- p=8 (TL only): Left→Up, Up→Left

**Straight-through cases:**
- p=3 (BL+BR): Right→Right, Left→Left
- p=12 (TL+TR): Right→Right, Left→Left
- p=5 (TR+BR): Up→Up, Down→Down
- p=10 (TL+BL): Up→Up, Down→Down

**Complement patterns** (p=7,11,13,14) have the same valid edges as their single-pixel complements.

**The complete LUT:**
```cpp
// Directions: 0=Right 1=Up 2=Left 3=Down
// Bits: bit3=TL bit2=TR bit1=BL bit0=BR
static const int LUT[16][4][3] = {
//          dir=0(R)    dir=1(U)    dir=2(L)    dir=3(D)
/* p= 0 */ {{0,0,0},   {0,0,0},   {0,0,0},   {0,0,0}},
/* p= 1 */ {{3,1,0},   {0,0,0},   {0,0,0},   {0,0,1}},
/* p= 2 */ {{0,0,0},   {0,0,0},   {3,1,0},   {2,0,-1}},
/* p= 3 */ {{0,0,1},   {0,0,0},   {2,0,-1},  {0,0,0}},
/* p= 4 */ {{1,-1,0},  {0,0,1},   {0,0,0},   {0,0,0}},
/* p= 5 */ {{0,0,0},   {1,-1,0},  {0,0,0},   {3,1,0}},
/* p= 6 */ {{3,1,0},   {0,0,1},   {1,-1,0},  {2,0,-1}},
/* p= 7 */ {{0,0,0},   {2,0,-1},  {1,-1,0},  {0,0,0}},
/* p= 8 */ {{0,0,0},   {2,0,-1},  {1,-1,0},  {0,0,0}},
/* p= 9 */ {{1,-1,0},  {2,0,-1},  {3,1,0},   {0,0,1}},
/* p=10 */ {{0,0,0},   {1,-1,0},  {0,0,0},   {3,1,0}},
/* p=11 */ {{1,-1,0},  {0,0,1},   {0,0,0},   {0,0,0}},
/* p=12 */ {{0,0,1},   {0,0,0},   {2,0,-1},  {0,0,0}},
/* p=13 */ {{0,0,0},   {0,0,0},   {3,1,0},   {2,0,-1}},
/* p=14 */ {{3,1,0},   {0,0,0},   {0,0,0},   {0,0,1}},
/* p=15 */ {{0,0,0},   {0,0,0},   {0,0,0},   {0,0,0}},
};
```

---

**Q:** Why does my book say there are 12 possible situations instead of 16?

**A:** Because objects are **4-connected**. That eliminates 4 patterns instead of just 2.

- p=0 (all background): impossible on any boundary
- p=15 (all object interior): impossible on any boundary
- p=6 (TR=BL=object, TL=BR=background): requires two object pixels that are only diagonally connected — impossible for a 4-connected object
- p=9 (TL=BR=object, TR=BL=background): same reason

16 - 2 - 2 = **12**

Practical consequence: `LUT[6]` and `LUT[9]` will never be accessed when objects were labeled with 4-connectivity. Leave them as `{0,0,0}`.

---

## Step 5 — Similarity

**Q:** What's `similarity`? What are `ca` and `cb`? What are `a` and `b`? Why should `a` be shorter? Why do we take max? What's `maxS`? What's `k`? Can you explain the equation?

**A:**

**`similarity`** is a lambda that takes two chain codes and returns a number between -1.0 and 1.0 measuring how similar the shapes are.

**`ca` and `cb`** — chain code of object A and object B. Two sequences of direction numbers (0/1/2/3).

**`a` and `b`** — same two codes, but guaranteed `a` is shorter (`n` elements) and `b` is longer (`m` elements). The formula slides `a` along `b`, so `a` must be shorter.

**Why max?** You're looking for the best possible alignment. Two circles look identical but their chain codes might start at completely different points on the contour. You try every possible starting offset `j` and every rotation `k` and take the best score.

**`maxS`** — best score seen so far. Starts at `-1.0` because cosine's minimum is `-1`.

**`k`** — rotation offset applied to `a`'s directions. Handles the case where two identical shapes were traced in different orientations. `k` cycles through all 4 possible rotational offsets (0°, 90°, 180°, 270°).

**The equation piece by piece:**
```cpp
int ai   = (a[i] + k) % 4;          // a's direction rotated by k
int bi   = b[(i + j) % m];          // b's direction at position shifted by j
int diff = std::abs(ai - bi);        // raw direction difference (0..3)
int dist = std::min(diff, 4 - diff); // shortest path around the circle (0..2)
sum += std::cos(dist * M_PI / 2.0);
```

`dist` is the circular distance between two directions (0 and 3 are only 1 step apart on the direction circle):
```
dist=0 → cos(0)    =  1.0  ← same direction, perfect match
dist=1 → cos(π/2) =  0.0  ← 90° apart, neutral
dist=2 → cos(π)   = -1.0  ← opposite, mismatch
```

After summing over all `n` elements, divide by `n` to normalize → score always in `[-1, 1]`.

---

## Final Code Review

**Q:** I added everything, can you check the files now?

**A:** Build: clean. No errors. Three issues to fix:

1. `<random>` in the header — move to `.cpp` only
2. `src` assumed grayscale — add grayscale conversion at top of function
3. `d` shadows the distribution inside `similarity` — rename to `dist`

**Q:** Now check `assignment.cpp` only one last time before I run it.

**A:** *(After reading the final version — all three issues were already fixed.)*

Code is correct and ready to run. Load a binary image (black objects on white background, objects not touching margins), arrow-key to "Contour Similarity" effect, check console for similarity matrix, drag the Threshold slider to see cyan links appear between similar objects.
