# Urban-Design_Procedural-3d-Land-map

Urban-Design_Procedural-3d-Land-map is a browser-based procedural urban design system that generates parcel-aware 2D plans and 3D massing from configurable grammar rules, then exposes a separate client-facing inspection experience for plot-level and city-level analysis.

## Features

- Procedural parcel subdivision and shape-grammar-driven building generation.
- Forest-attractor-aware typology and height gradient controls.
- Multiple view modes including technical and rendered visualization.
- First-person street-level navigation in both generator and client inspection app.
- Standalone client inspection app (`client-inspection.html`) with plot search, selection, and inspector panels.
- Export support for high-resolution 2D and 3D snapshots.

## Getting Started

1. Install dependencies:
   ```bash
   npm install
   ```
2. Start development server:
   ```bash
   npm run dev
   ```
3. Open the generator app:
   - `http://localhost:5173/`
4. Open the client-facing inspection app:
   - `http://localhost:5173/client-inspection.html`

## Controls

### Generator App
- `A`: Toggle attractors
- `V`: Toggle first-person walk mode
- `M`: Toggle technical view
- `N`: Toggle rendered view
- Left click on 2D map in first-person workflow to set jump target.

### Client Inspection App
- `V`: Toggle first-person mode
- Left click: Select plot/building
- Right drag: Orbit
- Middle drag: Pan
- Scroll: Zoom
