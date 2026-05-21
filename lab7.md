# Lab Context: Morphological Operations on Binary Images

[cite_start]**Source Material:** "Operații morfologice pe imagini binare" by Cristian Vancea[cite: 1, 2].

## 1. Core Concepts
* [cite_start]**Binary Images:** The primary data structure for the operations[cite: 1].
* [cite_start]**Structuring Element ($B$):** The shape used to probe the image[cite: 4, 332].
    * [cite_start]*Versions mentioned:* 4-neighbor and 8-neighbor[cite: 5, 344].
* [cite_start]**Dilation ($\oplus$):** Expands object boundaries[cite: 6]. 
* [cite_start]**Erosion ($\Theta$):** Shrinks object boundaries[cite: 106].
* [cite_start]**Opening ($\circ$):** Used to eliminate small object zones[cite: 332, 333].
* [cite_start]**Closing ($\bullet$):** Used to fill small object holes[cite: 335].
* [cite_start]**Idempotence:** A property applying to both Opening and Closing operations[cite: 332, 334].

## 2. Algorithms & Formulas
*(Note: As per instructions, these are mentioned with their core formulas without extensive detailing).*

* [cite_start]**Dilation:** Image traversed until the center of the structural element overlaps an object pixel[cite: 12].
* [cite_start]**Erosion:** All pixels covered by the structural element must be object pixels to mark the destination pixel as an object[cite: 110, 116].
* [cite_start]**Opening Formula:** $A\circ B=(A\Theta B)\oplus B$[cite: 332].
* [cite_start]**Closing Formula:** $A\bullet B=(A\oplus B)\Theta B$[cite: 335].
* [cite_start]**Contour Extraction Algorithm:** $\beta(A)=A-(A\Theta B)$[cite: 334, 335].
* **Morphological Region Filling Algorithm:**
    * [cite_start]*Initial state:* $X_{0}=\{p\}$, where $p$ is an internal point[cite: 337].
    * [cite_start]*Iterative step:* $X_{k}=(X_{k-1}\oplus B)-A$[cite: 340].
    * [cite_start]*Condition:* Repeat until $X_{k}=X_{k-1}$[cite: 339, 341].
    * [cite_start]*Result:* $rez=A\cup X_{k}$[cite: 341].

## 3. Practical Activity (Implementation Requirements)

### Tasks 1 & 2: Basic Operations and Idempotence
* [cite_start]**Input:** Read a grayscale image and a natural value $N > 0$ from the keyboard[cite: 351].
* [cite_start]**Output:** Display the original image alongside the results of dilation, erosion, opening, and closing applied $N$ times[cite: 352]. [cite_start]Display opening and closing applied exactly once to demonstrate idempotence[cite: 352, 353].
* [cite_start]**Required Version constraint:** You MUST use ONLY the **8-neighbor structuring element**[cite: 354].

### Tasks 3 & 4: Contour and Region Filling
* [cite_start]**Input:** Select an original grayscale image[cite: 354].
* [cite_start]**Output 1:** Display the detected contour using morphological operations[cite: 355].
* [cite_start]**Interaction:** Use a mouse callback to detect a click inside the contour[cite: 356, 359]. [cite_start](Relevant OpenCV functions: `setMouseCallback`, `EVENT_LBUTTONDOWN` [cite: 359, 361]).
* [cite_start]**Output 2:** Display the filled object using the Morphological Region Filling Algorithm[cite: 356].
* [cite_start]**Required Version constraint:** You MUST use ONLY the **8-neighbor structuring element** for both contour extraction and region filling[cite: 357].