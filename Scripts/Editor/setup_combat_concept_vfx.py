"""Rebuild the accepted combat VFX look without changing loadout gameplay values."""
import math
from pathlib import Path
import sys

import unreal as u

sys.path.insert(0, str(Path(u.Paths.project_dir()).resolve() / "Scripts" / "Editor"))
from setup_elemental_attacks import E, EFFECTS, COMBAT, TOOLS, load, material, save
from setup_warrior_blade_trail import author_montages, build_material

GOLD = (1., .63, .28)
MINT = (.19, 1., .56)
ICE = (.13, .62, 1.)
FIRE = (1., .25, .035)
STEEL = (.49, .69, 1.)
WATER = (.035, .52, .75)
THUNDER = (.57, .24, 1.)

ARROW = """
float x=saturate(UV.x), y=abs(UV.y-.5);
float width = lerp(.048,.19,x);
float core = exp(-y/max(.003,width*.27));
float rim = exp(-pow(y/width,2)*3.8);
return (TINT*(1.28*rim)+lerp(TINT,1,.55)*core*1.52)*smoothstep(0,.87,x);
"""
RELEASE = """
float2 p=UV-.5;
float r=length(p), a=atan2(p.y,p.x);
float ray=pow(saturate(cos(a*5.0+.2)),22)*smoothstep(.39,.02,r)*(1-smoothstep(.10,.20,t));
float core=exp(-r*26)*(1-smoothstep(0,.36,t));
return TINT*(ray*2.25)+lerp(TINT,1,.68)*(2.3*core);
"""
RICOCHET = """
float x=saturate(UV.x), y=(UV.y-.5)*2;
float head=min(1,t*2.8+.07);
float useful=(1-smoothstep(head,head+.06,x))*smoothstep(0,.07,x);
float route=exp(-pow(y/.20,2)*2.1)*useful;
float glint=exp(-pow(y/.42,2)*2)*exp(-36*abs(x-head));
float chevrons=0;
[unroll] for (int i=0;i<2;i++) {
float locator=.068+.10*i;
float shape=locator+.044*(1-abs(y));
chevrons+=exp(-83*abs(x-shape))*smoothstep(.85,.2,abs(y));
}
float alpha=(1-smoothstep(.16,1.,t));
return alpha*(TINT*(1.25*route+2.*chevrons)+lerp(TINT,1,.5)*(1.5*glint+.46*route));
"""
PETALS = """
float2 p=UV*2-1;
float r=length(p), a=atan2(p.y,p.x);
float phase=frac((a/6.283185+1)*9.+.075*sin(a*3));
float side = abs(phase-.5)*2;
float jag=.76+.19*cos(a*3.03-.5)+.08*sin(a*7.);
float reach=lerp(.26,.98,saturate(t*1.85))*jag;
float body=(1-smoothstep(.90,1.,r/reach + .71*pow(side,1.55)*smoothstep(0,.22,r)));
float edge=smoothstep(.24,.68,r);
float core=exp(-r*11.5)*exp(-t*7.);
float life=pow(1-t,1.9)*smoothstep(0,.085,t);
float wedge=smoothstep(.36,.51,r)*smoothstep(.03,.12,abs(phase-.5));
float3 hue=lerp(float3(2.55,.67,.06),float3(1.52,.16,.036),edge);
return life*(hue*body*.94+float3(2.5,1.65,.46)*body*(1-edge)*.57+core*float3(8,6,2.9));
"""
RING = """
float2 p=UV*2-1; float r=length(p); float a=atan2(p.y,p.x);
float phase=frac((a/6.283185+1)*8.);
float stroke=1-smoothstep(.03,.073,abs(r-lerp(.20,.962,pow(t,.43))));
float segment=smoothstep(.08,.145,phase)*(1-smoothstep(.86,.94,phase));
return TINT*stroke*segment*2.4*smoothstep(0,.10,t)*pow(1-t,.92);
"""
WARNING = """
float2 p=UV*2-1;
float r=length(p), a=atan2(p.y,p.x);
float sector=frac((a/6.283185+1)*4+.5);
float edge=1-smoothstep(.009,.024,abs(r-.975));
float segment=smoothstep(.063,.085,sector)*(1-smoothstep(.915,.937,sector));
float ticks= (1-smoothstep(.026,.048,abs(sector-.5)))*smoothstep(.88,.925,r)*(1-smoothstep(.975,.994,r));
float fill=.11*saturate(1-r)*smoothstep(.1,.85,t);
return (TINT*(edge*segment*(1.32+.36*t)+fill)+lerp(TINT,1,.59)*ticks*1.7)*smoothstep(0,.13,t);
"""
BOLT = """
float h=UV.x, y=(UV.y-.5)*2;
float bend=(.31*(1-abs(2*frac(h*4.7)-1))-.117)*smoothstep(.0,.15,h);
bend += .084*(1-abs(2*frac(h*11.5+.4)-1))*smoothstep(0,.2,h);
float d=abs(y-bend);
float branch=bend+.77*saturate((h-.28)*2.36)+.07*sin(h*83);
float branch2=bend-.68*saturate((h-.44)*2.45)+.041*sin(h*91);
float forks=exp(-58*abs(y-branch))*smoothstep(.28,.33,h)*(1-smoothstep(.57,.73,h))
          +exp(-60*abs(y-branch2))*smoothstep(.44,.49,h)*(1-smoothstep(.64,.79,h));
float bright=3.7*(1-smoothstep(.045,.088,d));
return (float3(.95,.78,1.)*bright+TINT*(2.4*exp(-d*8.)+2.1*forks))*pow(1-t,1.65);
"""
FIRE_BODY = """
float u=UV.x, h=1.-UV.y;
float curl=cos(6.283185*(u*3.-h*.83)+sin(h*7.)*.7);
float body=smoothstep(.14,.73,h);
float tongue=smoothstep(.085,.87,curl);
float stripe=exp(-pow((curl-.79)*4.3,2))*smoothstep(.1,.63,h);
float3 shell=lerp(float3(.53,.027,.012),float3(1.68,.29,.023),tongue);
float3 interior=float3(2.7,1.2,.14)+float3(1.35,1.15,.54)*pow(saturate(h),3.9);
return lerp(shell,interior,body*(.67+.33*tongue))+stripe*float3(1.3,.55,.047);
"""
FIRE_WAKE = """
float h=UV.x, y=(UV.y-.5)*2;
float stick=pow(1-abs(y),2.4)*smoothstep(0,.93,h);
float sway=.21*sin(h*9.9+Clock*16);
float tongue=exp(-abs(y-sway)*6.6)*smoothstep(.42,1.,h);
float cells=frac(h*5.-Clock*6.4);
float embers=exp(-abs(cells-.72)*24)*exp(-18*abs(y-.38*sin(floor(h*5.-Clock*6.4)*71.9)));
return float3(1.5,.19,.018)*(.48*stick+tongue)+float3(3.1,1.31,.13)*embers*smoothstep(.01,.22,h);
"""
WAVE = """
float x=UV.x, h=1.-UV.y;
float edge=abs(x*2-1);
float phase=cos(x*27+h*21)*.048+sin(x*19-h*11+Clock*5.6)*.035;
float foam=smoothstep(.69,.84,h+phase)*(1-.18*edge);
float crease=exp(-43*abs(h-.54-.08*sin(x*8.+h*7.)));
float band=smoothstep(.18,.58,h)*smoothstep(.91,.56,h);
float3 deep=float3(.018,.24,.34), mid=float3(.075,.65,.82), lip=float3(.89,1.31,1.4);
return lerp(lerp(deep,mid,band*.83+crease*.16),lip,foam);
"""
FOAM = """
float x=UV.x, y=(UV.y-.5)*2.;
float stem=abs(y)-.48*pow(max(x,.001),.64);
float cell=.5+.5*cos(33*x+13*y-Clock*7.);
float foam=exp(-abs(stem)*23.)*pow(cell,4.2);
return float3(.62,1.08,1.27)*foam*smoothstep(.1,.7,x);
"""
CRYSTAL = """
float h=1.-UV.y, angle=UV.x;
float facet=.41+.44*(.5+.5*cos(floor(angle*6.+.025)*2.35));
float edge=1-smoothstep(.012,.076,abs(frac(angle*6.+.5)-.5));
float cap=smoothstep(.66,.96,h);
float glint=exp(-abs(h-.64+angle*.23)*23)*.4;
return TINT*(facet*.94+cap*.33)+float3(.62,.99,1.18)*(edge*.61+cap*.74+glint);
"""
ICE_WAKE = """
float x=UV.x, y=(UV.y-.5)*2;
float spine=exp(-32*abs(y))*.85;
float flecks = exp(-18*abs(frac(x*6-Clock*8)-.5))*exp(-13*abs(y-.45*sin(x*20-Clock*5.)));
return TINT*(spine+1.45*flecks)*smoothstep(.03,.90,x);
"""
CONTACT = """
float2 p=UV*2-1; float r=length(p), a=atan2(p.y,p.x);
float spokes=pow(saturate(cos(6*a+.33)),21)*(1-smoothstep(.08,.68,r));
float flash=exp(-r*15);
return (TINT*spokes*1.15+lerp(TINT,1,.64)*flash*3.0)*pow(1-t,2.4);
"""


