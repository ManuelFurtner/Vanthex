"""Promote the user-approved, tested facility to the existing startup map."""
import unreal as u,json,hashlib,os
ROOT='H:/Vanthex';OUT=ROOT+'/Saved/GraphicsLab/Facility'
candidate='/Game/Vanthex/Experiments/GraphicsLab/Maps/FacilityCandidate_GridV4'
production='/Game/Vanthex/Maps/GridV4'
source=ROOT+'/Content/Vanthex/Maps/GridV4.umap'
backup=ROOT+'/Saved/GraphicsLab/ProductionBackup_20261104/GridV4.umap'
digest=lambda p:hashlib.sha256(open(p,'rb').read()).hexdigest()
original=json.load(open(OUT+'/generation.json'))['original_sha256']
assert digest(source)==original,'Production changed since backup; refusing overwrite'
assert digest(backup)==original,'Backup missing or inconsistent'
verification=json.load(open(OUT+'/verification_candidate.json'))
assert verification['components_equal'] and verification['queries_equal']
assert not verification['experiment_asset_references']
assert open(OUT+'/gameplay.txt',encoding='utf-8-sig').readline().strip()=='OVERALL PASS'
assert open(OUT+'/status.txt',encoding='utf-8-sig').read().strip()=='GAMEPLAY AND 22 SCREENSHOTS COMPLETE'
# The caller must inspect the saved screenshots before running this explicit step.
assert os.path.exists(OUT+'/visual_review_approved.txt'),'Visual review has not been recorded'
candidate_file=ROOT+'/Content/Vanthex/Experiments/GraphicsLab/Maps/FacilityCandidate_GridV4.umap'
assert open(OUT+'/visual_review_approved.txt',encoding='utf-8-sig').read().strip()==digest(candidate_file),'Candidate changed after visual review'
assert u.get_editor_subsystem(u.LevelEditorSubsystem).load_level(candidate)
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert u.EditorLoadingAndSavingUtils.save_map(world,production)
assert digest(source)!=original
with open(OUT+'/promotion.json','w') as f:json.dump({'production':production,'backup':backup,'original_sha256':original,'new_sha256':digest(source),'candidate':candidate},f,indent=2)
u.log('FACILITY PROMOTION COMPLETE')
