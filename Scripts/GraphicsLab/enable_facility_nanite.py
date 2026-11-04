"""Run with full UnrealEditor -ExecutePythonScript (the subsystem needs an editor)."""
import unreal as u,json
editor=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
assert editor,'Use UnrealEditor -ExecutePythonScript, not the Python commandlet'
results=[]
for name in ['SM_LabWallCassette','SM_LabCeilingCassette','SM_LabPilaster','SM_LabCableTray','SM_LabPowerCabinet']:
    mesh=u.load_asset('/Game/Vanthex/Art/IndustrialFinish/Meshes/'+name);assert mesh
    settings=editor.get_nanite_settings(mesh)
    settings.set_editor_property('enabled',True)
    editor.set_nanite_settings(mesh,settings,True)
    assert u.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
    results.append({'mesh':name,'nanite_enabled':editor.get_nanite_settings(mesh).enabled})
with open('H:/Vanthex/Saved/GraphicsLab/Facility/nanite.json','w') as f:json.dump(results,f,indent=2)
u.log('FACILITY NANITE COMPLETE')
