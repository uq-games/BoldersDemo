"""Configure local preview data and server-approved classes. Widget assets are created via MCP first."""
import unreal
from pathlib import Path
import shutil
import runpy

E=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
root='/Game/BoldersDemo'
folder=root+'/Assets/UI/ClassSelection'
bp_folder=root+'/Blueprints/UI/ClassSelection'

def save(a):
 if isinstance(a,unreal.Blueprint): unreal.BlueprintEditorLibrary.compile_blueprint(a)
 assert E.save_loaded_asset(a,False)

def data(name,cls):
 a=unreal.load_asset(folder+'/'+name)
 if not a:
  f=unreal.DataAssetFactory();f.set_editor_property('data_asset_class',cls)
  a=tools.create_asset(name,folder,cls,f)
 assert a
 return a

textures={}
for name in ['Background','CardFrame','ConfirmButton']:
 path=folder+'/T_Selection'+name
 asset=unreal.load_asset(path)
 if not asset:
  task=unreal.AssetImportTask()
  task.filename=str(Path(unreal.Paths.project_dir())/'ArtSources/UI/ClassSelection'/ (name+'.png'))
  task.destination_path=folder;task.destination_name='T_Selection'+name
  task.automated=True;task.save=True
  tools.import_asset_tasks([task]);asset=unreal.load_asset(path)
 assert asset
 asset.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
 asset.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
 asset.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
 save(asset);textures[name]=asset