class Geometry:
    def __init__(self):
        self.points = []
        self.faces = []

    def vertex(self, x, y, z, u_coord, v_coord):
        self.points.append((x, y, z, u_coord, v_coord))
        return len(self.points)

    def tube(self, rings, sides, section):
        first = len(self.points) + 1
        for row, ring in enumerate(rings):
            for col in range(sides + 1):
                f = col / sides
                x, y, z, v = section(ring, f * math.tau)
                self.vertex(x, y, z, f, v)
                if row and col:
                    a = first + row * (sides + 1) + col
                    self.faces.append((a, a - 1, a - sides - 2, a - sides - 1))

    def mesh(self, name, default_material):
        folder = Path(u.Paths.project_saved_dir()) / "CombatConceptVFX" / "MeshSource"
        folder.mkdir(parents=True, exist_ok=True)
        source = (folder / (name + ".obj")).resolve()
        lines = [f"o {name}"]
        lines += [f"v {x:.4f} {-y:.4f} {z:.4f}" for x, y, z, _, _ in self.points]
        lines += [f"vt {v:.4f} {w:.4f}" for _, _, _, v, w in self.points]
        lines += ["f " + " ".join(f"{i}/{i}" for i in face) for face in self.faces]
        payload = "\n".join(lines) + "\n"
        if E.does_asset_exist(f"{EFFECTS}/{name}") and source.is_file() and source.read_text(encoding="ascii") == payload:
            mesh = load(f"{EFFECTS}/{name}")
            if mesh.get_material(0) != default_material:
                mesh.set_material(0, default_material)
                save(mesh)
            return mesh
        source.write_text(payload, encoding="ascii")
        task = u.AssetImportTask()
        task.filename = str(source)
        task.destination_path = EFFECTS
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        task.factory = u.FbxFactory()
        opts = u.FbxImportUI()
        opts.set_editor_property("automated_import_should_detect_type", False)
        opts.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
        opts.import_materials = False
        opts.import_textures = False
        opts.static_mesh_import_data.combine_meshes = True
        opts.static_mesh_import_data.auto_generate_collision = False
        task.options = opts
        TOOLS.import_asset_tasks([task])
        mesh = next((a for a in task.get_objects() if isinstance(a, u.StaticMesh)), None)
        assert mesh, (name, task.imported_object_paths)
        mesh.set_material(0, default_material)
        u.log(f"CONCEPT_MESH {name} {mesh.get_bounding_box()}")
        return save(mesh)


