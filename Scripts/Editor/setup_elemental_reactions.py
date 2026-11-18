"""Author visuals, update the existing loadouts and place reactive barrels in TestMap."""

from pathlib import Path
import runpy

import unreal as u


SETUP = runpy.run_path(str(Path(__file__).with_name("setup_elemental_attacks.py")))
ROOT = SETUP["ROOT"]
EFFECTS = SETUP["EFFECTS"]
COMBAT = SETUP["COMBAT"]
E, M, TOOLS = SETUP["E"], SETUP["M"], SETUP["TOOLS"]
load, save, make, material = (SETUP[name] for name in ("load", "save", "make", "material"))
GOLD, STEEL, GREEN, EMBER, FIRE, ICE, WATER, THUNDER = (
    SETUP[name] for name in ("GOLD", "STEEL", "GREEN", "EMBER", "FIRE", "ICE", "WATER", "THUNDER"))

WINDUP = """
float2 p=(UV-.5)*2;
float r=length(p), a=atan2(p.y,p.x);
float t=frac(Clock*.92);
float angular=.66+.34*cos(a-Clock*4.8);
float outer=exp(-abs(r-.76)*33)*(.48+.52*pow(saturate(cos(a*8)),4));
float count=exp(-abs(r-lerp(.61,.31,t))*21)*sin(t*3.14159);
float glyph=GLYPH;
float direction=exp(-abs(abs(a)-.27)*21)*exp(-abs(r-.88)*19)*step(0,p.x);
return saturate(outer*angular+glyph*.87+count*.42+direction*.9);
"""

REACTION_RING = """
float2 p=(UV-.5)*2; float r=length(p), a=atan2(p.y,p.x);
float peak=lerp(.16,.98,pow(t,.57));
float front=exp(-abs(r-peak)*52);
float wake=exp(-abs(r-peak*.73)*21);
float saw=pow(saturate(cos(SEGMENTS*a+.24*sin(a*5))),13);
float edge=smoothstep(.015,.1,t)*pow(1-t,.9);
return (TINT*(front*1.8+wake*.57+saw*wake*1.23)+float3(.6,.67,.76)*front*.28)*edge;
"""

STEAM = """
float2 p=(UV-.5)*2;
p.x*=.87;
p.y+=t*.47;
float noise=sin(17*p.x-13*p.y+t*7)*sin(23*p.y+t*12);
float clouds=exp(-length(p+float2(.04+.10*noise,.17*noise))*lerp(7.,2.7,t));
float wisps=exp(-abs(p.x+.25*sin(5*p.y+t*4))*4)*smoothstep(.92,.18,abs(p.y));
float fade=smoothstep(0,.18,t)*pow(1-t,1.35);
return float4(TINT*(.55+.48*clouds+wisps*.12), saturate((clouds*.96+wisps*.22)*fade));
"""

CHARGED_LINK = """
float u=UV.x, v=(UV.y-.5)*2;
float a=.23*sin(u*41)+.18*sin(u*109+2*t);
float filament=exp(-abs(v-a)*29);
float bloom=exp(-abs(v-a)*7);
float pulse=.75+.25*sin(u*28+24*t);
return (TINT*(2.8*bloom)+float3(5.8,6.2,7)*filament)*sin(3.14159*u)*pulse*pow(1-t,1.35);
"""