fallback_path=folder+'/T_PortraitFallback'
if not E.does_asset_exist(fallback_path): assert E.duplicate_asset('/Engine/EngineResources/WhiteSquareTexture',fallback_path)
fallback=unreal.load_asset(fallback_path);fallback.set_editor_property('srgb',False);save(fallback)
material=unreal.load_asset(folder+'/M_CharacterPortrait')
if not material:
 material=tools.create_asset('M_CharacterPortrait',folder,unreal.Material,unreal.MaterialFactoryNew())
 material.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
 material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
 tex=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionTextureSampleParameter2D,-500,0)
 tex.set_editor_property('parameter_name','Portrait')
 tex.set_editor_property('texture',fallback)
 tex.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
 inverse=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionOneMinus,-200,150)
 unreal.MaterialEditingLibrary.connect_material_expressions(tex,'A',inverse,'Input')
 unreal.MaterialEditingLibrary.connect_material_property(tex,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 unreal.MaterialEditingLibrary.connect_material_property(inverse,'',unreal.MaterialProperty.MP_OPACITY)
 unreal.MaterialEditingLibrary.recompile_material(material);save(material)

preview=unreal.load_asset(bp_folder+'/BP_CharacterPreview')
if not preview:
 f=unreal.BlueprintFactory();f.set_editor_property('parent_class',unreal.CharacterPreview)
 preview=tools.create_asset('BP_CharacterPreview',bp_folder,unreal.Blueprint,f);save(preview)
preview_default=unreal.get_default_object(preview.generated_class())
preview_default.get_editor_property('capture').set_editor_property('fov_angle',29.)
save(preview)

shared='/Game/KayKit_Adventurers/Shared/Animations/Medium/'
configs=[
 ('Warrior','战士','近战 · 剑盾','靠近敌人，挥剑攻击',
  'General/A_Warrior_GeneralIdle_A',
  'CombatMelee/A_Warrior_CombatMelee_Melee_Block',
  shared+'Simulation/A_Medium_Simulation_Cheering',
  'SM_sword_1handed','Weapon_R','SM_shield_badge_color','Shield_L'),
 ('Archer','弓箭手','远程 · 弓箭','拉弓射击，保持距离',
  'CombatRanged/A_Archer_CombatRanged_Ranged_Bow_Idle',
  'CombatRanged/A_Archer_CombatRanged_Ranged_Bow_Draw',
  shared+'Simulation/A_Medium_Simulation_Waving',
  'SM_bow','Bow_L',None,None),
 ('Mage','法师','远程 · 魔法','施放魔法弹，远程攻击',
  'General/A_Mage_GeneralIdle_A',
  'CombatRanged/A_Mage_CombatRanged_Ranged_Magic_Spellcasting',
  'CombatRanged/A_Mage_CombatRanged_Ranged_Magic_Summon',
  'SM_staff','Staff_R',None,None),
]
definitions=[]
for role,title,subtitle,description,idle,hover,selected,weapon,socket,offhand,offsocket in configs:
 base=root+'/Assets/Characters/'+role
 d=data('DA_Class_'+role,unreal.ClassSelectionData)
 d.set_editor_property('class_id',role)
 d.set_editor_property('display_name',title);d.set_editor_property('role_text',subtitle);d.set_editor_property('description',description)
 d.set_editor_property('character_class',E.load_blueprint_class(root+'/Blueprints/Characters/'+role+'/BP_'+role))
 d.set_editor_property('preview_mesh',unreal.load_asset(base+'/SK_'+role))
 for key,source in [('idle',idle),('hover',hover),('selected',selected)]:
  source=source if source.startswith('/Game/') else base+'/Animations/'+source
  target=base+'/Animations/Preview/A_Preview_'+role+'_'+key.title()
  if not E.does_asset_exist(target): assert E.duplicate_asset(source,target)
  anim=unreal.load_asset(target)
  assert anim and anim.get_editor_property('skeleton')==d.get_editor_property('preview_mesh').get_editor_property('skeleton')
  d.set_editor_property(key+'_animation',anim)
  save(anim)
  print('PREVIEW_ANIMATION',role,key,source,anim.get_play_length())
 d.set_editor_property('weapon_mesh',unreal.load_asset(base+'/Accessories/'+weapon));d.set_editor_property('weapon_socket',socket)
 if offhand:
  d.set_editor_property('offhand_mesh',unreal.load_asset(base+'/Accessories/'+offhand));d.set_editor_property('offhand_socket',offsocket)
 d.set_editor_property('pedestal_mesh',unreal.load_asset('/Game/Inferno_World_Free/Meshes/Rocks/SM_Stone_001'))
 d.set_editor_property('pedestal_scale',unreal.Vector(.33,.27,.35))
 d.set_editor_property('animate_bow_string',role=='Archer')
 save(d);definitions.append(d)

unreal.SystemLibrary.execute_console_command(None,'Bolders.BuildClassSelection')
catalog=data('DA_ClassSelectionCatalog',unreal.ClassSelectionCatalog)
catalog.set_editor_property('classes',definitions)
catalog.set_editor_property('preview_actor_class',preview.generated_class())
catalog.set_editor_property('portrait_material',material)
catalog.set_editor_property('background',textures['Background'])
catalog.set_editor_property('card_frame',textures['CardFrame'])
catalog.set_editor_property('confirm_plate',textures['ConfirmButton'])
save(catalog)

for rel in ['Blueprints/Shared/BP_BoldersGameMode','Blueprints/Player/BP_BoldersPlayerController']:
 backup=Path(unreal.Paths.project_saved_dir())/'ClassSelection/Before'/(rel.split('/')[-1]+'.uasset')
 backup.parent.mkdir(parents=True,exist_ok=True)
 if not backup.exists():shutil.copy2(Path(unreal.Paths.project_dir())/'Content/BoldersDemo'/(rel+'.uasset'),backup)
 bp=unreal.load_asset(root+'/'+rel);c=unreal.get_default_object(bp.generated_class())
 c.set_editor_property('selection_catalog',catalog)
 if 'PlayerController' in rel:
  c.set_editor_property('selection_widget_class',E.load_blueprint_class(bp_folder+'/WBP_ClassSelection'))
 save(bp)
print('CLASS_SELECTION_CONFIGURED')

# Apply the current artwork after rebuilding the shared class data and animation blueprints.
style_script = Path(unreal.Paths.project_dir()) / 'Scripts/Editor/UI/ClassSelection/update_class_selection_style.py'
runpy.run_path(str(style_script.resolve()), run_name="__main__")
