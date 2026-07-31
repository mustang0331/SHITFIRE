# Kofa / King Valley FO Heightmap

`KOFA_KING_VALLEY_FO_HEIGHTMAP.png` is a ready-to-load terrain image for Q13.

- **Source:** USGS 3D Elevation Program (3DEP) bare-earth DEM
- **Area:** Kofa Mountains / King Valley, southwest Arizona
- **Extent:** 33.195 to 33.285 N; 114.235 to 114.128 W (about 10 km square)
- **Source resolution:** 1,024 x 1,024 pixels, sampled from the USGS service
- **Elevation source range:** approximately 440 to 942 m above sea level
- **Q13 mapping:** source elevations are linearly remapped to image luminance 10--255, which this build interprets as 0--240 m above its simulation sea level.

The crop deliberately places a broad, low King Valley between stronger mountain and foothill terrain. This supports long observation lines while retaining ridges, draws, and relief that can mask targets.

To use it, open **Mission Menu -> Terrain -> Load DEM Heightmap...** and choose the PNG. The simulator resamples it internally to 512 x 512, fades the outside edge into water, rebuilds the world, and begins a new mission.

The accompanying `kofa_king_valley_usgs_3dep_raw.tif` is the downloaded source raster retained for provenance and for producing future variants; Q13 itself uses the PNG.
