# gaming

## Aurès Battlefield Terrain GPU — `aures_terrain_gpu.html`

A single-file WebGL2 generator (Three.js r128) for an 8 km × 8 km battlefield map set in the Aurès mountains. Open the file in a desktop browser that supports WebGL2 and float render targets.

### Terrain (seeded and GPU-generated)
- **Landforms:** border mountain massifs (ridged noise), a meandering river valley (وادي) that narrows into a gorge, 4 tributary ravines (شعاب), a mesa plateau (هضبة) with a stepped escarpment, a plain (سهل) with fields, and a basin (منخفض) with a lake.
- **Erosion:** GPU hydraulic and thermal erosion.
- **Post-erosion sculpting:** roads (cut and fill) with switchbacks up the escarpment, a bridge over the river, a terraced village spur, a flat pad for the outpost, and the river channel.
- **Shading:** limestone strata on cliffs, scree, snow, forest floor, field patchwork with hedgerows, a dirt road with ruts, a gravel riverbed, and animated water.

### Detail
- **Procedural trees:** Atlas cedar, Aleppo pine, holm oak and juniper. Each has recursive branches and leaf cards from a generated atlas, 3 LODs, wind sway and translucency.
- **GPU grass:** up to 600k instanced clumps around the camera, with wind, dry/green variation and poppies.
- **Rocks and boulders:** placed on scree and rocky slopes.
- **Lighting:** a dynamic sun shadow map near the camera combined with the baked terrain shadow, plus procedural clouds.
- **Settlements:** a stone village (dechra), a hexagonal French outpost (watchtowers, barracks, HQ, comms tower, trucks), a stone bridge, and an overwatch ruin.
- **Capture points:** A–D, shown with an in-world HUD.

### Map and play
- **Tactical map:** contours, hillshade, a 1 km grid, roads, river, ravines and landform labels. Click it to travel; press `M` to enlarge.
- **Walk mode:** press `F` for first-person walking (WASD, Shift to run, Space to jump).
- **Quality presets:** low to ultra, plus an automatic resolution scaler.

### Exports
- 16-bit PNG and RAW `.r16` heightmaps, with Unreal landscape scale shown in the panel.
- An RGBA weightmap PNG (R = rock, G = grass, B = forest, A = farmland).
- A layout JSON: road, river and ravine splines, houses, fort, bridges, capture points and lake level, in metres.

URL parameters `?seed=&res=&q=&hm=&ero=` preset the controls.
