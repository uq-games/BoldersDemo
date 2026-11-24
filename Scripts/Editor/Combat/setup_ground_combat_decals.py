"""Author the three projected decals used by combat presentation."""
import unreal as u

ASSETS = u.EditorAssetLibrary
EDIT = u.MaterialEditingLibrary
FOLDER = "/Game/BoldersDemo/Assets/Combat/Effects"

# The decals use the same UV boundary as the presentation code's world-space sizing.
FOOTPRINT = """
float2 p = 2.0 * (UV - .5);
float r = length(p);
float aa = max(fwidth(r) * .7, .003);
float a = atan2(p.y, p.x);
float outer = 1.0 - smoothstep(.88 - aa, .88 + aa, r);
float border = outer * smoothstep(.801 - aa, .801 + aa, r);
float inner = outer * smoothstep(.705 - aa, .705 + aa, r);
float direction = 1.0 - smoothstep(.13, .24, abs(p.x));
float playerMark = direction * smoothstep(.54, .68, -p.y) * outer;
float enemyCut = 1.0 - smoothstep(.80, .97, abs(cos(a * 2.0 + .785)));
float playerCut = lerp(1.0, smoothstep(.10, .20, abs(p.x)), step(.5, Style));
float cut = lerp(playerCut, enemyCut, step(1.5, Style));
float mark = lerp(playerMark, 0.0, step(1.5, Style));
float edge = max(border * cut, mark);
float opacity = saturate((inner * .34 + edge * .68) * Brightness);
return float4(lerp(Tint.rgb * .17, Tint.rgb, saturate(edge)), opacity);
"""

REACH = """
float2 p = (UV - .5) * (2.0 / .88);
float r = length(p);
float aa = max(fwidth(r) * .65, .0018);
float a = atan2(p.y, p.x);
float distant = step(.5, Style);
float impact = step(1.5, Style);
float lineWidth = lerp(.037, .013, distant);
float key = smoothstep(.965, .999, abs(cos(a * 4.0)));
float breaks = lerp(1.0, smoothstep(.12, .31, abs(sin(a * 8.0))), impact);
float border = 1.0 - smoothstep(lineWidth - aa, lineWidth + aa, abs(r - 1.0));
float lining = 1.0 - smoothstep(lineWidth * 2.2, lineWidth * 2.2 + aa, abs(r - 1.0));
float marks = smoothstep(.886, .908, r) * (1.0 - smoothstep(.959, .975, r)) * key;
float center = (1.0 - smoothstep(.014, .039, min(abs(p.x), abs(p.y)))) * (1.0 - smoothstep(.11, .16, r)) * impact;
float edge = max(border * breaks, max(marks * .64, center * .8));
float alpha = Brightness * saturate(lining * breaks * .34 + edge * .84);
return float4(lerp(Tint.rgb * .13, Tint.rgb, saturate(edge)), alpha);
"""

DANGER = """
float2 p = (UV - .5) * (2.0 / .88);
float r = length(p);
float a = atan2(p.y, p.x);
float aa = max(fwidth(r) * .65, .003);
float inside = 1.0 - smoothstep(1.0 - aa, 1.0 + aa, r);
float t = saturate((Clock - Start) / max(Duration,.01));
float after = saturate((Clock - Start - Duration) / .22);
float size = lerp(.065, 1.0, t);
float edgeWidth = lerp(.047, .033, step(2.5, Style));
float rim = smoothstep(1.0 - edgeWidth - aa, 1.0 - edgeWidth + aa, r) * inside;
float innerBorder = smoothstep(.916, .952, r) * inside;
float revolution = Style < .5 ? 6.0 : (Style < 1.5 ? 4.0 : (Style < 3.5 ? 8.0 : 10.0));
float tooth = smoothstep(.78, .96, cos(a * revolution));
float teeth = tooth * smoothstep(.862, .887, r) * inside;
float stripes = smoothstep(.27, .42, abs(frac((p.x + p.y) * 7.5) - .5));
float fill = (1.0 - smoothstep(size - aa, size + aa, r)) * inside;
float sweep = (1.0 - smoothstep(.021, .021 + aa, abs(r - size))) * inside;
float aim = (1.0 - smoothstep(.016, .042, min(abs(p.x), abs(p.y)))) * (1.0 - smoothstep(.15, .21, r)) * inside;
float detail = saturate(max(rim, max(teeth * .84, sweep * .69)));
float opacity = .115 * inside + .105 * innerBorder + .16 * fill + .11 * stripes * fill
              + .56 * rim + .25 * teeth + .22 * sweep + .29 * aim;
float3 color = lerp(Tint.rgb * .58, lerp(Tint.rgb, float3(1.,.74,.41), .30 + .40 * t), detail);
float3 impactColor = lerp(Tint.rgb, float3(1.,.64,.28), saturate(1.0 - after));
color = lerp(color, impactColor, Contact);
return float4(color, saturate(lerp(opacity, (.14 * inside + .82 * rim) * (1.0 - after), Contact)));
"""

