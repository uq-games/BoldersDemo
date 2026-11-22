"""Author the layered fireball and breaking-wave models against the Mage B sheet."""
import math
from pathlib import Path
import sys

import unreal as u

# UE can execute this entry point by path without adding its folder to sys.path.
script_dir = str(Path(__file__).resolve().parent)
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

from concept_vfx_assets import FIRE, WATER, Geometry, mat, update_loadout
from elemental_assets import M, material, save


FIRE_CORE = """
float h=1.-UV.y, u=UV.x;
float rim=.5+.5*cos(6.283185*(2*u+.30*h));
float heat=smoothstep(.08,.85,h);
return lerp(float3(2.1,.36,.025),float3(4.5,2.8,.82),heat*.75+rim*.20);
"""

FLAME_TONGUE = """
float x=1.-UV.y, across=abs(UV.x-.5)*2;
float rim=1-smoothstep(.12,.65,across);
float band=smoothstep(.15,.87,x)*rim;
float hot=.40*rim+.92*band;
return lerp(float3(.75,.046,.009),float3(3.3,1.19,.12),saturate(hot))
       +float3(1.8,1.08,.21)*pow(band,3.5);
"""

EMBER = """
return lerp(float3(1.5,.28,.018),float3(4.8,1.8,.12),smoothstep(.10,.90,1-UV.y));
"""

WATER_BODY = """
float h=1-UV.y, x=UV.x*2-1;
float f=smoothstep(.36,.69,h)*(1-smoothstep(.77,.98,h));
float ribs=.5+.5*cos(19*x+5*h+.35*sin(6*h));
float streak=pow(ribs,4)*smoothstep(.1,.75,h)*.18;
float3 belly=float3(.006,.070,.110);
float3 face=float3(.019,.40,.51);
float3 ridge=float3(.047,.76,.88);
float light=smoothstep(.0,.42,h)*(1-.23*abs(x));
float under=smoothstep(.61,.87,h);
float streak2=exp(-abs(x+.42+.18*sin(h*3.8))*16)*smoothstep(.19,.46,h)*(1-under);
float3 color=lerp(lerp(belly,face,light)+ridge*(.54*f+streak+streak2*.24),belly*.72,under*.89);
float alpha=lerp(.14,.97,smoothstep(.09,.49,h))*(1-.29*smoothstep(.66,1,abs(x)));
return float4(color,alpha);
"""

WATER_FOAM = """
float f=1-UV.y;
float blue=.5+.5*cos(6.283185*(UV.x+.08*sin(11*f)));
float3 underside=float3(.15,.65,.79);
return lerp(underside,float3(1.11,1.35,1.45),.43+.57*blue);
"""

FIRE_VAPOR = """
float x=UV.x, y=(UV.y-.5)*2;
float s=exp(-pow(y/(.12+.56*x),2)*3.);
float arc=exp(-abs(y-.17*sin(19*x+Clock*19))*19.);
float heat=smoothstep(.02,.94,x)*(1-smoothstep(.85,1,x));
float alpha=heat*s*(.39+.18*cos(23*x+Clock*9+8*y));
return float4(float3(.12,.025,.013)+float3(2.9,.42,.038)*arc*x,alpha);
"""

FOAM_WAKE = """
float x=UV.x, y=(UV.y-.5)*2.;
float span=abs(y)-(.29+.31*x);
float curb=exp(-abs(span)*30);
float drops=pow(.5+.5*cos(41*x+10*y-Clock*10),7);
float flash=exp(-abs(span-.095)*17.)*drops;
return float3(.43,.98,1.23)*(curb*.45+flash*.85)*smoothstep(.02,.70,x);
"""

SPLASH = """
float2 p=UV*2-1;
float r=length(p), a=atan2(p.y,p.x);
float wave=lerp(.11,.94,pow(t,.61));
float spray=pow(saturate(cos(a*7.0+.22)),14)*exp(-10*abs(r-wave));
float bloom=exp(-r*11-t*6);
return float3(.41,.95,1.19)*(2.15*spray+1.78*bloom)*pow(1-t,1.65);
"""


def refine(rows, subdivisions=4):
    for i in range(len(rows)-1):
        a, b, c, d = rows[max(0,i-1)], rows[i], rows[i+1], rows[min(i+2,len(rows)-1)]
        for k in range(subdivisions):
            t = k / subdivisions
            yield tuple(.5*((2*y)+(-x+z)*t+(2*x-5*y+4*z-w)*t*t+(-x+3*y-3*z+w)*t*t*t)
                        for x,y,z,w in zip(a,b,c,d))
    yield rows[-1]


def ellipse(g, center, size, sides=16):
    def shape(f, a):
        theta = math.pi * f
        return (center[0] - size[0]*math.cos(theta),
                center[1] + size[1]*max(.002,math.sin(theta))*math.cos(a),
                center[2] + size[2]*max(.002,math.sin(theta))*math.sin(a), f)
    g.tube([j / 16 for j in range(17)], sides, shape)


