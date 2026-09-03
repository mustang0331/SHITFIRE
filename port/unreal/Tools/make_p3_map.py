"""Regenerates Content/Maps/P3World.umap - the near-empty P3 world map.

The map holds NO actors and NO logic: AShitfireGameMode + AShitfireWorldActor
spawn everything on BeginPlay (no-Blueprint-logic / near-empty-map rule,
UNREAL_RESEARCH.md 8). Classic level, NOT world-partitioned, so no
__ExternalActors__ spray and the uasset-allowlist stays one line.

Run headless:
  UnrealEditor-Cmd.exe <Shitfire.uproject> -run=pythonscript ^
      -script="port/unreal/Tools/make_p3_map.py" -unattended -nopause
"""
import unreal

ASSET_PATH = "/Game/Maps/P3World"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
    unreal.log("make_p3_map: {} already exists - leaving it alone".format(ASSET_PATH))
else:
    ok = les.new_level(ASSET_PATH, False)  # False = classic level, no World Partition
    if not ok:
        raise RuntimeError("new_level failed for " + ASSET_PATH)
    if not les.save_current_level():
        raise RuntimeError("save_current_level failed for " + ASSET_PATH)
    unreal.log("make_p3_map: created " + ASSET_PATH)