GROUND_MASK = """
float3 c = cross(ddx(World), ddy(World));
float z = abs(c.z) * rsqrt(max(dot(c,c), 1e-12));
return Layer.a * smoothstep(.33, .60, z);
"""


def wire(src, destination, name, output=""):
    assert EDIT.connect_material_expressions(src, output, destination, name), name


def pin(name):
    value = u.CustomInput()
    value.set_editor_property("input_name", name)
    return value


def shader(material, source, inputs, output, x, y):
    node = EDIT.create_material_expression(material, u.MaterialExpressionCustom, x, y)
    node.set_editor_properties(dict(code=source, inputs=[pin(k) for k in inputs], output_type=output))
    for key, expression in inputs.items():
        wire(expression, node, key)
    return node


def scalar(material, name, value, y):
    node = EDIT.create_material_expression(material, u.MaterialExpressionScalarParameter, -600, y)
    node.set_editor_properties(dict(parameter_name=name, default_value=value))
    return node


def decal(name, source, timed=False):
    path = f"{FOLDER}/{name}"
    tools = u.AssetToolsHelpers.get_asset_tools()
    mat = ASSETS.load_asset(path) if ASSETS.does_asset_exist(path) else tools.create_asset(name, FOLDER, u.Material, u.MaterialFactoryNew())
    mat.set_editor_properties(dict(material_domain=u.MaterialDomain.MD_DEFERRED_DECAL,
                                  blend_mode=u.BlendMode.BLEND_TRANSLUCENT))
    EDIT.delete_all_material_expressions(mat)
    uv = EDIT.create_material_expression(mat, u.MaterialExpressionTextureCoordinate, -600, -100)
    world = EDIT.create_material_expression(mat, u.MaterialExpressionWorldPosition, -160, 400)
    tint = EDIT.create_material_expression(mat, u.MaterialExpressionVectorParameter, -600, 0)
    tint.set_editor_properties(dict(parameter_name="RingColor", default_value=u.LinearColor(.24, .72, 1., 1.)))
    inputs = {"UV": uv, "Tint": tint, "Style": scalar(mat, "MarkStyle", 0., 100)}
    if timed:
        inputs.update({"Clock": EDIT.create_material_expression(mat, u.MaterialExpressionTime, -600, 200),
                       "Start": scalar(mat, "StartTime", 0., 300), "Duration": scalar(mat, "Lifetime", 1., 400),
                       "Contact": scalar(mat, "Contact", 0., 500)})
    else:
        inputs["Brightness"] = scalar(mat, "RingBrightness", 1., 200)
    layer = shader(mat, source, inputs, u.CustomMaterialOutputType.CMOT_FLOAT4, -250, -100)
    rgb = EDIT.create_material_expression(mat, u.MaterialExpressionComponentMask, 20, -180)
    rgb.set_editor_properties(dict(r=True, g=True, b=True, a=False))
    rgb_input = "Input"
    wire(layer, rgb, rgb_input)
    light = EDIT.create_material_expression(mat, u.MaterialExpressionMultiply, 250, 30)
    light.set_editor_property("const_b", .78 if timed else .33)
    wire(rgb, light, "A")
    opacity = shader(mat, GROUND_MASK, {"Layer": layer, "World": world}, u.CustomMaterialOutputType.CMOT_FLOAT1, 160, 320)
    assert EDIT.connect_material_property(rgb, "", u.MaterialProperty.MP_BASE_COLOR)
    assert EDIT.connect_material_property(light, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
    assert EDIT.connect_material_property(opacity, "", u.MaterialProperty.MP_OPACITY)
    EDIT.recompile_material(mat)
    assert ASSETS.save_loaded_asset(mat, False), name


if __name__ == "__main__":
    decal("M_CombatFootprint", FOOTPRINT)
    decal("M_CombatReach", REACH)
    decal("M_CombatDanger", DANGER, timed=True)
    u.log("COMBAT_GROUND_DECALS_SAVED")

