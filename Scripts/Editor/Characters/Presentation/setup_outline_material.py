"""Build only the local hover/selection outline material (no map or UI edits)."""
import unreal
E=unreal.EditorAssetLibrary
A=unreal.AssetToolsHelpers.get_asset_tools()
M=unreal.MaterialEditingLibrary
root='/Game/BoldersDemo'
# Same CustomDepth/Stencil mechanism as Aura; yellow exterior silhouette only.
mp=root+'/Assets/Shared/Materials/M_HoverOutline'
mat=E.load_asset(mp) if E.does_asset_exist(mp) else A.create_asset('M_HoverOutline',root+'/Assets/Shared/Materials',unreal.Material,unreal.MaterialFactoryNew())
mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_POST_PROCESS)
# Custom depth/stencil shares scene jitter. Composite before TSR/TAA so the
# silhouette is temporally resolved with the character, not over the final image.
mat.set_editor_property('blendable_location',unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_DOF)
M.delete_all_material_expressions(mat)
scene=M.create_material_expression(mat,unreal.MaterialExpressionSceneTexture,-600,0);scene.set_editor_property('scene_texture_id',unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
stencil=M.create_material_expression(mat,unreal.MaterialExpressionSceneTexture,-600,200);stencil.set_editor_property('scene_texture_id',unreal.SceneTextureId.PPI_CUSTOM_STENCIL)
custom=M.create_material_expression(mat,unreal.MaterialExpressionCustom,-200,0)
i1=unreal.CustomInput();i1.set_editor_property('input_name','Scene')
i2=unreal.CustomInput();i2.set_editor_property('input_name','Stencil')
custom.set_editor_property('inputs',[i1,i2])
custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
custom.set_editor_property('description','Hover yellow (200), selected enemy red (201)')
custom.set_editor_property('code','''
float2 uv = GetDefaultSceneTextureUV(Parameters, 25);
float2 pixel = View.BufferSizeAndInvSize.zw * 2.0;
float center = (abs(Stencil.r - 200.0) < 0.5 || abs(Stencil.r - 201.0) < 0.5) ? 1.0 : 0.0;
float hoverEdge = 0;
float selectedEdge = 0;
for (int x=-1;x<=1;x++) for(int y=-1;y<=1;y++)
{
 float2 q=uv+float2(x,y)*pixel;
 float value=SceneTextureLookup(q,25,false).r;
 float hoverMask=abs(value-200.0)<0.5 ? 1.0 : 0.0;
 float selectedMask=abs(value-201.0)<0.5 ? 1.0 : 0.0;
 float visible=SceneTextureLookup(q,13,false).r <= SceneTextureLookup(q,1,false).r+2.0 ? 1.0 : 0.0;
 hoverEdge=max(hoverEdge,hoverMask*visible);
 selectedEdge=max(selectedEdge,selectedMask*visible);
}
float3 color = selectedEdge > 0 ? float3(1.0,0.01,0.01) : float3(1.0,0.8,0.02);
return lerp(Scene.rgb,color,max(hoverEdge,selectedEdge)*(1-center));
''')
M.connect_material_expressions(scene,'Color',custom,'Scene');M.connect_material_expressions(stencil,'Color',custom,'Stencil');M.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);M.recompile_material(mat)
if not E.save_loaded_asset(mat,False):
 raise RuntimeError('Could not save outline material')
unreal.log('OUTLINE_MATERIAL_READY: hover=200 yellow, selection=201 red, BeforeDOF')
