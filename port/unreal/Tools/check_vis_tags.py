"""ART-a gate: every UMaterialInterface under /Game must carry the Vis tag,
and every StaticMesh/SkeletalMesh slot under /Game must hold one.

Lists /Game recursively (EditorAssetLibrary.list_assets), loads each asset
and reports, per material, whether `VisNvg` (scalar) and `VisThermal`
(vector) resolve (MaterialEditingLibrary.get_scalar_parameter_names /
get_vector_parameter_names, which walk instance -> parent) and their
values; per mesh slot, whether the material is None or untagged. Raises at
the end with the offender list - an untagged material fails here, never
falls back to a luminance guess (PORT_PLAN.md section ART, ART-a gate).

Run headless after the gen_*/import_assets.py chain:
  UnrealEditor-Cmd.exe <Shitfire.uproject> -run=pythonscript ^
      -script="port/unreal/Tools/check_vis_tags.py" -unattended -nopause
Exit: the log ends with "CHECK_VIS_TAGS: OK ..." or a RuntimeError.
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
ROOT = "/Game"


def log(msg):
    unreal.log("CHECK_VIS_TAGS: " + msg)


def vis_values(mat):
    """(nvg, (r,g,b)) as the material resolves them; instance override or
    parent default."""
    if isinstance(mat, unreal.MaterialInstanceConstant):
        n = MEL.get_material_instance_scalar_parameter_value(mat, vm.VIS_NVG_PARAM)
        t = MEL.get_material_instance_vector_parameter_value(mat, vm.VIS_THERMAL_PARAM)
    elif isinstance(mat, unreal.Material):
        n = MEL.get_material_default_scalar_parameter_value(mat, vm.VIS_NVG_PARAM)
        t = MEL.get_material_default_vector_parameter_value(mat, vm.VIS_THERMAL_PARAM)
    else:
        return None, None
    return n, (t.r, t.g, t.b)


def main():
    paths = EAL.list_assets(ROOT, recursive=True, include_folder=False)
    materials = {}   # object path -> (asset, tagged)
    meshes = []      # (object path, asset)
    for op in paths:
        a = EAL.load_asset(op)
        if a is None:
            raise RuntimeError("could not load " + op)
        if isinstance(a, unreal.MaterialInterface):
            materials[op] = (a, vm.material_has_vis(a))
        elif isinstance(a, (unreal.StaticMesh, unreal.SkeletalMesh)):
            meshes.append((op, a))

    untagged = []
    for op in sorted(materials):
        a, ok = materials[op]
        kind = a.get_class().get_name()
        if ok:
            n, t = vis_values(a)
            log("material %-70s %-24s VisNvg=%.3f VisThermal=(%.3f, %.3f, %.3f)" % (
                op, kind, n, t[0], t[1], t[2]))
        else:
            log("material %-70s %-24s UNTAGGED" % (op, kind))
            untagged.append(op)

    bad_slots = []
    n_slots = 0
    for op, a in meshes:
        if isinstance(a, unreal.StaticMesh):
            slots = [(str(s.get_editor_property("material_slot_name")),
                      s.get_editor_property("material_interface"))
                     for s in a.get_editor_property("static_materials")]
        else:
            slots = [(str(s.get_editor_property("material_slot_name")),
                      s.get_editor_property("material_interface"))
                     for s in a.get_editor_property("materials")]
        for i, (slot_name, mi) in enumerate(slots):
            n_slots += 1
            if mi is None:
                bad_slots.append("%s slot %d (%s): None" % (op, i, slot_name))
            elif not vm.material_has_vis(mi):
                bad_slots.append("%s slot %d (%s): %s untagged" % (
                    op, i, slot_name, mi.get_path_name()))
        log("mesh %-72s %d slot(s)" % (op, len(slots)))

    log("%d materials (%d tagged), %d meshes, %d slots" % (
        len(materials), len(materials) - len(untagged), len(meshes), n_slots))
    if untagged or bad_slots:
        for u in untagged:
            log("FAIL untagged material: " + u)
        for b in bad_slots:
            log("FAIL mesh slot: " + b)
        raise RuntimeError("check_vis_tags: %d untagged material(s), %d bad mesh slot(s)" % (
            len(untagged), len(bad_slots)))
    log("OK - every material under %s carries VisNvg/VisThermal and every mesh slot is tagged" % ROOT)


main()