def build_fire(body):
    g = Geometry()
    rings = [(-128,.1,0),(-116,10,.08),(-96,19,.18),(-71,28,.32),
             (-48,37,.49),(-28,44,.64),(-7,39,.78),(12,23,.92),(18,.1,1.)]

    def section(r, a):
        x, radius, v = r
        lobe = math.cos(3*a - 4*v)
        radius *= 1 + (.30-.21*v)*lobe
        x += (1-v)*(1-v)*lobe*18
        return x, radius*math.cos(a), radius*.88*math.sin(a)+2*math.cos(v*8), v

    g.tube(rings, 30, section)
    return g.mesh("SM_ConceptFireComet", body)


def build_wave(body):
    g = Geometry()
    cross = [(-99,-40),(-74,-35),(-52,-27),(-34,-16),(-23,1),(-17,18),
             (-6,34),(10,43),(31,42),(47,31),(54,12),(51,3)]
    cols = 25
    for row, (x, z) in enumerate(cross):
        for col in range(cols):
            f = col/(cols-1)
            span = f*2-1
            wing = 1-abs(span)**2.5*.94
            chop = math.sin(span*13. + .41*row)*2.5*wing
            i=g.vertex(x - 43*span*span, span*109, -40+(z+40)*wing+chop, f, row/(len(cross)-1))
            if row and col:
                g.faces.append((i, i-1, i-cols-1, i-cols))
    return g.mesh("SM_ConceptCurlingWave", body)


def build_crystals(body):
    lance = Geometry()

    def spear(ring, a):
        x, radius, v = ring
        return x, radius*math.cos(a), .88*radius*math.sin(a), v

    lance.tube([(-139,.05,.01),(-120,11.,.10),(-60,28.,.51),(-34,22.,.64),(0.,.045,1.)], 6, spear)
    projectile = lance.mesh("SM_ConceptIceLance", body)

    crown = Geometry()
    for i, (angle, height, radius) in enumerate([(5,62,15),(47,40,12),(92,56,15),(138,43,13),
        (181,65,15),(221,47,13),(260,57,14),(299,51,13),(332,37,10)]):
        turn=math.radians(angle)
        dx,dy=math.cos(turn),math.sin(turn)
        distance=35. if i%2==0 else 43.
        def crystal(ring,a):
            h,w,v=ring
            base=distance + h*.23
            x, y = math.cos(a+.21*i)*w, math.sin(a+.21*i)*w
            return dx*base+x, dy*base+y, h, v
        crown.tube([(2,radius*.46,.03),(height*.37,radius,.38),
                    (height*.72,radius*.72,.72),(height,.045,1.)],6,crystal)
    return projectile, crown.mesh("SM_ConceptFrozenCrystals", body)


