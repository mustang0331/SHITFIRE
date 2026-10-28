"""Shared helpers for the ART-a optic-signature convention (PORT_PLAN.md
section ART, row ART-a). Imported by gen_assets.py, gen_fx_assets.py,
gen_unit_assets.py and import_assets.py; not a script itself.

The browser tags every material with `vis = {nvg: 0..1, th: [r,g,b]}`
(src/js/04-world.js:93-159): NVG reads `nvg` as the tube luminance, thermal
passes `th` through, and an UNTAGGED material silently falls back to a
luminance guess. The port makes the tag a hard requirement instead:

  * every UMaterial under /Game carries a scalar parameter `VisNvg` and a
    vector parameter `VisThermal` (rgb, alpha 1), both CONNECTED into the
    graph through the preview wrap below (a dangling parameter would be
    stripped by the compiler and stop resolving);
  * /Game/Materials/MPC_Vision is a MaterialParameterCollection with one
    scalar `VisMode` (0 = day, 1 = NVG preview, 2 = thermal preview);
  * the material's day colour runs through ONE custom node
        float3 c = Day;
        c = lerp(c, VisNvg.xxx, saturate(VisMode));
        c = lerp(c, VisThermal.rgb, saturate(VisMode - 1.0));
        return c;
    so VisMode 0 is pixel-identical to the untagged graph;
  * /Game/Materials/M_ShitfireMaster is the parent of every imported pack
    material (BaseTex * Tint -> wrap -> BaseColor, Roughness, the Vis
    params, skeletal + ISM + Nanite usage flags baked in).

Parameter names (VisNvg, VisThermal, VisMode, BaseTex, Tint, Roughness) are
the contract with Source/ShitfireGame - do not rename. check_vis_tags.py is
the gate: an untagged UMaterialInterface or an empty mesh slot fails it.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

MAT_DIR = "/Game/Materials"
MPC_NAME = "MPC_Vision"
MPC_PATH = MAT_DIR + "/" + MPC_NAME
MASTER_NAME = "M_ShitfireMaster"
MASTER_PATH = MAT_DIR + "/" + MASTER_NAME

VIS_MODE_PARAM = "VisMode"
VIS_NVG_PARAM = "VisNvg"
VIS_THERMAL_PARAM = "VisThermal"

# Master defaults (contract point 7); C++ overrides per unit at runtime.
MASTER_VIS = (0.5, (0.5, 0.5, 0.5))
MASTER_ROUGHNESS = 0.85

WHITE_TEX_CANDIDATES = [
    "/Engine/EngineResources/WhiteSquareTexture",
    "/Engine/EngineResources/DefaultTexture",
]

VIS_PREVIEW_HLSL = """
float3 c = Day;
c = lerp(c, VisNvg.xxx, saturate(VisMode));
c = lerp(c, VisThermal.rgb, saturate(VisMode - 1.0));
return c;
"""


def log(msg):
    unreal.log("VIS_MATERIAL: " + msg)


def _names(name_list):
    return set(str(n) for n in name_list)


def material_has_vis(mat):
    """True when VisNvg (scalar) and VisThermal (vector) both resolve on a
    UMaterialInterface (material or instance)."""
    if mat is None:
        return False
    return (VIS_NVG_PARAM in _names(MEL.get_scalar_parameter_names(mat)) and
            VIS_THERMAL_PARAM in _names(MEL.get_vector_parameter_names(mat)))


# ------------------------------------------------------------------ MPC_Vision
def _mpc_has_vis_mode(mpc):
    for p in mpc.get_editor_property("scalar_parameters"):
        if str(p.get_editor_property("parameter_name")) == VIS_MODE_PARAM:
            return True
    return False


def ensure_vision_mpc(fresh=False):
    """Load /Game/Materials/MPC_Vision, creating it when missing (or always,
    with fresh=True - gen_assets.py's delete + recreate regime)."""
    if EAL.does_asset_exist(MPC_PATH):
        if not fresh:
            mpc = EAL.load_asset(MPC_PATH)
            if mpc is not None and _mpc_has_vis_mode(mpc):
                return mpc
            log("%s exists but lacks %s - rebuilding" % (MPC_PATH, VIS_MODE_PARAM))
        EAL.delete_asset(MPC_PATH)
    mpc = TOOLS.create_asset(MPC_NAME, MAT_DIR, unreal.MaterialParameterCollection,
                             unreal.MaterialParameterCollectionFactoryNew())
    if mpc is None:
        raise RuntimeError("create_asset failed for " + MPC_PATH)
    # FCollectionParameterBase's constructor assigns the Guid
    # (MaterialParameterCollection.h:27-30), so a struct built here is valid.
    p = unreal.CollectionScalarParameter()
    p.set_editor_property("parameter_name", VIS_MODE_PARAM)
    p.set_editor_property("default_value", 0.0)
    mpc.set_editor_property("scalar_parameters", [p])
    mpc.set_editor_property("vector_parameters", [])
    if not EAL.save_loaded_asset(mpc):
        raise RuntimeError("save failed for " + MPC_PATH)
    log("wrote " + MPC_PATH)
    return mpc


# ------------------------------------------------------------ graph helpers
def add_vis_params(mat, nvg, th, x=-900, y=800):
    """Create the VisNvg scalar + VisThermal vector parameter nodes on `mat`
    with the given defaults. Returns (nvg_expr, thermal_expr); they are
    only live once wrap_vis_preview() connects them."""
    n = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", VIS_NVG_PARAM)
    n.set_editor_property("default_value", float(nvg))
    t = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y + 150)
    t.set_editor_property("parameter_name", VIS_THERMAL_PARAM)
    t.set_editor_property("default_value", unreal.LinearColor(th[0], th[1], th[2], 1.0))
    return n, t


