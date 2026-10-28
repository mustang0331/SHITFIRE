"""Regenerates the burst-FX materials under /Game/FX (BUILDER-BRIDGE, P3/P4).

Same regime as gen_assets.py: the .uassets are build products, THIS FILE is
the source of truth (delete + recreate on every run); every asset is on
port/unreal/uasset-allowlist.txt. Consumed by the C++ FX actors
(Source/ShitfireGame/*/bridge/fx/) - parameter names are the contract:

  M_FxFlash  unlit additive blob for the impact flash.
             Params: Color, Intensity.
  M_FxDirt   unlit translucent thrown-dirt column; world-pos value noise
             breaks the silhouette. Params: Color, Opacity, NoiseScaleM.
  M_FxSmoke  unlit translucent persistent smoke column (the training
             feature); noise scrolls slowly UP so the column reads as rising.
             Params: Color, Opacity, NoiseScaleM, ScrollMps.
  M_FxFlare  unlit additive glow blob for the illum round. Params: Color,
             Intensity (driven per-frame from C++ for gutter/flicker).

ART-a: every material carries VisNvg + VisThermal through the preview wrap
(vis_material.py; needs MPC_Vision, so gen_assets.py runs first). The glows
wrap Color BEFORE the * Intensity multiply into Emissive; the masked
columns wrap BaseColor (opacity mask untouched). Defaults: M_FxFlash and
M_FxFlare {1, white}, M_FxDirt {0.85, 0.62 grey}, M_FxSmoke {0.60, 0.44
grey}.

Run headless:
  UnrealEditor-Cmd.exe <Shitfire.uproject> -run=pythonscript ^
      -script="port/unreal/Tools/gen_fx_assets.py" -unattended -nopause
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import vis_material as vm  # noqa: E402

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
PKG_PATH = "/Game/FX"
MPC = vm.ensure_vision_mpc()


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
    unreal.log("gen_fx_assets: wrote " + mat.get_path_name())


def unlit_additive_glow(name, rgb, intensity, nvg, th):
    """Flash + flare share a structure: emissive = Color * Intensity.

    Intensity is ABSOLUTE LUMINANCE (cd/m2): unlit emissive bypasses
    lighting, and the pawn camera runs fixed manual exposure at EV100 ~13
    where sunlit ground sits around 10^4 nits - a glow needs the same order
    of magnitude or it renders black (2026-09-03 FX probe: Intensity 40
    was invisible)."""
    mat = fresh_material(name)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    col = vector_param(mat, "Color", rgb)
    inten = scalar_param(mat, "Intensity", intensity, -900, 300)
    # vis wrap sits on Color BEFORE the luminance multiply (the preview
    # recolours the glow, Intensity still sets how bright it is)
    wrap = vm.wrap_vis_preview(mat, col, "", nvg, th, MPC, -700, 0)
    mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -500, 100)
    MEL.connect_material_expressions(wrap, "", mul, "A")
    MEL.connect_material_expressions(inten, "", mul, "B")
    MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(mat)


# 2-tap value noise over a vertical slice of world space (same hash as
# gen_assets.py terrain noise). WP cm -> m. ScrollMps > 0 pans the noise
# upward so a static column mesh reads as rising smoke; 0 = static dirt.
COLUMN_ALPHA_HLSL = """
float2 p = float2(WP.x + WP.y, WP.z * 0.85 - T * ScrollMps * 100.0) / (100.0 * max(NoiseScaleM, 0.01));
float2 ip = floor(p);
float2 fp = frac(p);
fp = fp * fp * (3.0 - 2.0 * fp);
float h00 = frac(sin(dot(ip,                float2(127.1, 311.7))) * 43758.5453);
float h10 = frac(sin(dot(ip + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
float h01 = frac(sin(dot(ip + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
float h11 = frac(sin(dot(ip + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
float n = lerp(lerp(h00, h10, fp.x), lerp(h01, h11, fp.x), fp.y);
// High floor: a smoke/dirt column is near-opaque through its body - the
// 2026-09-03 FX-probe screenshots showed 0.55-floor columns reading as
// translucent blue glass against the sea at 3.2 km.
return saturate(Opacity * (0.78 + 0.45 * n));
"""


def lit_masked_column(name, rgb, opacity, noise_scale_m, scroll_mps, nvg, th):
    """LIT + MASKED: Color is an ALBEDO the sun lights (correct under the
    fixed manual EV100 ~13 camera and any later TOD), and the noise output
    drives the OPACITY MASK - solid smoke body with ragged noise-clipped
    edges, which is how a smoke pillar actually reads at 3 km.

    History (2026-09-03 FX probe, both prior takes): unlit emissive 0.6 =
    0.6 nits = black under day exposure (take 1); lit TRANSLUCENT read as a
    ghost against the hazed sea (take 2/3). Masked kills the ghosting, and
    the C++ end-of-life Opacity ramp becomes a progressive dissolve."""
    mat = fresh_material(name)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("opacity_mask_clip_value", 0.35)
    mat.set_editor_property("two_sided", True)
    col = vector_param(mat, "Color", rgb)
    vm.wrap_vis_property(mat, col, "", unreal.MaterialProperty.MP_BASE_COLOR,
                         nvg, th, MPC, -500, -300)
    rough = scalar_param(mat, "ColumnRoughness", 0.95, -900, 150)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    wp = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1300, 0)
    tim = MEL.create_material_expression(mat, unreal.MaterialExpressionTime, -1300, 200)
    opac = scalar_param(mat, "Opacity", opacity, -900, 300)
    nscale = scalar_param(mat, "NoiseScaleM", noise_scale_m, -900, 450)
    scroll = scalar_param(mat, "ScrollMps", scroll_mps, -900, 600)
    cn = custom_node(mat, COLUMN_ALPHA_HLSL,
                     ["WP", "T", "Opacity", "NoiseScaleM", "ScrollMps"],
                     unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    MEL.connect_material_expressions(wp, "", cn, "WP")
    MEL.connect_material_expressions(tim, "", cn, "T")
    MEL.connect_material_expressions(opac, "", cn, "Opacity")
    MEL.connect_material_expressions(nscale, "", cn, "NoiseScaleM")
    MEL.connect_material_expressions(scroll, "", cn, "ScrollMps")
    MEL.connect_material_property(cn, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    finish(mat)


# glows: luminance-scaled for the EV100 ~13 day exposure (see docstring)
WHITE = (1.0, 1.0, 1.0)
unlit_additive_glow("M_FxFlash", (1.0, 0.85, 0.55), 60000.0, 1.0, WHITE)
unlit_additive_glow("M_FxFlare", (1.0, 0.92, 0.75), 25000.0, 1.0, WHITE)
# dirt: dark earth albedo, chunky noise, no scroll
lit_masked_column("M_FxDirt", (0.16, 0.12, 0.08), 0.85, 7.0, 0.0, 0.85, (0.62, 0.62, 0.62))
# smoke: pale dust albedo, slow upward scroll
lit_masked_column("M_FxSmoke", (0.63, 0.60, 0.55), 0.95, 11.0, 1.6, 0.60, (0.44, 0.44, 0.44))

unreal.log("gen_fx_assets: done")
