"""One-shot diagnostic: print the soldier skeletal mesh's imported bounds and
first-bone directions so BUILDER-CHARACTERS can pick the upright-fix rotation
from data instead of screenshot roulette. Read-only; writes nothing."""
import unreal

MESH = "/Game/Models/Soldier/SM_Soldier/SkeletalMeshes/Body_1b3e0aad2706b303962c73fef1dbefd3"
sk = unreal.EditorAssetLibrary.load_asset(MESH)
b = sk.get_bounds()
ext = b.box_extent
org = b.origin
unreal.log("SOLDIERBOUNDS extent=({:.1f},{:.1f},{:.1f}) origin=({:.1f},{:.1f},{:.1f})".format(
    ext.x, ext.y, ext.z, org.x, org.y, org.z))

anims = [
    "/Game/Models/Soldier/SM_Soldier/SkeletalMeshes/SM_SoldierCharacterArmature_Idle",
    "/Game/Models/Soldier/SM_Soldier/SkeletalMeshes/SM_SoldierCharacterArmature_Death",
    "/Game/Models/Soldier/SM_Soldier/SkeletalMeshes/SM_SoldierCharacterArmature_Run",
]
for p in anims:
    a = unreal.EditorAssetLibrary.load_asset(p)
    if a is None:
        unreal.log("SOLDIERANIM missing " + p)
    else:
        unreal.log("SOLDIERANIM {} class={} len={:.2f}".format(
            a.get_name(), a.get_class().get_name(),
            a.get_editor_property("sequence_length")
            if hasattr(a, "get_editor_property") else -1))
