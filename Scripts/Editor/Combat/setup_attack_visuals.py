"""Author class-specific combat materials and bind the playable Blueprint defaults."""
import unreal as u

E = u.EditorAssetLibrary
M = u.MaterialEditingLibrary
root = "/Game/BoldersDemo"
folder = root + "/Assets/Combat/Effects"
E.make_directory(folder)
palette = {"Warrior": "float3(1,.52,.12)", "Archer": "float3(.22,.94,.54)",
           "Mage": "float3(.45,.19,1)"}

def material(name, shader, timed=False):
    path = folder + "/" + name
    mat = E.load_asset(path) if E.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, u.Material, u.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", u.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("shading_model", u.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    M.delete_all_material_expressions(mat)
    node = M.create_material_expression(mat, u.MaterialExpressionCustom)
    nodes = {"UV": M.create_material_expression(mat, u.MaterialExpressionTextureCoordinate, -400, 0),
             "Clock": M.create_material_expression(mat, u.MaterialExpressionTime, -400, 160)}
    if timed:
        start = M.create_material_expression(mat, u.MaterialExpressionScalarParameter, -400, 320)
        start.set_editor_property("parameter_name", "StartTime")
        nodes["Start"] = start
    inputs = []
    for key in nodes:
        item = u.CustomInput()
        item.set_editor_property("input_name", key)
        inputs.append(item)
    node.set_editor_property("inputs", inputs)
    node.set_editor_property("output_type", u.CustomMaterialOutputType.CMOT_FLOAT3)
    node.set_editor_property("code", shader)
    for key, source in nodes.items():
        assert M.connect_material_expressions(source, "", node, key)
    assert M.connect_material_property(node, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
    M.recompile_material(mat)
    assert E.save_loaded_asset(mat, False)
    return mat

slash = material("M_SwordArc", """
float t=saturate((Clock-Start)/.34);
float2 p=(UV-.5)*2; float r=length(p);
float a=atan2(p.y,p.x);
float head=lerp(-1.45,1.85,saturate(.25+t*1.5));
float w=saturate(1-(head-a)/2.1)*smoothstep(0,.12,head-a);
float band=exp(-abs(r-.77)*20)+.42*exp(-abs(r-.68)*61);
return float3(8.0,3.0,.65)*band*w*(1-t);
""", True)
ring_mat = E.load_asset(root + "/Assets/Shared/Materials/M_FactionRing")
sphere = E.load_asset("/Engine/BasicShapes/Sphere")
for role, tint in palette.items():
    hit = material("M_Hit" + role, """
float t=saturate((Clock-Start)/.36);
float2 p=(UV-.5)*2; float r=length(p); float a=atan2(p.y,p.x);
float wave=exp(-abs(r-(.24+.54*t))*28);
float spikes=pow(abs(cos(5*a+.35*sin(3*a))),14)*exp(-abs(r-(.12+.45*t))*6);
float core=exp(-r*5)*(1-t);
float shard=pow(saturate(cos(7*a+1.2)),18)*exp(-abs(r-(.2+.63*t))*11);
return (TINT*(wave*2.2+spikes*4.5+shard*2.4)+float3(9,8,6)*core)*(1-t);
""".replace("TINT", tint), True)
    bp = E.load_asset(root + "/Blueprints/Characters/" + role + "/BP_" + role)
    cdo = u.get_default_object(bp.generated_class())
    cdo.set_editor_property("impact_material", hit)
    if role == "Warrior":
        cdo.set_editor_property("swing_material", slash)
    else:
        ring = cdo.get_editor_property("faction_ring")
        ring.set_editor_property("decal_material", ring_mat)
        ring.set_editor_property("radius", 76.)
        ring.set_editor_property("player_color", u.LinearColor(.01,.08,1,1))
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assert E.save_loaded_asset(bp, False)
    if role == "Warrior":
        continue
    mage = role == "Mage"
    bolt = "Magic" if mage else "Arrow"
    trail = material("M_" + bolt + "Wake", """
float flow=.78+.22*cos(18*UV.x+25*Clock+9*UV.y);
return TINT*(1.1+2.5*pow(saturate(sin(UV.y*3.14159)),4))*flow;
""".replace("TINT", tint))
    bp = E.load_asset(root + "/Blueprints/Combat/Projectiles/BP_" + bolt + "Projectile")
    cdo = u.get_default_object(bp.generated_class())
    glow = cdo.get_editor_property("flight_glow")
    glow.set_static_mesh(sphere)
    glow.set_material(0, trail)
    glow.set_relative_scale3d(u.Vector(.78,.13,.13) if mage else u.Vector(.66,.085,.085))
    glow.set_relative_location(u.Vector(-29 if mage else -51,0,0),False,False)
    if mage:
        orb = material("M_ArcaneOrb", """
float phase=UV.x*18.85+sin(UV.y*10-Clock*7)*2.6+Clock*8;
float veins=pow(.5+.5*cos(phase),7);
return float3(.33,.08,1.35)+float3(2,3.1,4.4)*veins;
""")
        mesh = cdo.get_editor_property("mesh")
        mesh.set_relative_scale3d(u.Vector(.32,.30,.30))
        mesh.set_material(0, orb)
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assert E.save_loaded_asset(bp, False)
u.log("ATTACK_VISUALS_CONFIGURED")

# Pick the broad ornamental end of the staff from its outer vertex rings.
# This works in imported mesh space irrespective of the staff's local long axis.
for role in ("Archer", "Mage"):
    bp = E.load_asset(root + "/Blueprints/Combat/Weapons/BP_" + role + "Weapon")
    weapon = u.get_default_object(bp.generated_class())
    release = u.Vector(0,0,0)  # Bow grip, where the shaft crosses the bow.
    if role == "Mage":
        staff = weapon.get_editor_property("mesh").static_mesh
        lod = staff.get_static_mesh_description(0)
        vertices = [lod.get_vertex_position(u.VertexID(i)) for i in range(lod.get_vertex_count())]
        points = [(v.x, v.y, v.z) for v in vertices]
        ranges = [(min(p[i] for p in points), max(p[i] for p in points)) for i in range(3)]
        axis = max(range(3), key=lambda i: ranges[i][1]-ranges[i][0])
        lo, hi = ranges[axis]
        ends = [[p for p in points if abs(p[axis]-tip)<(hi-lo)*.18] for tip in (lo, hi)]
        head = max(ends, key=lambda ps: sum(max(p[i] for p in ps)-min(p[i] for p in ps) for i in range(3) if i!=axis))
        release = u.Vector(*(sum(p[i] for p in head)/len(head) for i in range(3)))
    weapon.set_editor_property("weapon_release_point", release)
    weapon.set_editor_property("use_weapon_release_point", True)
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assert E.save_loaded_asset(bp, False)
    u.log("WEAPON_RELEASE " + role + " " + str(release))
