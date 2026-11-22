"""Create the authored attack materials, meshes and loadouts in BoldersDemo."""

import math
from pathlib import Path

import unreal as u


E = u.EditorAssetLibrary
M = u.MaterialEditingLibrary
TOOLS = u.AssetToolsHelpers.get_asset_tools()
ROOT = "/Game/BoldersDemo"
EFFECTS = ROOT + "/Assets/Combat/Effects/Elemental"
COMBAT = ROOT + "/Assets/Combat"


def load(path):
    asset = E.load_asset(path)
    assert asset, path
    return asset


def save(asset):
    assert E.save_loaded_asset(asset, False), asset.get_name()
    return asset


def make(name, folder, cls, factory):
    path = f"{folder}/{name}"
    return load(path) if E.does_asset_exist(path) else TOOLS.create_asset(name, folder, cls, factory)


def material(name, shader, color, time="burst", blend=u.BlendMode.BLEND_ADDITIVE, particle=False, local=False):
    asset = make("M_" + name, EFFECTS, u.Material, u.MaterialFactoryNew())
    asset.set_editor_property("two_sided", True)
    asset.set_editor_property("blend_mode", blend)
    asset.set_editor_property("shading_model", u.MaterialShadingModel.MSM_UNLIT)
    M.delete_all_material_expressions(asset)
    node = M.create_material_expression(asset, u.MaterialExpressionCustom, 160, 0)
    if local:
        world = M.create_material_expression(asset, u.MaterialExpressionWorldPosition, -810, 0)
        position = M.create_material_expression(asset, u.MaterialExpressionTransformPosition, -500, 0)
        positions = u.MaterialPositionTransformSource
        position.set_editor_property("transform_source_type", positions.TRANSFORMPOSSOURCE_WORLD)
        position.set_editor_property("transform_type", positions.TRANSFORMPOSSOURCE_LOCAL)
        assert M.connect_material_expressions(world, "", position, "Input")
        inputs = {"LocalPosition": position}
    else:
        inputs = {"UV": M.create_material_expression(asset, u.MaterialExpressionTextureCoordinate, -500, 0)}
    if particle:
        asset.set_editor_property("used_with_niagara_sprites", True)
        asset.set_editor_property("used_with_niagara_ribbons", True)
        inputs["ParticleAlpha"] = M.create_material_expression(asset, u.MaterialExpressionParticleColor, -500, 380)
    if time != "static":
        inputs["Clock"] = M.create_material_expression(asset, u.MaterialExpressionTime, -500, 140)
    if time == "burst":
        for index, (parameter, default) in enumerate((("StartTime", 0.), ("Lifetime", .45))):
            scalar = M.create_material_expression(asset, u.MaterialExpressionScalarParameter, -500, 250 + index * 100)
            scalar.set_editor_property("parameter_name", parameter)
            scalar.set_editor_property("default_value", default)
            inputs[parameter] = scalar
    definitions = []
    for key in inputs:
        pin = u.CustomInput()
        pin.set_editor_property("input_name", key)
        definitions.append(pin)
    node.set_editor_property("inputs", definitions)
    rgba = blend == u.BlendMode.BLEND_TRANSLUCENT
    node.set_editor_property("output_type", u.CustomMaterialOutputType.CMOT_FLOAT4 if rgba
                             else u.CustomMaterialOutputType.CMOT_FLOAT3)
    tint = f"float3({color[0]},{color[1]},{color[2]})"
    header = "float t=saturate((Clock-StartTime)/max(.03,Lifetime));" if time == "burst" else ""
    node.set_editor_property("code", header + shader.replace("TINT", tint))
    for key, source in inputs.items():
        assert M.connect_material_expressions(source, "A" if key == "ParticleAlpha" else "", node, key)
    assert M.connect_material_property(node, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
    if rgba:
        alpha = M.create_material_expression(asset, u.MaterialExpressionComponentMask, 405, 120)
        alpha.set_editor_property("r", False)
        alpha.set_editor_property("g", False)
        alpha.set_editor_property("b", False)
        alpha.set_editor_property("a", True)
        assert M.connect_material_expressions(node, "", alpha, "")
        assert M.connect_material_property(alpha, "", u.MaterialProperty.MP_OPACITY)
    M.recompile_material(asset)
    return save(asset)


GOLD = (1., .54, .15)
STEEL = (.63, .78, 1.)
GREEN = (.19, 1., .44)
EMBER = (1., .17, .018)
FIRE = (1., .39, .025)
ICE = (.21, .73, 1.)
WATER = (.034, .43, 1.)
THUNDER = (.58, .25, 1.)

SLASH = """
float2 p=LocalPosition.xy/50; float a=atan2(p.y,p.x); float r=length(p);
float lead=lerp(-ANGLE+.16,ANGLE+.10,smoothstep(0,.58,t));
float cone=1-smoothstep(ANGLE-.11,ANGLE+.04,abs(a));
float sweep=cone*(smoothstep(-.02,.09,lead-a)*saturate(1-(lead-a)/TAIL)+.24*sin(t*3.14159));
float edge=exp(-abs(r-(.86+.08*t+.08*cos(a*1.8)))*67);
float inner=exp(-abs(r-(.77+.09*cos(a*2)))*10)*.85;
float strands=exp(-abs(r-.70-.045*cos(a*5))*45)*pow(saturate(cos(9*a)),3);
return (TINT*(4*inner+2*strands)+float3(6.4,7,7.8)*edge)*sweep*pow(1-t,1.4);
"""

TRAIL = """
float u=saturate(UV.x); float v=(UV.y-.5)*2;
float tail=pow(smoothstep(0,.90,u),.70);
float edge=exp(-abs(v)*8/(.24+.76*u));
float hair=exp(-abs(v-.26*sin(19*u+Clock*15))*20);
float streaks=.4+.6*pow(.5+.5*sin(31*u-Clock*32),3);
return (TINT*(edge*2.6+hair*.58)*streaks+float3(1.9,2.1,2.3)*pow(edge,4))*tail;
"""

FIRE_TRAIL = """
float u=UV.x; float v=(UV.y-.5)*2;
float flowing=sin(24*u+Clock*18+7*v)*sin(11*u+Clock*11-5*v);
float core=exp(-abs(v)*6/(.30+u))*smoothstep(0,.7,u);
float tongue=saturate(1-abs(v+.22*flowing)/(pow(u,.50)*.94+.025));
float flames=pow(tongue,2.3)*smoothstep(0,1,u)*(.45+.55*abs(flowing));
return TINT*(3.4*flames+1.25*core)+float3(4,2.8,1.5)*pow(core,4);
"""

ICE_TRAIL = """
float u=UV.x; float v=(UV.y-.5)*2;
float mist=exp(-v*v*5)*pow(u,1.6)*(.55+.45*sin(22*u-Clock*18+6*v));
float d=abs(v-.34*sin(12*u-Clock*12));
float sparks=exp(-38*d)*pow(saturate(cos(46*u+Clock*16)),12)*u;
return TINT*(1.7*mist+3.1*sparks)+float3(1,2.4,2.9)*exp(-abs(v)*21)*u*u;
"""

WAKE = """
float2 p=UV*2-1; float v=abs(p.y); float u=UV.x;
float ridge=1-u-(v*v*.57);
float noise=sin(Clock*25+28*u+12*p.y);
float outer=exp(-abs(ridge-.25+.035*noise)*23)*smoothstep(1,.46,v)*smoothstep(.03,.7,u);
float foam=pow(saturate(cos((ridge+Clock*.54)*24+v*9)),8)*smoothstep(0,.9,u);
return (TINT*(outer*2.2+foam*.68)+float3(.7,1.6,2.3)*pow(outer,2))*saturate(1-v);
"""

BURST = """
float2 p=(UV-.5)*2; float r=length(p); float a=atan2(p.y,p.x);
float blast=.11+.72*pow(t,.63);
float broken=.80+.20*sin(a*7+sin(a*3)*2);
float ring=exp(-abs(r-blast*broken)*16);
float rays=pow(saturate(cos(SPOKES*a+.38*sin(5*a)+PHASE)),11)*exp(-abs(r-blast*.83)*8);
float flash=exp(-r*8)*exp(-t*8);
float ripple=exp(-abs(r-blast*.72)*38)*pow(.5+.5*cos(9*a),3);
float grain=.71+.29*sin(76*r-21*t+9*a);
return ((ring*1.5+rays*2.2+ripple*.8)*TINT*grain+flash*float3(11,10,8))*pow(1-t,1.4);
"""

GROUND = """
float2 p=(UV-.5)*2; float r=length(p); float a=atan2(p.y,p.x);
float border=exp(-abs(r-.92)*61);
float ticks=exp(-abs(r-.84)*27)*pow(saturate(cos(a*12)),9);
float ramp=exp(-abs(r-lerp(.15,.91,t))*35);
float fog=pow(saturate(1-r),1.65);
return TINT*(.22*fog+1.7*border+ticks*.59+ramp*.9)*smoothstep(0,.1,t)*(1-.36*t);
"""

SHOCK_RING = """
float2 p=(UV-.5)*2; float r=length(p); float a=atan2(p.y,p.x);
float wave=lerp(.13,.95,pow(t,.43));
float cracks=pow(saturate(cos(a*11+.16*sin(a*8))),18)*exp(-abs(r-wave*.80)*12);
return TINT*(1.35*exp(-abs(r-wave)*30)+.7*cracks)*(1-t);
"""

BEAM = """
float u=UV.x, v=(UV.y-.5)*2;
float bend=.27*(1-u)*sin(u*52)+.17*sin(u*117+.4)+.07*sin(u*248+3*t);
float main=exp(-abs(v-bend)*36);
float fork=exp(-abs(v-bend-.55*sin(u*8+.1))*38)*smoothstep(.18,.30,u)*smoothstep(.93,.62,u);
float glow=exp(-abs(v-bend)*9);
float ground=exp(-u*12)*exp(-abs(v)*4);
return (TINT*(3.4*glow+5.5*fork+2.1*ground)+float3(9,8.3,11)*main)*pow(1-t,2.5);
"""

LINK = """
float u=UV.x; float v=(UV.y-.5)*2;
float split=abs(v-.19*sin(u*19));
float second=abs(v+.31*sin(u*16));
float head=exp(-abs(u-(.15+t*.80))*8);
return (TINT*(3.4*exp(-split*16)+1.5*exp(-second*19))+
    float3(1.6,3,2.3)*exp(-split*50))*head*(1-t)*sin(u*3.14159);
"""

CAST = """
float2 p=(UV-.5)*2; float r=length(p); float a=atan2(p.y,p.x);
float spread=.1+t*.68;
float head=pow(saturate(cos(a)),4);
float lance=exp(-abs(r-spread)*16)*pow(saturate(cos(a*3)),5)*head;
float core=exp(-r*8)*exp(-t*7);
return (TINT*lance*4+float3(5,5,5)*core)*(1-t);
"""

FIRE_CORE = """
float bands=sin(UV.x*37.7+sin(UV.y*13+Clock*17)*1.9+Clock*14);
float veins=pow(saturate(.45+.55*bands),5);
return float3(2.9,.18,.014)+veins*float3(6.8,3.7,.52);
"""

ICE_CORE = """
float z=UV.y; float facet=.48+.52*pow(saturate(abs(sin(UV.x*31.42))),.5);
float striation=pow(.5+.5*cos(UV.x*39.-z*46.),10);
float edge=pow(saturate(1-abs(frac(UV.x*6.)-.5)*2),7);
return float3(.028,.29,.60)*facet+float3(.38,1.1,1.5)*(edge*.74+striation*.22+z*z*.25);
"""

WAVE_BODY = """
float height=UV.y; float span=abs(UV.x-.5)*2;
float churn=.55+.45*sin(UV.x*53+height*29+Clock*15)*sin(UV.x*27-height*38-Clock*17);
float foam=smoothstep(.61,.95,height)*pow(churn,.64);
float folds=pow(saturate(cos(UV.x*45+height*18+Clock*6.5)),6)*.27;
float alpha=saturate(.27+foam*.66+folds*.85)*smoothstep(1,.64,span)*smoothstep(0,.22,height);
return float4(TINT*(.72+folds*2.7)+float3(.87,1.76,2.02)*foam,alpha);
"""

PARTICLE = """
float2 p=UV*2-1;
float soft=saturate(1-SHAPE);
float mask=pow(soft,1.55);
return (TINT*mask*3.8+float3(1.9,2.2,2.6)*pow(mask,2.4))*ParticleAlpha;
"""


def create_meshes():
    # OBJ positions are in centimetres. The legacy FBX/OBJ importer flips handedness.
    folder = Path(u.Paths.project_saved_dir()) / "ElementalCombat" / "MeshSource"
    folder.mkdir(parents=True, exist_ok=True)

    def import_mesh(name, points, faces):
        path = f"{EFFECTS}/{name}"
        source = (folder / (name + ".obj")).resolve()
        lines = [f"o {name}"]
        lines += [f"v {x:.4f} {-y:.4f} {z:.4f}" for x, y, z, _, _ in points]
        lines += [f"vt {v:.4f} {w:.4f}" for _, _, _, v, w in points]
        lines += ["f " + " ".join(f"{i}/{i}" for i in face) for face in faces]
        geometry = "\n".join(lines) + "\n"
        if E.does_asset_exist(path) and source.is_file() and source.read_text(encoding="ascii") == geometry:
            return load(path)
        source.write_text(geometry, encoding="ascii")
        task = u.AssetImportTask()
        task.filename = str(source)
        task.destination_path = EFFECTS
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = True
        task.factory = u.FbxFactory()
        options = u.FbxImportUI()
        options.set_editor_property("automated_import_should_detect_type", False)
        options.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
        options.import_materials = False
        options.import_textures = False
        options.static_mesh_import_data.combine_meshes = True
        options.static_mesh_import_data.auto_generate_collision = False
        task.options = options
        TOOLS.import_asset_tasks([task])
        mesh = next((a for a in task.get_objects() if isinstance(a, u.StaticMesh)), None)
        assert mesh, (name, task.imported_object_paths)
        u.log(f"ELEMENTAL_MESH {name} {mesh.get_bounding_box()}")
        return mesh

    # The tip is at the collision origin. The shaft flares into an irregular six-facet crystal.
    points = [(0, 0, 0, .5, 1)]
    faces = []
    for x, radius, shift, uv_y in ((-56, 20, .08, .53), (-108, 14, .27, .20), (-139, 5, .42, .06)):
        for i in range(7):
            angle = math.tau * i / 6 + shift
            points.append((x, math.cos(angle) * radius, math.sin(angle) * radius * .88, i / 6, uv_y))
    for i in range(6):
        faces += [(1, 3 + i, 2 + i), (2 + i, 3 + i, 10 + i, 9 + i),
                  (9 + i, 10 + i, 17 + i, 16 + i)]
    points.append((-148, 0, 0, .5, 0))
    faces += [(23, 16 + i, 17 + i) for i in range(6)]
    lance = import_mesh("SM_IceLance", points, faces)

    # A broad low flume that curls forward at the crest. The height falls toward the two wings.
    points = []
    faces = []
    cross_section = ((-67, -31), (-32, -24), (-5, -1), (2, 29), (18, 57), (48, 63), (68, 43))
    columns = 21
    for row, (x, z) in enumerate(cross_section):
        for column in range(columns):
            u_coord = column / (columns - 1)
            span = u_coord * 2.0 - 1.0
            falloff = 1.0 - abs(span) ** 1.7 * .80
            lip = 3.5 * math.cos(span * 11.0 + row * .8)
            points.append((x - 63 * span * span, span * 110., -36 + (z + 36 + lip) * falloff,
                           u_coord, row / (len(cross_section) - 1)))
            if row and column:
                i = row * columns + column + 1
                faces.append((i, i - 1, i - columns - 1, i - columns))
    return lance, import_mesh("SM_WaterCrest", points, faces)


def create_particle_systems():
    def niagara(name):
        path = f"/Niagara/DefaultAssets/Templates/Systems/{name}.{name}"
        result = u.load_object(None, path)
        assert isinstance(result, u.NiagaraSystem), path
        return result

    directional = niagara("DirectionalBurst")
    radial = niagara("RadialBurst")

    particles = {}
    particle_styles = (
        ("Gold", GOLD, "length(p*float2(.93,2.7))", directional),
        ("Steel", STEEL, "max(abs(p.x),abs(p.y)*3.8)", directional),
        ("Ricochet", GREEN, "abs(p.x)*.92+abs(p.y)*1.8", directional),
        ("Ember", EMBER, "length(p*float2(1.1,1.7))", radial),
        ("Flame", FIRE, "length(p*float2(1.65,.97))", radial),
        ("Frost", ICE, "abs(p.x)*1.3+abs(p.y)*.9", radial),
        ("Foam", WATER, "length(p*float2(.92,1.16))", directional),
        ("Thunder", THUNDER, "abs(p.x+.3*p.y)*1.3+abs(p.y)*.85", radial),
    )
    for effect, color, shape, template in particle_styles:
        material(f"Particle_{effect}", PARTICLE.replace("SHAPE", shape), color, "static", particle=True)
        name = f"NS_Impact_{effect}"
        path = f"{EFFECTS}/{name}"
        system = load(path) if E.does_asset_exist(path) else TOOLS.duplicate_asset(name, EFFECTS, template)
        assert system, name
        particles[effect] = save(system)
    u.SystemLibrary.execute_console_command(None, "Bolders.AssignAttackParticleMaterials")
    return particles


def apply():
    E.make_directory(EFFECTS)
    u.SystemLibrary.execute_console_command(None, "Bolders.BuildAttackModuleMontages")
    lance, water_crest = create_meshes()
    ice_body = material("IceLance", ICE_CORE, ICE, "static", u.BlendMode.BLEND_OPAQUE)
    wave_body = material("WaterCrest", WAVE_BODY, WATER, "loop", u.BlendMode.BLEND_TRANSLUCENT)
    fire_body = material("FireballCore", FIRE_CORE, FIRE, "loop", u.BlendMode.BLEND_OPAQUE)
    sphere = load("/Engine/BasicShapes/Sphere")
    particles = create_particle_systems()

    def burst(name, color, spokes, phase):
        return material(name, BURST.replace("SPOKES", str(spokes)).replace("PHASE", str(phase)), color)

    def blade(name, color, angle, tail):
        return material(name, SLASH.replace("ANGLE", str(angle)).replace("TAIL", str(tail)), color, local=True)

    def montage(role, name):
        return load(ROOT + f"/Assets/Characters/{role}/Animations/Montages/AM_{role}_{name}")

    def weapon_mesh(role, name):
        return load(ROOT + f"/Assets/Characters/{role}/Weapons/SM_{name}")

    def blueprint(role):
        return load(ROOT + f"/Blueprints/Characters/{role}/BP_{role}")

    def new(id_, label, element, pattern, damage, reach, cooldown, montages, **properties):
        data = u.AttackModule()
        values = dict(id=u.Name(id_), label=label, element=getattr(u.DamageElement, element),
                      pattern=getattr(u.AttackPattern, pattern), damage=damage, range=reach,
                      cooldown=cooldown, montages=montages, **properties)
        for key, value in values.items():
            data.set_editor_property(key, value)
        return data

    arrow_class = load(ROOT + "/Blueprints/Combat/Projectiles/BP_ArrowProjectile").generated_class()
    magic_class = load(ROOT + "/Blueprints/Combat/Projectiles/BP_MagicProjectile").generated_class()
    bow = [montage("Archer", "BasicAttack")]
    ice_wake = material("IceWake", ICE_TRAIL, ICE, "loop")
    ice_burst = burst("CrystalShatter", ICE, 6, .8)
    ground_fire = material("BlastScorch", SHOCK_RING, EMBER)
    ground_ice = material("FrostSigil", SHOCK_RING, ICE)
    warrior = [
        new("SwordShield", "剑盾", "PHYSICAL", "SINGLE_MELEE", 20., 82., .85,
            [montage("Warrior", name) for name in ("Chop", "Slice_Horizontal", "Stab")],
            weapon_mesh=weapon_mesh("Warrior", "sword_1handed"), offhand_mesh=weapon_mesh("Warrior", "shield_badge_color"),
            cast=blade("SwordShieldCut", GOLD, .79, 1.42), impact=burst("SwordContact", GOLD, 5, 0.),
            impact_particles=particles["Gold"]),
        new("Greatsword", "双手剑", "PHYSICAL", "CLEAVE", 33., 142., 1.65,
            [montage("Warrior", "GreatswordSweep"), montage("Warrior", "GreatswordChop")],
            weapon_mesh=weapon_mesh("Warrior", "sword_2handed_LongGrip"), weapon_grip_offset=u.Vector(-2,-8,75),
            cast=blade("GreatswordCrescent", STEEL, 1.10, 1.92),
            impact=burst("GreatswordContact", STEEL, 5, 1.5), impact_particles=particles["Steel"]),
    ]
    archer = [
        new("NormalArrow", "普通箭", "PHYSICAL", "PROJECTILE", 20., 750., 1.55, bow,
            projectile_class=arrow_class, speed=1550., trail=material("ArrowStreak", TRAIL, GOLD, "loop"),
            cast=material("BowRelease", CAST, GOLD), impact=burst("ArrowContact", GOLD, 4, 0.), impact_particles=particles["Gold"]),
        new("RicochetArrow", "弹射箭", "PHYSICAL", "RICOCHET", 19., 750., 3.2, bow,
            projectile_class=arrow_class, speed=1920., radius=340., trail=material("RicochetStreak", TRAIL, GREEN, "loop"),
            cast=material("RicochetLink", LINK, GREEN), impact=burst("RicochetFlare", GREEN, 7, .1), impact_particles=particles["Ricochet"]),
        new("ExplosiveArrow", "爆炸箭", "FIRE", "BLAST", 29., 680., 4.2, bow,
            projectile_class=arrow_class, speed=1280., radius=190., trail=material("EmberFletch", FIRE_TRAIL, EMBER, "loop"),
            cast=material("EmberIgnition", CAST, EMBER), impact=burst("ExplosiveArrowBurst", EMBER, 10, .42),
            ground_mark=ground_fire, impact_particles=particles["Ember"]),
        new("FreezingArrow", "冰冻箭", "ICE", "FREEZE", 16., 720., 4.0, bow,
            projectile_class=arrow_class, speed=1450., radius=78., trail=ice_wake,
            cast=material("FrostRelease", CAST, ICE), impact=ice_burst, ground_mark=ground_ice,
            projectile_mesh=lance, projectile_material=ice_body, impact_particles=particles["Frost"]),
    ]
    mage = [
        new("Fire", "火球", "FIRE", "BLAST", 32., 700., 2.2, [montage("Mage", "BasicAttack")],
            projectile_class=magic_class, speed=990., radius=135., trail=material("FireballPlume", FIRE_TRAIL, FIRE, "loop"),
            projectile_mesh=sphere, projectile_material=fire_body, cast=material("FireballRelease", CAST, FIRE),
            impact=burst("FireballBurst", FIRE, 8, .9), ground_mark=ground_fire, impact_particles=particles["Flame"]),
        new("Water", "涌浪", "WATER", "WAVE", 23., 650., 3.2, [montage("Mage", "WaterWave")],
            projectile_class=magic_class, speed=810., radius=110., projectile_mesh=water_crest, projectile_material=wave_body,
            trail=material("WaterWake", WAKE, WATER, "loop"), cast=material("WaterRelease", CAST, WATER),
            impact=burst("FoamSplash", WATER, 5, 1.1), impact_particles=particles["Foam"]),
        new("Lightning", "落雷", "LIGHTNING", "SKY_STRIKE", 38., 780., 4.8, [montage("Mage", "SkyStrike")],
            radius=166., ground_mark=material("ThunderWarning", GROUND, THUNDER),
            cast=material("ThunderBolt", BEAM, THUNDER), impact=burst("ThunderImpact", THUNDER, 8, 1.8),
            impact_particles=particles["Thunder"]),
        new("Ice", "冰锥", "ICE", "ICE_LANCE", 34., 790., 2.9, [montage("Mage", "IceLance")],
            projectile_class=magic_class, speed=1260., projectile_mesh=lance, projectile_material=ice_body,
            trail=ice_wake, cast=material("LanceRelease", CAST, ICE), impact=ice_burst, impact_particles=particles["Frost"]),
    ]
    for role, modules in (("Warrior", warrior), ("Archer", archer), ("Mage", mage)):
        factory = u.DataAssetFactory()
        factory.set_editor_property("data_asset_class", u.AttackLoadout)
        data = make(f"DA_{role}AttackLoadout", COMBAT, u.AttackLoadout, factory)
        data.set_editor_property("modules", modules)
        save(data)
        character = blueprint(role)
        defaults = u.get_default_object(character.generated_class())
        defaults.get_editor_property("combat").set_editor_property("attack_loadout", data)
        u.BlueprintEditorLibrary.compile_blueprint(character)
        save(character)
        u.log(f"ELEMENTAL_LOADOUT {role}: {[str(attack.id) for attack in modules]}")

    u.log("ELEMENTAL_ATTACK_ASSETS_READY")


if __name__ == "__main__":
    apply()