def wrap_vis_preview(mat, day_expr, day_output, nvg, th, mpc, x=-200, y=0):
    """Build the preview-wrap custom node fed by `day_expr`'s `day_output`
    pin, the Vis params (created here) and MPC_Vision.VisMode. Returns the
    custom node; the caller connects its output where the day colour went."""
    nvg_e, th_e = add_vis_params(mat, nvg, th, x - 700, y + 600)
    cp = MEL.create_material_expression(
        mat, unreal.MaterialExpressionCollectionParameter, x - 700, y + 900)
    # collection FIRST: PostEditChangeProperty resolves ParameterId from the
    # name only when Collection is already set (MaterialExpressions.cpp,
    # UMaterialExpressionCollectionParameter::PostEditChangeProperty).
    cp.set_editor_property("collection", mpc)
    cp.set_editor_property("parameter_name", VIS_MODE_PARAM)
    cn = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    cn.set_editor_property("code", VIS_PREVIEW_HLSL)
    cn.set_editor_property("description", "VisPreview")
    cn.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for name in ("Day", VIS_NVG_PARAM, VIS_THERMAL_PARAM, VIS_MODE_PARAM):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        ins.append(ci)
    cn.set_editor_property("inputs", ins)
    for src, pin, dst in ((day_expr, day_output, "Day"), (nvg_e, "", VIS_NVG_PARAM),
                          (th_e, "", VIS_THERMAL_PARAM), (cp, "", VIS_MODE_PARAM)):
        if not MEL.connect_material_expressions(src, pin, cn, dst):
            raise RuntimeError("%s: could not connect %s -> VisPreview.%s" % (
                mat.get_name(), src.get_name(), dst))
    return cn


def wrap_vis_property(mat, day_expr, day_output, prop, nvg, th, mpc, x=-200, y=0):
    """wrap_vis_preview() + connect the wrap to material property `prop`
    (the common BaseColor case)."""
    cn = wrap_vis_preview(mat, day_expr, day_output, nvg, th, mpc, x, y)
    if not MEL.connect_material_property(cn, "", prop):
        raise RuntimeError("%s: could not connect VisPreview -> %s" % (mat.get_name(), prop))
    return cn


# -------------------------------------------------------- M_ShitfireMaster
def _white_texture():
    for cand in WHITE_TEX_CANDIDATES:
        if EAL.does_asset_exist(cand):
            return EAL.load_asset(cand)
    raise RuntimeError("no engine white texture among %s" % WHITE_TEX_CANDIDATES)


