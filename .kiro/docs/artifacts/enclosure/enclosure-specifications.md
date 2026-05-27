### ENCLOSURE & PCB LAYOUT DIMENSIONS SPECIFICATION DOCUMENT

**1. ENCLOSURE DIMENSIONS**
*   **External Dimensions (L x W x H):** 125.0 mm x 80.0 mm x 32.5 mm
*   **Internal Dimensions (L x W x H):** 119.0 mm x 74.0 mm x ~26.5 mm *(Internal height is estimated based on an assumed ~3mm wall thickness).*

---

**2. INTERNAL USABLE RECTANGLE AREAS**
*(These areas represent the maximum rectangular space available before intersecting with the 4 corner screw standoffs.)*
*   **Strict Clearance:** 98.0 mm x 52.2 mm *(Avoids all corner standoffs completely on both axes)*
*   **Length Ease (Restricted Width):** 119.0 mm x 52.2 mm *(Maximized length; fits between top and bottom standoffs)*
*   **Width Ease (Restricted Length):** 98.0 mm x 74.0 mm *(Maximized width; fits between left and right standoffs)*

---

**3. MOUNTING HOLES CONFIGURATION (ENCLOSURE HARDWARE)**
*   **Layout:** 2 Rows x 3 Columns (6 mounting posts total)
*   **Horizontal Spacing (Hole-to-Hole):** 41.8 mm between adjacent holes *(Total span from far-left to far-right is 83.6 mm)*
*   **Vertical Spacing (Hole-to-Hole):** 59.1 mm
*   **Enclosure Hole Inner Diameter ($\phi$):** ~2.1 mm *(Standard for M2.5 self-tapping screws)*
*   **Enclosure Standoff Outer Diameter ($\phi$):** ~8.5 mm
*   **Recommended PCB Drill Hole Diameter ($\phi$):** 2.5 mm to 3.2 mm *(Provides necessary clearance for an M2.5 or M3 screw to pass through the board)*

---

**4. THEORETICAL VS. ACTUAL PCB FOOTPRINT (WIDTH EASE CONFIGURATION)**

To ensure the PCB fits cleanly inside the enclosure without risking an "overfit" due to manufacturing tolerances, a standard **0.8 mm total reduction** (0.4 mm clearance gap per edge) is applied to the theoretical maximum bounds.

**A. Full-Size PCB Layout (Symmetric Centered Layout)**
*   **Theoretical Maximum Space:** 98.0 mm (L) x 74.0 mm (W)
*   **Actual Toleranced PCB Footprint:** 97.2 mm (L) x 73.2 mm (W)
*   **Distance from Actual PCB Edges to Hole Centers:**
    *   From Left/Right Edges to Left/Right Holes: **6.8 mm** 
    *   From Top/Bottom Edges to Top/Bottom Holes: **7.05 mm**

**B. Half-Size PCB Layout (With Dedicated Battery Space)**
*(This layout cuts the PCB symmetrically past the middle mounting holes, leaving the left side of the enclosure empty for a battery).*
*   **Theoretical Maximum Space:** 56.2 mm (L) x 74.0 mm (W)
*   **Actual Toleranced PCB Footprint:** 55.4 mm (L) x 73.2 mm (W)
*   **Distance from Actual PCB Edges to Hole Centers:**
    *   From Left Edge to Middle Holes: **6.8 mm**
    *   From Right Edge to Right Holes: **6.8 mm**
    *   From Top Edge to Top Holes: **7.05 mm**
    *   From Bottom Edge to Bottom Holes: **7.05 mm**
*   **Remaining Battery Allocation Space:** 41.8 mm (L) x 74.0 mm (W) *(Note: The two left-most mounting standoffs reside in this area).*

### UPDATED PCB FOOTPRINT (BATTERY ACCOMMODATION LAYOUT)

**1. BATTERY AREA ALLOCATION**
*   **Required Battery Area Width:** **43.0 mm** *(Allows 1.0 mm total clearance for two 21700 cells)*
*   **Total Battery Area Height:** 74.0 mm

**2. ACTUAL TOLERANCED PCB FOOTPRINT**
By sacrificing 1.2 mm of theoretical width from the left side, the PCB becomes narrower, but it ensures the batteries drop in without friction. The standard 0.8 mm (0.4 mm per edge) manufacturing clearance remains applied.
*   **Actual Toleranced Width:** **54.2 mm** *(Shrunk from 55.4 mm)*
*   **Actual Toleranced Height:** **73.2 mm** *(Remains unchanged)*

**3. DISTANCE FROM ACTUAL PCB EDGES TO HOLE CENTERS**
Because the left edge of the PCB was shifted further to the right to clear the batteries, the mounting holes are no longer perfectly symmetric relative to the physical board edges:
*   **Left Edge to Middle Holes:** **5.6 mm** *(Originally 6.8 mm. This narrower margin clears the batteries).*
*   **Right Edge to Right Holes:** **6.8 mm** *(Unchanged)*
*   **Hole-to-Hole Horizontal Span:** **41.8 mm** *(Hardware fixed)*
*   **Top/Bottom Edges to Holes:** **7.05 mm** *(Unchanged)*
*   **Hole-to-Hole Vertical Span:** **59.1 mm** *(Hardware fixed)*