def fire_models(core_mat, tongues_mat, ember_mat):
    core=Geometry()
    ellipse(core, (-14,0,0), (42,31,31), 30)
    flames=Geometry()
    for i, (start, stretch, turn, width) in enumerate([(0.,142.,2.72,12.),(2.12,124.,2.38,10.5),(4.19,106.,2.20,9.1)]):
        def tongue(f, a):
            # A twisting ribbon with a real silhouette, wrapping the bright core before flaring outward.
            angle=start+turn*f
            x=21.-stretch*(1.-f)**1.13
            radius=(13.+27.*math.sin(math.pi*f*.89)+8.*(1.-f))
            angle+=.30*math.sin(8*f+i)
            thick=(.012+max(0.,math.sin(math.pi*f))**.91)
            ellipse_y=math.cos(a)*width*thick
            ellipse_z=math.sin(a)*3.6*thick
            return (x, math.cos(angle)*(radius+ellipse_z)-math.sin(angle)*ellipse_y,
                    math.sin(angle)*(radius+ellipse_z)+math.cos(angle)*ellipse_y, f)
        flames.tube([j / 22 for j in range(23)], 10, tongue)
    embers=Geometry()
    ellipse(embers,(0,0,0),(8.5,2.9,2.55), 6)
    return (core.mesh("SM_FireballHotCore",core_mat),flames.mesh("SM_FireballCoilingTongues",tongues_mat),
            embers.mesh("SM_FireballEmber",ember_mat))


def wave_models(body_mat, foam_mat):
    wave=Geometry()
    # Water gathers into the crest, then the lip curls over open air.
    profile=[(-143,-38),(-112,-32),(-84,-24),(-57,-15),(-43,4),(-32,24),(-21,50),
             (-8,72),(13,86),(39,81),(58,65),(63,44),(52,29)]
    rows=list(refine(profile))
    cols=37

    def section(x,z,f,row):
        s=2*f-1
        progress=row/(len(rows)-1)
        heart=s+.16
        height=max(.08,1-abs(heart)**2.20*.89)
        edge=1-abs(s)**2*.31
        droop=(.52+.48*height)
        width=(.42+.58*min(1.,progress*2.1))*(1-.16*max(0.,progress-.55)/.45)
        return (x*edge-52*s*s+25*s+3.9*math.sin(8*s+row*.21)*height, 108*s*width,
                -39+(z+39)*height+2.8*math.sin(10*s-row*.12)*droop)

    for row,(x,z) in enumerate(rows):
        for col in range(cols):
            f=col/(cols-1)
            px,py,pz=section(x,z,f,row)
            n=wave.vertex(px,py,pz,f,row/(len(rows)-1))
            if row and col:
                wave.faces.append((n,n-1,n-cols-1,n-cols))

    foam=Geometry()
    # Crosswise lip is a scalloped roll, plus individual lobes at the crest and wake.
    def crest(f,a):
        s=f*2-1
        x,y,z=section(49,75,f,(len(rows)-1)*.82)
        k=(.4+.6*(1-abs(s)**2.6))*(.84+.16*math.cos(s*21.))
        return x+math.cos(a)*16.*k,y,z+math.sin(a)*9.*k+1.5,f
    foam.tube([j/48 for j in range(49)],14,crest)
    for j,s in enumerate([-.81,-.61,-.38,-.10,.15,.36,.56,.76]):
        f=(s+1)/2
        row=38 + j%3
        x,y,z=section(49,74,f,row)
        ellipse(foam,(x+4+j%3*5,y,z-8+j%2*4),(12+j%3*2,11+j%2*3,7+j%3*2),12)
    for x,y,z,rx,ry in [(-85,-66,-24,17,7),(-102,66,-29,13,6),(-116,-72,-29,11,6),(-112,47,-30,16,5)]:
        ellipse(foam,(x,y,z),(rx,ry,3.7),10)

    droplets=Geometry()
    ellipse(droplets,(0,0,0),(8.2,4.75,4.1),10)
    return (wave.mesh("SM_SeawaveDeepCurl",body_mat),foam.mesh("SM_SeawaveFoamCap",foam_mat),
            droplets.mesh("SM_SeawaveSprayDrop",foam_mat))


def apply():
    def instanced(asset):
        # Save the ISM permutation so the first cast can render mesh particles immediately.
        asset.set_editor_property("used_with_instanced_static_meshes", True)
        M.recompile_material(asset)
        return save(asset)

    core_mat=mat("FireballHotCore",FIRE_CORE,FIRE,"static",True)
    flame_mat=mat("FireballCoilingTongues",FLAME_TONGUE,FIRE,"static",True)
    ember_mat=instanced(mat("FireballEmber",EMBER,FIRE,"static",True))
    fire, tongues, ember = fire_models(core_mat, flame_mat, ember_mat)
    body_mat=material("Concept_SeawaveDeepCurl", WATER_BODY, WATER, "static", u.BlendMode.BLEND_TRANSLUCENT)
    foam_mat=instanced(mat("SeawaveFoamCap",WATER_FOAM,WATER,"static",True))
    wave, foam, spray = wave_models(body_mat, foam_mat)
    vapor=material("Concept_FireballVapor", FIRE_VAPOR, FIRE, "loop", u.BlendMode.BLEND_TRANSLUCENT)
    foam_wake=mat("SeawaveWake",FOAM_WAKE,WATER,"loop")
    update_loadout("Mage", {
        "Fire": dict(projectile_mesh=fire, projectile_material=core_mat,
                     projectile_layers=[tongues], flight_particle_mesh=ember, trail=vapor),
        "Water": dict(projectile_mesh=wave, projectile_material=body_mat,
                      projectile_layers=[foam], flight_particle_mesh=spray, trail=foam_wake,
                      impact=mat("SeawaveSplash",SPLASH,WATER)),
    })
    u.log("MAGE_LAYERED_VISUALS_READY")


if __name__ == "__main__":
    apply()
