"""Regenerates the unit-presentation materials under /Game/Units
(BUILDER-CHARACTERS, P4).

Same regime as gen_assets.py / gen_fx_assets.py: the .uassets are build
products, THIS FILE is the source of truth (delete + recreate every run);
every asset is on port/unreal/uasset-allowlist.txt. Consumed by
Source/ShitfireGame/*/units/ShitfireUnitsActor.cpp - parameter names are the
contract:

  M_UnitBody  lit opaque tint material for humanoid recolors (friendly navy,
              civilian bone - 05-units-scenario.js:49 / 04-world.js:70) and
              the dead-vehicle blacken swap (05:19 vehMatDead). Carries the
              skeletal-mesh usage flag, which the terrain/placeholder
              materials lack - THE reason this material exists.
              Params: Tint (vector), BodyRoughness (scalar).

Run headless:
  UnrealEditor-Cmd.exe <Shitfire.uproject> -run=pythonscript ^
      -script="port/unreal/Tools/gen_unit_assets.py" -unattended -nopause
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
PKG_PATH = "/Game/Units"


def fresh_material(name):
    full = "{}/{}".format(PKG_PATH, name)
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    mat = TOOLS.create_asset(name, PKG_PATH, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        raise RuntimeError("create_asset failed for " + full)
    return mat


def finish(mat):
    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    if not EAL.save_loaded_asset(mat):
        raise RuntimeError("save failed for " + mat.get_name())
    unreal.log("gen_unit_assets: wrote " + mat.get_path_name())


def unit_body():
    mat = fresh_material("M_UnitBody")
    # Skeletal usage is the point (see docstring). Static usage comes free.
    mat.set_editor_property("used_with_skeletal_mesh", True)
    tint = MEL.create_material_expression(
        mat, unreal.MaterialExpressionVectorParameter, -600, 0)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    MEL.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = MEL.create_material_expression(
        mat, unreal.MaterialExpressionScalarParameter, -600, 250)
    rough.set_editor_property("parameter_name", "BodyRoughness")
    rough.set_editor_property("default_value", 0.9)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(mat)


unit_body()
unreal.log("gen_unit_assets: done")
