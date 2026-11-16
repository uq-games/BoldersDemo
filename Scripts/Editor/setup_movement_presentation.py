import unreal
E=unreal.EditorAssetLibrary
A=unreal.AssetToolsHelpers.get_asset_tools()
M=unreal.MaterialEditingLibrary
root='/Game/BoldersDemo'
# Generated art, original source retained for reimport.
t=unreal.AssetImportTask(); t.filename='H:/UE5/Unreal Projects/BoldersDemo/Content/BoldersDemo/Assets/UI/PlayerMarkers/Source/PlayerMarkers.png';t.destination_path=root+'/Assets/UI/PlayerMarkers';t.destination_name='T_PlayerMarkers';t.automated=True;t.save=True;t.replace_existing=True
A.import_asset_tasks([t])
tex=E.load_asset(root+'/Assets/UI/PlayerMarkers/T_PlayerMarkers'); tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON);tex.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI);E.save_loaded_asset(tex)
font=E.load_asset(root+'/Assets/UI/Fonts/Source/Cinzel-Variable')
# A Blueprint HUD keeps presentation assets editable without hard-coded runtime loads.
p=root+'/Blueprints/UI/BP_BoldersHUD'
if not E.does_asset_exist(p):
 f=unreal.BlueprintFactory();f.set_editor_property('parent_class',unreal.load_class(None,'/Script/BoldersDemo.BoldersHUD'));A.create_asset('BP_BoldersHUD',root+'/Blueprints/UI',None,f)
hud=unreal.get_default_object(E.load_blueprint_class(p));hud.set_editor_property('MarkerAtlas',tex);unreal.BlueprintEditorLibrary.compile_blueprint(E.load_asset(p));E.save_asset(p,False)
gmp=root+'/Blueprints/Shared/BP_BoldersGameMode';gm=unreal.get_default_object(E.load_blueprint_class(gmp));gm.set_editor_property('hud_class',E.load_blueprint_class(p));unreal.BlueprintEditorLibrary.compile_blueprint(E.load_asset(gmp));E.save_asset(gmp,False)
pcp=root+'/Blueprints/Player/BP_BoldersPlayerController';pc=unreal.get_default_object(E.load_blueprint_class(pcp));pc.set_editor_property('MoveAcceptedFX',E.load_asset('/Game/TopDown/Cursor/FX_Cursor_Success'));pc.set_editor_property('MoveRejectedFX',E.load_asset('/Game/TopDown/Cursor/FX_Cursor_Failure'));unreal.BlueprintEditorLibrary.compile_blueprint(E.load_asset(pcp));E.save_asset(pcp,False)
# Same CustomDepth/Stencil mechanism as Aura; yellow exterior silhouette only.
mp=root+'/Assets/Shared/Materials/M_HoverOutline'
mat=E.load_asset(mp) if E.does_asset_exist(mp) else A.create_asset('M_HoverOutline',root+'/Assets/Shared/Materials',unreal.Material,unreal.MaterialFactoryNew())
mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_POST_PROCESS)
mat.set_editor_property('blendable_location',unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
M.delete_all_material_expressions(mat)
scene=M.create_material_expression(mat,unreal.MaterialExpressionSceneTexture,-600,0);scene.set_editor_property('scene_texture_id',unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
stencil=M.create_material_expression(mat,unreal.MaterialExpressionSceneTexture,-600,200);stencil.set_editor_property('scene_texture_id',unreal.SceneTextureId.PPI_CUSTOM_STENCIL)
custom=M.create_material_expression(mat,unreal.MaterialExpressionCustom,-200,0)
i1=unreal.CustomInput();i1.set_editor_property('input_name','Scene')
i2=unreal.CustomInput();i2.set_editor_property('input_name','Stencil')
custom.set_editor_property('inputs',[i1,i2])
custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
custom.set_editor_property('description','Yellow local hover silhouette, stencil 200')
custom.set_editor_property('code','''
float2 uv = GetDefaultSceneTextureUV(Parameters, 25);
float2 pixel = View.BufferSizeAndInvSize.zw * 2.0;
float center = abs(Stencil.r - 200.0) < 0.5 ? 1.0 : 0.0;
float edge = 0;
for (int x=-1;x<=1;x++) for(int y=-1;y<=1;y++)
{
 float2 q=uv+float2(x,y)*pixel;
 float mask=abs(SceneTextureLookup(q,25,false).r-200.0)<0.5 ? 1.0 : 0.0;
 float visible=SceneTextureLookup(q,13,false).r <= SceneTextureLookup(q,1,false).r+2.0 ? 1.0 : 0.0;
 edge=max(edge,mask*visible);
}
return lerp(Scene.rgb,float3(1.0,0.8,0.02),edge*(1-center));
''')
M.connect_material_expressions(scene,'Color',custom,'Scene');M.connect_material_expressions(stencil,'Color',custom,'Stencil');M.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);M.recompile_material(mat);E.save_loaded_asset(mat,False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world,'r.CustomDepth 3')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
vol=next((a for a in actors.get_all_level_actors() if a.get_actor_label()=='HoverOutline'),None)
if not vol: vol=actors.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector());vol.set_actor_label('HoverOutline')
vol.set_editor_property('unbound',True)
settings=vol.get_editor_property('settings');settings.set_editor_property('weighted_blendables',unreal.WeightedBlendables(array=[unreal.WeightedBlendable(weight=1.0,object=mat)]));vol.set_editor_property('settings',settings)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('PRESENTATION ASSETS CONFIGURED')
