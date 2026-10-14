# Replay-only shader replacement experiments (nothing is written to the capture).
# The weapon fragment shader (spirv-cross GLSL) is recompiled by RenderDoc's glslang with small
# instrumentation edits; each variant writes an intermediate value to the colour target, which is then
# read back on the GPU at the weapon pixels.
import sys, os, struct
sys.path.insert(0, os.environ.get("DOOMFIX_SCRIPTS", "."))  # set by analysis/run_analysis.ps1
import renderdoc as rd
from rdcommon import *

SUB = rd.Subresource(0, 0, 0)
DRAW = int(os.environ.get("DOOMFIX_DRAW", "15976"))
PS_ID = int(os.environ.get("DOOMFIX_PS", "15507"))
GLSL = os.path.join(OUTDIR, "shaders", "frag_%d.glsl" % PS_ID)
TARGET = "53374"
FINAL = "    out_FragColor0 = vec4(inputs.output_lighting, 0.0);"
INIT = "    lightingInput_t inputs = defaultLightingInput();"
LOOP_DIFFUSE = "        inputs.output_lighting += ((diffuse_light_color * NdotL_2) * inputs.albedo);"
SHADOW_TEST = "        if (shadow <= clip_min_1)"


def out(expr):
    return lambda s: s.replace(FINAL, "    out_FragColor0 = vec4(%s, 0.0);" % expr)


def loop_instr(expr):
    def f(s):
        s = s.replace(INIT, INIT + "\n    vec3 dbgA = vec3(0.0); vec3 dbgB = vec3(0.0);")
        s = s.replace(LOOP_DIFFUSE, LOOP_DIFFUSE + "\n        dbgA += light_color_final_1; dbgB.x += 1.0;")
        s = s.replace("        SPIRV_CROSS_BRANCH\n" + SHADOW_TEST, "        dbgB.y += 1.0; dbgB.z += shadow;\n        SPIRV_CROSS_BRANCH\n" + SHADOW_TEST)
        return s.replace(FINAL, "    out_FragColor0 = vec4(%s, 0.0);" % expr)
    return f


VARIANTS = [
    ("const_red", out("vec3(1.0, 0.0, 0.0)")),
    ("recompiled_unmodified", lambda s: s),
    ("albedo", out("inputs.albedo")),
    ("specular", out("inputs.specular")),
    ("abs_normal", out("abs(inputs.normal)")),
    ("ambientSH0_abs(FS input)", out("abs(vofi_TexCoord8.xyz)")),
    ("ambientOcclusion", out("vec3(inputs.ambientOcclusion)")),
    ("numLights,clusterOffset/1000,modelLightingFlags", out("vec3(float(numLights), float(clusterOffset_1) / 1000.0, float(modelLightingFlags))")),
    ("output_x1000", out("abs(inputs.output_lighting) * 1000.0")),
    ("negative_part_x1000", out("max(-inputs.output_lighting, vec3(0.0)) * 1000.0")),
    ("isnan/isinf flags", out("vec3(any(isnan(inputs.output_lighting)) ? 1.0 : 0.0, any(isinf(inputs.output_lighting)) ? 1.0 : 0.0, 0.5)")),
    ("loop: sum light_color_final x100", loop_instr("dbgA * 100.0")),
    ("loop: lights lit, lights reaching shadow test, sum shadow", loop_instr("dbgB")),
]


def body(ctrl, log):
    names = res_names(ctrl)
    tgt = [r for r, n in names.items() if n.endswith(" " + TARGET)][0]
    ps = [r for r in names if int(r) == PS_ID][0]
    src = open(GLSL, encoding="utf-8").read()
    for marker in (FINAL, INIT, LOOP_DIFFUSE, SHADOW_TEST):
        if marker not in src:
            log("WARNING marker missing:", marker.strip())
    mask = open(os.path.join(OUTDIR, "draw%d_mask.bin" % DRAW), "rb").read() if os.path.exists(os.path.join(OUTDIR, "draw%d_mask.bin" % DRAW)) else None
    pts = [(1437, 917)]
    if mask:
        for (x, y) in [(1500, 1000), (1600, 1060), (1380, 1050), (1460, 960), (1900, 1075)]:
            if mask[y * 1920 + x] == 1: pts.append((x, y))
    log("pixels:", pts)
    log("encodings:", [str(e) for e in ctrl.GetTargetShaderEncodings()])
    ctrl.SetFrameEvent(DRAW, True)
    base = [list(ctrl.PickPixel(tgt, x, y, SUB, rd.CompType.Typeless).floatValue[:3]) for x, y in pts]
    log("%-50s %s" % ("original SPIR-V (no replacement)", "  ".join("(%.5g %.5g %.5g)" % tuple(v) for v in base)))
    res = {}
    for name, fn in VARIANTS:
        code = fn(src)
        sid, err = ctrl.BuildTargetShader("main", rd.ShaderEncoding.GLSL, code.encode("utf-8"), rd.ShaderCompileFlags(), rd.ShaderStage.Pixel)
        if sid == rd.ResourceId.Null():
            log("%-50s BUILD FAILED: %s" % (name, err[:1500])); continue
        ctrl.ReplaceResource(ps, sid)
        ctrl.SetFrameEvent(DRAW, True)
        vals = [list(ctrl.PickPixel(tgt, x, y, SUB, rd.CompType.Typeless).floatValue[:3]) for x, y in pts]
        ctrl.RemoveReplacement(ps)
        ctrl.FreeTargetResource(sid)
        res[name] = vals
        log("%-50s %s" % (name, "  ".join("(%.5g %.5g %.5g)" % tuple(v) for v in vals)))
    ctrl.SetFrameEvent(DRAW, True)

run(os.environ.get("DOOMFIX_LOG", "s6_replace.log"), body)
