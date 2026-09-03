"""Regenerates the first-pass procedural materials under /Game/Materials.

These are the ONLY material assets in the project and they are 100%%
script-generated (delete + recreate on every run - the .uassets are build
products; THIS FILE is the source of truth). All four are parameter-driven
so BUILDER-ASSETS can later swap real texture packs into the same parameter
names (or regenerate with texture-sampling graphs) without touching C++.

  M_Terrain      slope/height-layered island surface (rock/scrub/sand/beach),
                 shader-side value noise for breakup. Params: RockColor,
                 ScrubColor, SandColor, BeachColor, SlopeRockStart,
                 SlopeRockFeather, ScrubStartM, ScrubFeatherM, BeachTopM,
                 NoiseScaleM, NoiseAmp, TerrainRoughness.
  M_Road         flat dark surfacing for road/path ribbons. Params: RoadColor,
                 RoadRoughness.
  M_Ocean        translucent sea sheet, panning sum-of-sines normal (no
                 textures). Params: OceanColor, WaveScaleM, WaveSpeed,
                 WaveBump, OceanOpacity, OceanRoughness.
  M_Placeholder  solid color for ISM/HISM placeholder boxes/cones. Params:
                 Color, PlaceholderRoughness.

Run headless:
  UnrealEditor-Cmd.exe <Shitfire.uproject> -run=pythonscript ^
      -script="port/unreal/Tools/gen_assets.py" -unattended -nopause
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
PKG_PATH = "/Game/Materials"


def fresh_material(name):
    full = "{}/{}".format(PKG_PATH, name)
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    mat = TOOLS.create_asset(name, PKG_PATH, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        raise RuntimeError("create_asset failed for " + full)
    return mat


def scalar_param(mat, name, value, x=-900, y=0):
    e = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    return e


def vector_param(mat, name, rgb, x=-900, y=0):
    e = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return e


def custom_node(mat, code, input_names, out_type, x=-400, y=0):
    e = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    e.set_editor_property("code", code)
    e.set_editor_property("output_type", out_type)
    ins = []
    for n in input_names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    e.set_editor_property("inputs", ins)
    return e


def finish(mat):
    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    if not EAL.save_loaded_asset(mat):
        raise RuntimeError("save failed for " + mat.get_name())
    unreal.log("gen_assets: wrote " + mat.get_path_name())


# ---------------------------------------------------------------- M_Terrain
TERRAIN_HLSL = """
// WP cm -> m; layer masks by height and slope; 2-tap value noise for breakup.
float3 wp = WP.xyz / 100.0;
float slope = saturate(1.0 - N.z);
float2 p = wp.xy / max(NoiseScaleM, 0.001);
float2 ip = floor(p);
float2 fp = frac(p);
fp = fp * fp * (3.0 - 2.0 * fp);
float h00 = frac(sin(dot(ip,                  float2(127.1, 311.7))) * 43758.5453);
float h10 = frac(sin(dot(ip + float2(1, 0),   float2(127.1, 311.7))) * 43758.5453);
float h01 = frac(sin(dot(ip + float2(0, 1),   float2(127.1, 311.7))) * 43758.5453);
float h11 = frac(sin(dot(ip + float2(1, 1),   float2(127.1, 311.7))) * 43758.5453);
float n = lerp(lerp(h00, h10, fp.x), lerp(h01, h11, fp.x), fp.y);
float3 c = lerp(BeachColor.rgb, SandColor.rgb, saturate(wp.z / max(BeachTopM, 0.01)));
float scrub = saturate((wp.z - ScrubStartM) / max(ScrubFeatherM, 0.01)) * (1.0 - slope);
c = lerp(c, ScrubColor.rgb, scrub);
c = lerp(c, RockColor.rgb, saturate((slope - SlopeRockStart) / max(SlopeRockFeather, 0.001)));
c *= (1.0 - NoiseAmp) + 2.0 * NoiseAmp * n;
return c;
"""

mat = fresh_material("M_Terrain")
wp = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1300, 0)
nrm = MEL.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1300, 200)
params = [
    ("RockColor",    vector_param(mat, "RockColor",  (0.23, 0.20, 0.18))),
    ("ScrubColor",   vector_param(mat, "ScrubColor", (0.11, 0.14, 0.06))),
    ("SandColor",    vector_param(mat, "SandColor",  (0.42, 0.35, 0.22))),
    ("BeachColor",   vector_param(mat, "BeachColor", (0.62, 0.56, 0.44))),
    ("SlopeRockStart",  scalar_param(mat, "SlopeRockStart", 0.13)),
    ("SlopeRockFeather", scalar_param(mat, "SlopeRockFeather", 0.10)),
    ("ScrubStartM",  scalar_param(mat, "ScrubStartM", 5.0)),
    ("ScrubFeatherM", scalar_param(mat, "ScrubFeatherM", 28.0)),
    ("BeachTopM",    scalar_param(mat, "BeachTopM", 2.5)),
    ("NoiseScaleM",  scalar_param(mat, "NoiseScaleM", 61.0)),
    ("NoiseAmp",     scalar_param(mat, "NoiseAmp", 0.24)),
]
cn = custom_node(mat, TERRAIN_HLSL, ["WP", "N"] + [n for n, _ in params],
                 unreal.CustomMaterialOutputType.CMOT_FLOAT3)
MEL.connect_material_expressions(wp, "", cn, "WP")
MEL.connect_material_expressions(nrm, "", cn, "N")
for pname, pexpr in params:
    MEL.connect_material_expressions(pexpr, "", cn, pname)
MEL.connect_material_property(cn, "", unreal.MaterialProperty.MP_BASE_COLOR)
rough = scalar_param(mat, "TerrainRoughness", 0.95, -400, 400)
MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
finish(mat)

# ------------------------------------------------------------------- M_Road
mat = fresh_material("M_Road")
# Ribbon winding is not proven both ways; two-sided makes it irrelevant
# (tiny triangle count, no perf concern).
mat.set_editor_property("two_sided", True)
col = vector_param(mat, "RoadColor", (0.10, 0.09, 0.08))
MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
rough = scalar_param(mat, "RoadRoughness", 0.92, -900, 300)
MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
finish(mat)

# ------------------------------------------------------------------ M_Ocean
OCEAN_HLSL = """
// Panning sum-of-sines tangent-space normal; the plane is flat/world-up so
// tangent space == world XY. No textures - BUILDER-ASSETS may replace.
float2 p = WP.xy / (100.0 * max(WaveScaleM, 0.01));
float t = T * WaveSpeed;
float dx = sin(p.x * 2.1 + t)       * 0.5 + sin(p.x * 0.7 + p.y * 1.3 + t * 1.7) * 0.5;
float dy = sin(p.y * 1.9 - t * 1.3) * 0.5 + sin(p.x * 1.1 - p.y * 0.9 + t * 0.9) * 0.5;
return normalize(float3(dx * WaveBump, dy * WaveBump, 1.0));
"""

mat = fresh_material("M_Ocean")
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("translucency_lighting_mode",
                        unreal.TranslucencyLightingMode.TLM_SURFACE)
wp = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1300, 0)
tim = MEL.create_material_expression(mat, unreal.MaterialExpressionTime, -1300, 200)
wscale = scalar_param(mat, "WaveScaleM", 55.0)
wspeed = scalar_param(mat, "WaveSpeed", 0.8)
wbump = scalar_param(mat, "WaveBump", 0.35)
cn = custom_node(mat, OCEAN_HLSL, ["WP", "T", "WaveScaleM", "WaveSpeed", "WaveBump"],
                 unreal.CustomMaterialOutputType.CMOT_FLOAT3)
MEL.connect_material_expressions(wp, "", cn, "WP")
MEL.connect_material_expressions(tim, "", cn, "T")
MEL.connect_material_expressions(wscale, "", cn, "WaveScaleM")
MEL.connect_material_expressions(wspeed, "", cn, "WaveSpeed")
MEL.connect_material_expressions(wbump, "", cn, "WaveBump")
MEL.connect_material_property(cn, "", unreal.MaterialProperty.MP_NORMAL)
col = vector_param(mat, "OceanColor", (0.02, 0.09, 0.13))
MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
rough = scalar_param(mat, "OceanRoughness", 0.06, -900, 300)
MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
opac = scalar_param(mat, "OceanOpacity", 0.85, -900, 500)
MEL.connect_material_property(opac, "", unreal.MaterialProperty.MP_OPACITY)
finish(mat)

# ------------------------------------------------------------ M_Placeholder
mat = fresh_material("M_Placeholder")
# ISM/HISM placeholder instancing needs the usage flag baked into the asset
# (the editor auto-adds it interactively; cooked/-game builds will not).
mat.set_editor_property("used_with_instanced_static_meshes", True)
col = vector_param(mat, "Color", (0.75, 0.25, 0.25))
MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
rough = scalar_param(mat, "PlaceholderRoughness", 0.7, -900, 300)
MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
finish(mat)

unreal.log("gen_assets: done")
