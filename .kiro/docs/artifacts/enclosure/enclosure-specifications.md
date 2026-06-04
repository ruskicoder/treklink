### ENCLOSURE & PCB LAYOUT DIMENSIONS SPECIFICATION DOCUMENT

**1. ENCLOSURE DIMENSIONS**
*   **External Dimensions (L x W x H):** 125.0 mm x 80.0 mm x 32.5 mm
*   **Internal Dimensions (L x W x H):** 119.0 mm x 74.0 mm x ~26.5 mm

---

**2. INTERNAL USABLE RECTANGLE AREAS**
*(These areas represent the maximum rectangular space available before intersecting with the 4 corner screw standoffs.)*
*   **Strict Clearance:** 98.0 mm x 52.2 mm *(Avoids all corner standoffs completely on both axes)*
*   **Length Ease (Restricted Width):** 119.0 mm x 52.2 mm *(Maximized length)*
*   **Width Ease (Restricted Length):** 98.0 mm x 74.0 mm *(Maximized width)*

---

**3. MOUNTING HOLES CONFIGURATION (ENCLOSURE HARDWARE)**
*   **Layout:** 2 Rows x 3 Columns (6 mounting posts total)
*   **Horizontal Spacing (Hole-to-Hole):** 41.8 mm between adjacent holes *(Total span from far-left to far-right is 83.6 mm)*
*   **Vertical Spacing (Hole-to-Hole):** 59.1 mm
*   **Enclosure Hole Inner Diameter ($\phi$):** ~2.1 mm *(Standard for M2.5 self-tapping screws)*
*   **Enclosure Standoff Outer Diameter ($\phi$):** ~8.5 mm
*   **Recommended PCB Drill Hole Diameter ($\phi$):** 2.5 mm to 3.2 mm *(Provides clearance for M2.5 or M3 mounting screws)*

---

**4. FINAL REVISED PCB FOOTPRINT (UNIFORM 5MM CLEARANCE)**

This half-board design uses the middle and right enclosure mounting posts. It applies a strict 5.0 mm boundary around the hole centers to guarantee manufacturing safety, concentric corner rounding, and generous wire routing paths.

**A. Actual PCB Dimensions**
*   **Total Width (X):** **51.8 mm**
*   **Total Height (Y):** **69.1 mm**
*   **Corner Radius:** **5.0 mm** *(Concentric with mounting hole centers)*

**B. Distance from PCB Edges to Hole Centers**
*   **Left Edge to Left-most Holes:** **5.0 mm**
*   **Right Edge to Right-most Holes:** **5.0 mm**
*   **Top Edge to Top-most Holes:** **5.0 mm**
*   **Bottom Edge to Bottom-most Holes:** **5.0 mm**

**C. Battery Area & Wire Clearances**
*   **Remaining Battery Area Width:** **44.0 mm** *(Comfortably exceeds the 43.0 mm minimum required for a dual 21700 cell pack + wrappers).*
*   **Right Enclosure Wall Wire Gap:** **2.2 mm** *(Free space between the right edge of the PCB and the physical enclosure wall).*
*   **Top / Bottom Enclosure Wall Wire Gaps:** **2.45 mm** *(Free space between the top/bottom edges of the PCB and the physical enclosure walls).*