def mat(name, shader, color, time="burst", solid=False):
    return material("Concept_" + name, shader, color, time, u.BlendMode.BLEND_OPAQUE if solid else u.BlendMode.BLEND_ADDITIVE)


def update_loadout(role, changes):
    asset=load(f"{COMBAT}/DA_{role}AttackLoadout")
    modules=list(asset.get_editor_property("modules"))
    found=set()
    for module in modules:
        name=str(module.get_editor_property("id"))
        if name not in changes:
            continue
        found.add(name)
        for property_name,value in changes[name].items():
            module.set_editor_property(property_name,value)
    assert found == set(changes), (role,found)
    asset.set_editor_property("modules",modules)
    save(asset)
    for module in asset.get_editor_property("modules"):
        name = str(module.get_editor_property("id"))
        for key, expected in changes.get(name, {}).items():
            assert module.get_editor_property(key) == expected, (role, name, key)
    u.log(f"CONCEPT_LOADOUT {role} {sorted(found)}")


def apply():
    author_montages(build_material())

    fire_body=mat("FireComet",FIRE_BODY,FIRE,"static",True)
    wave_body=mat("WaveBody",WAVE,WATER,"loop",True)
    ice_body=mat("GlassyCrystal",CRYSTAL,ICE,"static",True)
    fire=build_fire(fire_body)
    wave=build_wave(wave_body)
    lance,crown=build_crystals(ice_body)
    cold_tail=mat("IceFlecks",ICE_WAKE,ICE,"loop")
    embers=mat("EmberTail",FIRE_WAKE,FIRE,"loop")
    frost_flash=mat("IceContact",CONTACT,ICE)
    petal=mat("FirePetals",PETALS,FIRE)
    ring=mat("FireRing",RING,FIRE)

    update_loadout("Warrior",{
        "SwordShield": dict(impact=mat("SteelContact",CONTACT,STEEL),impact_particles=None),
        "Greatsword": dict(impact=mat("HeavyContact",CONTACT,GOLD),impact_particles=load(EFFECTS+"/NS_Impact_Gold")),
    })
    update_loadout("Archer",{
        "NormalArrow": dict(trail=mat("ArrowTrace",ARROW,GOLD,"loop"),cast=mat("BowSnap",RELEASE,GOLD),
                            impact=mat("ArrowContact",CONTACT,GOLD),impact_particles=None),
        "RicochetArrow": dict(trail=mat("RicochetTrace",ARROW,MINT,"loop"),cast=mat("GreenBowSnap",RELEASE,MINT),
                              impact=mat("RicochetContact",CONTACT,MINT),ricochet_link=mat("RicochetChevrons",RICOCHET,MINT),impact_particles=None),
        "ExplosiveArrow": dict(trail=mat("ExplosiveTrace",ARROW,FIRE,"loop"),impact=petal,ground_mark=ring,pressure_ring=None),
        "FreezingArrow": dict(trail=cold_tail,impact=frost_flash,ground_mark=None,impact_shard_mesh=lance,impact_shard_material=ice_body),
    })
    update_loadout("Mage",{
        "Fire": dict(projectile_mesh=fire,projectile_material=fire_body,trail=embers,impact=petal,ground_mark=ring,pressure_ring=None),
        "Water": dict(projectile_mesh=wave,projectile_material=wave_body,trail=mat("FoamWake",FOAM,WATER,"loop"),
                      impact=mat("WaterSplash",CONTACT,WATER)),
        "Lightning": dict(ground_mark=mat("ThunderWarning",WARNING,THUNDER),cast=mat("ThunderBolt",BOLT,THUNDER),
                          impact=mat("ThunderContact",CONTACT,THUNDER)),
        "Ice": dict(projectile_mesh=lance,projectile_material=ice_body,trail=cold_tail,impact=frost_flash,
                    impact_shard_mesh=lance,impact_shard_material=ice_body),
    })
    for path in ("Enemy/BP_EnemyMushroom","Warrior/BP_Warrior","Archer/BP_Archer","Mage/BP_Mage"):
        bp=load("/Game/BoldersDemo/Blueprints/Characters/"+path)
        u.get_default_object(bp.generated_class()).set_editor_property("frozen_visual_mesh",crown)
        u.BlueprintEditorLibrary.compile_blueprint(bp)
        save(bp)
    u.log("COMBAT_CONCEPT_ASSETS_READY")


if __name__ == "__main__":
    apply()