def windup_decal(name, tint, glyph):
    asset = make("M_" + name + "Windup", EFFECTS, u.Material, u.MaterialFactoryNew())
    asset.set_editor_property("blend_mode", u.BlendMode.BLEND_TRANSLUCENT)
    asset.set_editor_property("material_domain", u.MaterialDomain.MD_DEFERRED_DECAL)
    M.delete_all_material_expressions(asset)
    uv = M.create_material_expression(asset, u.MaterialExpressionTextureCoordinate, -470, 0)
    clock = M.create_material_expression(asset, u.MaterialExpressionTime, -470, 120)
    shape = M.create_material_expression(asset, u.MaterialExpressionCustom, -230, 0)
    pins = []
    for name in ("UV", "Clock"):
        pin = u.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    shape.set_editor_property("inputs", pins)
    shape.set_editor_property("output_type", u.CustomMaterialOutputType.CMOT_FLOAT1)
    shape.set_editor_property("code", WINDUP.replace("GLYPH", glyph))
    color = M.create_material_expression(asset, u.MaterialExpressionConstant3Vector, -215, 180)
    color.set_editor_property("constant", u.LinearColor(*(2.8 * channel for channel in tint), 1.))
    assert M.connect_material_expressions(uv, "", shape, "UV")
    assert M.connect_material_expressions(clock, "", shape, "Clock")
    assert M.connect_material_property(shape, "", u.MaterialProperty.MP_OPACITY)
    assert M.connect_material_property(color, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
    M.recompile_material(asset)
    return save(asset)


def author_reactions():
    factory = u.DataAssetFactory()
    factory.set_editor_property("data_asset_class", u.ElementalReactionAppearance)
    asset = make("DA_ElementalReactionAppearance", COMBAT, u.ElementalReactionAppearance, factory)
    asset.set_editor_property("crystal_mesh", load(EFFECTS + "/SM_IceLance"))
    asset.set_editor_property("crystal_material", load(EFFECTS + "/M_IceLance"))
    row_specs = (
        ("Steam", (.74,.89,1.), 7, .1, "Foam"),
        ("Melt", (1.,.65,.26), 11, .4, "Flame"),
        ("Freeze", (.16,.81,1.), 6, .0, "Frost"),
        ("Conduct", (.41,.73,1.), 9, 1.2, "Thunder"),
        ("Overload", (1.,.25,.54), 12, .35, "Ember"),
        ("Fracture", (.60,.49,1.), 5, 1.4, "Frost"),
        ("Shatter", (.43,.95,1.), 4, .4, "Steel"),
    )
    rows = []
    for name, tint, spokes, phase, particles in row_specs:
        row = u.ReactionAppearance()
        burst = (material("ReactionSteam", STEAM, tint, blend=u.BlendMode.BLEND_TRANSLUCENT)
                 if name == "Steam" else material("Reaction" + name,
                     SETUP["BURST"].replace("SPOKES", str(spokes)).replace("PHASE", str(phase)), tint))
        ring = material("ReactionRing" + name, REACTION_RING.replace("SEGMENTS", str(spokes)), tint)
        properties = dict(reaction=getattr(u.ElementalReaction, name.upper()), burst=burst, ring=ring,
                          particles=load(EFFECTS + "/NS_Impact_" + particles))
        if name == "Conduct":
            properties["beam"] = material("ReactionConductionArc", CHARGED_LINK, tint)
        for key, value in properties.items():
            row.set_editor_property(key, value)
        rows.append(row)
    asset.set_editor_property("reactions", rows)
    return save(asset)


def author_loadouts(reactions):
    windups = {
        "SwordShield": (GOLD, "exp(-abs(abs(a)-.53)*21)*exp(-abs(r-.57)*11)"),
        "Greatsword": (STEEL, "exp(-abs(r-.51-.11*cos(a))*29)*smoothstep(-.6,.7,cos(a))"),
        "NormalArrow": (GOLD, "exp(-abs(p.y)*26)*smoothstep(.04,.42,p.x)*smoothstep(.9,.46,p.x)"),
        "RicochetArrow": (GREEN, "exp(-abs(p.y-.22*sin(p.x*13))*24)*smoothstep(.0,.48,p.x)"),
        "ExplosiveArrow": (EMBER, "exp(-abs(r-.43)*26)*pow(saturate(cos(a*4+t*7)),4)"),
        "FreezingArrow": (ICE, "pow(saturate(cos(a*6)),12)*smoothstep(.56,.20,r)"),
        "Fire": (FIRE, "exp(-abs(r-.42-.09*sin(6*a-t*15))*22)*(1.+.35*cos(5*a))"),
        "Water": (WATER, "exp(-abs(p.x-.22-.33*p.y*p.y)*24)*smoothstep(.66,.23,abs(p.y))"),
        "Lightning": (THUNDER, "exp(-abs(p.y-.20*sin(21*p.x+t*8))*22)*smoothstep(.7,.25,abs(p.x))"),
        "Ice": (ICE, "exp(-abs(r-(.36+.075*cos(6*a)))*36)+pow(saturate(cos(a*6)),15)*smoothstep(.45,.10,r)"),
    }
    pushes = {"Greatsword": 185., "ExplosiveArrow": 970., "Fire": 770., "Water": 195.}
    pressure = {
        "ExplosiveArrow": material("ArrowPressureFront", REACTION_RING.replace("SEGMENTS", "11"), (.98,.31,.12)),
        "Fire": material("FireballPressureFront", REACTION_RING.replace("SEGMENTS", "8"), (1.,.55,.18)),
    }
    for role in ("Warrior", "Archer", "Mage"):
        asset = load(COMBAT + f"/DA_{role}AttackLoadout")
        # Editable struct entries are copies when read from a reflected array.
        modules = []
        for module in asset.get_editor_property("modules"):
            name = str(module.get_editor_property("id"))
            color, glyph = windups[name]
            module.set_editor_property("windup", windup_decal(name, color, glyph))
            if name in pushes:
                module.set_editor_property("impact_push", pushes[name])
            if name in pressure:
                module.set_editor_property("pressure_ring", pressure[name])
            modules.append(module)
        asset.set_editor_property("modules", modules)
        asset.set_editor_property("reaction_appearance", reactions)
        save(asset)
        written = asset.get_editor_property("modules")
        assert all(x.get_editor_property("windup") for x in written)
        assert all(abs(x.get_editor_property("impact_push") - pushes[str(x.get_editor_property("id"))]) < .01
                   for x in written if str(x.get_editor_property("id")) in pushes)
        u.log(f"ELEMENTAL_REACTION_LOADOUT {role} {len(modules)}")


def author_props():
    folder = COMBAT + "/Props"
    barrel_mat = make("M_ImpulseBarrel", folder, u.Material, u.MaterialFactoryNew())
    M.delete_all_material_expressions(barrel_mat)
    uv = M.create_material_expression(barrel_mat, u.MaterialExpressionTextureCoordinate, -340, 0)
    pattern = M.create_material_expression(barrel_mat, u.MaterialExpressionCustom, -110, 0)
    uv_input = u.CustomInput()
    uv_input.set_editor_property("input_name", "UV")
    pattern.set_editor_property("inputs", [uv_input])
    pattern.set_editor_property("output_type", u.CustomMaterialOutputType.CMOT_FLOAT3)
    pattern.set_editor_property("code", """
float stave=floor(UV.x*14);
float seam=pow(abs(frac(UV.x*14)-.5)*2,17);
float shade=.76+.24*frac(sin(stave*15.11)*59.21);
float grain=.93+.07*sin(UV.x*760+2.7*sin(UV.y*19)+stave*3);
float band=max(1-smoothstep(.043,.055,abs(UV.y-.18)),1-smoothstep(.043,.055,abs(UV.y-.82)));
float rivet=exp(-abs(frac(UV.x*14)-.5)*21)*max(exp(-abs(UV.y-.18)*45),exp(-abs(UV.y-.82)*45));
return lerp(float3(.35,.131,.050)*shade*grain*(1-.66*seam),float3(.066,.089,.118)+.31*rivet,band);
""")
    assert M.connect_material_expressions(uv, "", pattern, "UV")
    assert M.connect_material_property(pattern, "", u.MaterialProperty.MP_BASE_COLOR)
    M.recompile_material(barrel_mat)
    save(barrel_mat)

    blueprint_dir = ROOT + "/Blueprints/Combat/Props"
    factory = u.BlueprintFactory()
    factory.set_editor_property("parent_class", u.CombatImpulseProp)
    blueprint = make("BP_ImpulseBarrel", blueprint_dir, u.Blueprint, factory)
    defaults = u.get_default_object(blueprint.generated_class())
    mesh = defaults.get_editor_property("mesh")
    mesh.set_static_mesh(load("/Engine/BasicShapes/Cylinder"))
    mesh.set_material(0, barrel_mat)
    mesh.set_relative_scale3d(u.Vector(.58,.58,.84))
    u.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(blueprint)

    map_path = ROOT + "/Assets/Maps/TestMap"
    assert u.get_editor_subsystem(u.LevelEditorSubsystem).load_level(map_path)
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    existing = actors.get_all_level_actors()
    enemies = sorted((a for a in existing if isinstance(a,u.EnemyCharacter)), key=lambda a:a.get_actor_label())
    assert len(enemies) >= 2
    for index, (enemy, delta) in enumerate(zip(enemies, (u.Vector(60,118,0), u.Vector(80,-124,0))), 1):
        label = f"ImpulseBarrel_{index}"
        if any(a.get_actor_label() == label for a in existing):
            continue
        position = enemy.get_actor_location() + delta
        floor = u.SystemLibrary.line_trace_single(enemy, position + u.Vector(0,0,120), position - u.Vector(0,0,230),
            u.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [enemy], u.DrawDebugTrace.NONE, True)
        if floor:
            position.z = floor.get_editor_property("impact_point").z + 43
        else:
            position.z = 43
        barrel = actors.spawn_actor_from_class(blueprint.generated_class(),position)
        assert barrel
        barrel.set_actor_label(label)
        u.log(f"ELEMENTAL_TEST_PROP {label} {barrel.get_actor_location()}")
    assert u.EditorLoadingAndSavingUtils.save_current_level()


if __name__ == "__main__":
    author_loadouts(author_reactions())
    author_props()
    u.log("ELEMENTAL_REACTION_ASSETS_READY")