def build_master(mpc):
    """Delete + recreate /Game/Materials/M_ShitfireMaster."""
    if EAL.does_asset_exist(MASTER_PATH):
        EAL.delete_asset(MASTER_PATH)
    mat = TOOLS.create_asset(MASTER_NAME, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        raise RuntimeError("create_asset failed for " + MASTER_PATH)
    # Usage flags baked in: skeletal units, HISM vegetation, Nanite meshes.
    # The editor would add them on first use interactively; cooked/-game
    # builds will not (gen_assets.py M_Placeholder note).
    mat.set_editor_property("used_with_skeletal_mesh", True)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    try:
        mat.set_editor_property("used_with_nanite", True)
    except Exception as e:  # noqa: BLE001
        log("note: used_with_nanite not settable (%s)" % e)
    ts = MEL.create_material_expression(
        mat, unreal.MaterialExpressionTextureSampleParameter2D, -1100, 0)
    ts.set_editor_property("parameter_name", "BaseTex")
    ts.set_editor_property("texture", _white_texture())
    tint = MEL.create_material_expression(
        mat, unreal.MaterialExpressionVectorParameter, -1100, 300)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -700, 100)
    MEL.connect_material_expressions(ts, "RGB", mul, "A")
    MEL.connect_material_expressions(tint, "", mul, "B")
    wrap_vis_property(mat, mul, "", unreal.MaterialProperty.MP_BASE_COLOR,
                      MASTER_VIS[0], MASTER_VIS[1], mpc)
    rough = MEL.create_material_expression(
        mat, unreal.MaterialExpressionScalarParameter, -1100, 500)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", MASTER_ROUGHNESS)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    if not EAL.save_loaded_asset(mat):
        raise RuntimeError("save failed for " + MASTER_PATH)
    if not material_has_vis(mat):
        raise RuntimeError(MASTER_PATH + " built without resolving Vis params")
    log("wrote " + MASTER_PATH)
    return mat


def ensure_master():
    """Load M_ShitfireMaster; rebuild it (and MPC_Vision if missing) when it
    is absent or predates the Vis convention."""
    mat = EAL.load_asset(MASTER_PATH) if EAL.does_asset_exist(MASTER_PATH) else None
    if mat is not None and material_has_vis(mat):
        return mat
    if mat is not None:
        log("%s lacks %s - rebuilding" % (MASTER_PATH, VIS_NVG_PARAM))
    return build_master(ensure_vision_mpc())


def new_master_instance(name, folder, master, base_tex, tint, nvg, th):
    """Fresh MaterialInstanceConstant `folder/name` of the master with the
    pack's texture/tint and the model's vis tag applied."""
    full = "%s/%s" % (folder, name)
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    mic = TOOLS.create_asset(name, folder, unreal.MaterialInstanceConstant,
                             unreal.MaterialInstanceConstantFactoryNew())
    if mic is None:
        raise RuntimeError("create_asset failed for " + full)
    MEL.set_material_instance_parent(mic, master)
    MEL.set_material_instance_vector_parameter_value(
        mic, "Tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    if base_tex is not None:
        MEL.set_material_instance_texture_parameter_value(mic, "BaseTex", base_tex)
    MEL.set_material_instance_scalar_parameter_value(mic, VIS_NVG_PARAM, float(nvg))
    MEL.set_material_instance_vector_parameter_value(
        mic, VIS_THERMAL_PARAM, unreal.LinearColor(th[0], th[1], th[2], 1.0))
    MEL.update_material_instance(mic)
    if not EAL.save_loaded_asset(mic):
        raise RuntimeError("save failed for " + full)
    return mic


def model_vis(model):
    """The manifest `vis` entry of a model as (nvg, (r, g, b)). Raises when
    absent or malformed: an untagged model is a manifest error, never a
    default."""
    v = model.get("vis")
    if not isinstance(v, dict) or "nvg" not in v or "th" not in v:
        raise RuntimeError("asset_manifest.json: model %s has no vis {nvg, th} entry"
                           % model.get("name"))
    th = v["th"]
    if len(th) != 3:
        raise RuntimeError("asset_manifest.json: model %s vis.th must be [r,g,b]"
                           % model.get("name"))
    return float(v["nvg"]), (float(th[0]), float(th[1]), float(th[2]))
