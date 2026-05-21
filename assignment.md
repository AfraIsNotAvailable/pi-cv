# Project Assignment: Contour-Based Similarity Measurement

## Overview
A grayscale image will be selected from the disk via a graphical user interface. The application will detect distinct black objects, trace their extended contours, and calculate the morphological similarity between every pair of objects using the cross-correlation of their chain codes. Based on a user-defined threshold, similar objects will be linked visually.

---

## Phase 1: Object Detection and Labeling

You must identify the black objects in the selected image using a connected-component labeling algorithm.
* **Image Properties:** In the binary image, objects are represented by black pixels (value 0) and the background by white pixels (value 255)[cite: 326]. The objects do not contain holes and do not touch the image margins.
* **Neighborhood Rule:** During labeling, diagonal neighborhoods must **not** be taken into account. You must strictly use 4-connectivity. The 4-neighborhood of a pixel at $(i,j)$ is defined as the pixels directly above, below, left, and right: $N_{4}(i,j)=\{(i-1,j), (i,j-1), (i+1,j), (i,j+1)\}$[cite: 328, 329, 330].
* **Labeling Algorithm:** You may implement either a Breadth-First Search (BFS) traversal (using a queue) or a Two-Pass algorithm with equivalence classes [cite: 350-356, 379-394]. 
* **Center of Gravity:** While labeling, compute the center of gravity (centroid) for each distinct object. You will need these coordinates for the final visualization step.

---

## Phase 2: Extended Contour Tracing

For each labeled object, you must extract its extended contour. 
* **Definition of Extended Boundary:** An extended boundary solves the problem of adjacent regions not sharing a standard pixel border[cite: 237, 239]. It is a single common border that has the exact shape of the inter-pixel boundary, but shifted one half-pixel down and one half-pixel right[cite: 239, 241].
* **Tracing Algorithm (Algorithm 5.8):**
    1.  **Start Pixel:** Search the image line-by-line (left-to-right, top-to-bottom) until you find the first pixel belonging to the labeled region[cite: 285]. 
    2.  **Initial Move:** The first move along the traced boundary from the starting pixel is strictly downwards[cite: 286]. 
    3.  **Look-up Table Traversal:** Trace the extended boundary by evaluating $2\times2$ pixel windows[cite: 283]. At each step, evaluate the local configuration of region and background pixels in this window, alongside your previous direction of movement, to determine the next move[cite: 283, 304]. Continue tracing until a closed extended border results[cite: 287].

---

## Phase 3: Chain Code Representation

As you trace the extended contour, you must represent it efficiently using a chain code[cite: 52].
* A chain code represents the contour as a sequence of integer steps, indicating the direction of movement from one contour pixel to the next[cite: 56, 80]. 
* Because the extended boundary is constructed using 4-connectivity logic for its segments, you will use a 4-direction chain code:
    * **0:** Right
    * **1:** Up
    * **2:** Left
    * **3:** Down

---

## Phase 4: Contour Visualization

Once the contours are computed:
1.  Create a color image of identical size to the original selected image.
2.  Draw the interior of the objects in black.
3.  Draw the extended contour of each object in a uniquely generated random color. 
    * *Implementation Note for Random Colors:* Use the C++ `<random>` library for high-quality randomization:
        ```cpp
        #include <random>
        std::default_random_engine gen;
        std::uniform_int_distribution<int> d(0, 255);
        uchar color_component = d(gen);
        ```
        [cite: 481, 482, 483, 484]
4.  The application must pause here. Wait for the user to press a key before moving to the next processing step.

---

## Phase 5: Chain Code Cross-Correlation & Similarity

You will calculate the similarity between every pair of labeled objects (e.g., Object A and Object B) using the cross-correlation of their 4-directional chain codes[cite: 164].

Let $a$ be the shorter chain code of size $n$, and $b$ be the longer chain code of size $m$ ($n \le m$)[cite: 168, 169, 170].
1.  **Base Cross-Correlation Formula (for 4 neighborhoods):**
    The correlation between the two sequences at a shift $j$ is calculated as:
    $$\phi_{ab}(j) = \frac{1}{n} \cdot \sum_{i=0}^{n-1} \cos\left[ \min\left( |a_i - b_{(i+j)\pmod m}|, 4 - |a_i - b_{(i+j)\pmod m}| \right) \cdot \frac{\pi}{2} \right]$$
    where $j = 0 \dots m-1$[cite: 166, 167, 171].

2.  **Orientation Invariance:**
    To account for rotational differences and cover the entire range of 4 orientations used by the extended contour, apply the formula 4 times. In each iteration, increment the directional values of the shorter contour $a$ by $k$ (modulo 4):
    $$a^{(k)} = \{ a_i^{(k)} \mid a_i^{(k)} = (a_i + k) \pmod 4, \quad i \in [0, n-1] \}$$
    where $k = 0 \dots 3$[cite: 175, 176, 178, 181].

3.  **Maximum Similarity:**
    The final degree of similarity $S$ between the two contours is the absolute maximum correlation across all possible shifts $j$ and all 4 rotation alignments $k$:
    $$S = \max_{k=0\dots3} \left( \max_{j=0\dots m-1} \phi_{a^{(k)}b}(j) \right)$$
    [cite: 179, 180, 181].

---

## Phase 6: User Interaction & Final Output

1.  **Similarity Matrix:** Output the full calculated similarity matrix between all pairs of objects to the screen/console.
2.  **Threshold Input:** Prompt the user to enter a numerical similarity threshold from the keyboard.
3.  **Visual Links:** On the color image generated in Phase 4, draw **turquoise (cyan)** lines connecting the centers of gravity of any two objects whose calculated similarity strictly exceeds the entered threshold.
4.  **Application Loop:**
    * The user can repeatedly enter new threshold values. 
    * For each new value, refresh the displayed image and redraw the cyan lines accordingly.
    * If the user inputs a **negative** threshold value, exit the current visualization loop and return to the file selection step to load a new test image.