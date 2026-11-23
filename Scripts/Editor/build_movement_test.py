"""Build the movement test course using Unreal Geometry Script; run inside the editor."""
import unreal
E=unreal.EditorAssetLibrary
SS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
WORLD=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
ROOT='/Game/BoldersDemo/Assets/Environment/MovementTest'
P=unreal.GeometryScript_Primitives
OPT=unreal.GeometryScriptPrimitiveOptions()

def material(name,color):
 path=ROOT+'/Materials/'+name
 if E.does_asset_exist(path): return E.load_asset(path)
 m=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
 n=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant3Vector,-200,0)
 n.set_editor_property('constant',unreal.LinearColor(*color,1))
 unreal.MaterialEditingLibrary.connect_material_property(n,'',unreal.MaterialProperty.MP_BASE_COLOR)
 r=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant,-200,160)
 r.set_editor_property('r',.8)
 unreal.MaterialEditingLibrary.connect_material_property(r,'',unreal.MaterialProperty.MP_ROUGHNESS)
 unreal.MaterialEditingLibrary.recompile_material(m); E.save_loaded_asset(m)
 return m
MATS={'ramp':material('M_TestRamp',(.08,.4,.35)), 'stairs':material('M_TestStairs',(.14,.32,.65)), 'high':material('M_TestHighSteps',(.65,.16,.06)), 'pillar':material('M_TestPillar',(.35,.25,.1))}

def mesh(name,build,mat):
 path=ROOT+'/Meshes/'+name
 if E.does_asset_exist(path): return E.load_asset(path)
 dm=unreal.DynamicMesh()
 build(dm)
 opts=unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
 opts.set_editor_property('enable_collision',True)
 opts.set_editor_property('collision_mode',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 opts.set_editor_property('enable_recompute_normals',True)
 opts.set_editor_property('enable_recompute_tangents',True)
 result=unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm,path,opts)
 sm=result[0]
 assert sm, (name,result)
 sm.set_material(0,mat); E.save_loaded_asset(sm)
 return sm

def box(dm,x,y,z): P.append_box(dm,OPT,unreal.Transform(),x,y,z)
ramp=mesh('SM_TestRamp_600x300x200',lambda d:P.append_simple_extrude_polygon(d,OPT,unreal.Transform(rotation=unreal.Rotator(roll=-90)),[unreal.Vector2D(0,0),unreal.Vector2D(600,0),unreal.Vector2D(600,200)],300),MATS['ramp'])
stairs=mesh('SM_TestStairs_8x20',lambda d:P.append_linear_stairs(d,OPT,unreal.Transform(),300,20,70,8,False),MATS['stairs'])
high=mesh('SM_TestSteps_4x65',lambda d:P.append_linear_stairs(d,OPT,unreal.Transform(),300,65,100,4,False),MATS['high'])
pillar=mesh('SM_TestPillar',lambda d:P.append_cylinder(d,OPT,unreal.Transform(),70,400,24),MATS['pillar'])
landing_ramp=mesh('SM_TestLanding_200',lambda d:box(d,400,300,200),MATS['ramp'])
landing_stairs=mesh('SM_TestLanding_160',lambda d:box(d,400,300,160),MATS['stairs'])
wall=mesh('SM_TestObstacle',lambda d:box(d,100,600,200),MATS['pillar'])

def put(label,sm,minx,centery,walkable=False):
 a=next((a for a in SS.get_all_level_actors() if a.get_actor_label()==label),None)
 if not a: a=SS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector()); a.set_actor_label(label)
 b=sm.get_bounds(); origin=b.origin; ext=b.box_extent
 a.static_mesh_component.set_static_mesh(sm)
 a.set_actor_scale3d(unreal.Vector(1,1,1))
 a.set_actor_location(unreal.Vector(minx-(origin.x-ext.x),centery-origin.y,-(origin.z-ext.z)),False,False)
 a.static_mesh_component.set_collision_profile_name('BlockAll')
 a.set_folder_path('MovementTest')
 a.set_editor_property('tags',[unreal.Name('MovementTest')]+([unreal.Name('WalkableGround')] if walkable else []))
 print(label,a.get_actor_location(),a.get_actor_bounds(False))
 return a
put('MovementTest_Ramp',ramp,-600,-1000,True)
put('MovementTest_RampLanding',landing_ramp,0,-1000,True)
put('MovementTest_Stairs',stairs,-600,850,True)
put('MovementTest_StairsLanding',landing_stairs,-40,850,True)
put('MovementTest_HighSteps',high,-600,1450,True)
for i,(x,y) in enumerate([(750,-550),(950,-150),(750,300),(1150,550),(1350,100)]):
 put('MovementTest_Pillar_'+str(i+1),pillar,x,y)
# Replace the previous engine-cube test obstacle with a project-owned mesh.
put('WarriorDemo_Obstacle',wall,450,0)
# Remove only the empty navigation volume created by the earlier setup.
for a in SS.get_all_level_actors():
 if a.get_actor_label()=='WarriorDemo_NavMeshBounds' and a.get_actor_bounds(False)[1].length()<1: SS.destroy_actor(a)
nav=next((a for a in SS.get_all_level_actors() if a.get_actor_label()=='MovementTest_Navigation'),None)
if not nav:
 nav=SS.spawn_actor_from_class(unreal.NavMeshBoundsVolume,unreal.Vector(0,0,200))
 nav.set_actor_label('MovementTest_Navigation')
nav.set_actor_scale3d(unreal.Vector(19.5,19.5,5))
nav.set_folder_path('MovementTest')
# Navigation rebuild is deferred until the editor has finished loading.
unreal.SystemLibrary.execute_console_command(WORLD,'RebuildNavigation')
unreal.EditorLoadingAndSavingUtils.save_current_level()
print('MovementTest: course created')

