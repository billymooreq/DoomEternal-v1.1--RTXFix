# For every distinct (shader, uniform block) bound with range < block size: which members lie beyond the
# bound range, and does the shader actually reference them? Exports SPIR-V + spirv-cross GLSL per shader.
import sys, os, csv, subprocess, re
sys.path.insert(0, r"C:\Users\vippr\Documents\doomfix\analysis\scripts")
import renderdoc as rd
from rdcommon import *

SPIRV_DIR = r"C:\Program Files\RenderDoc\plugins\spirv"
DIR = os.path.join(OUTDIR, "shaders", "ubo_cases")
STAGE = {"Vertex": rd.ShaderStage.Vertex, "Pixel": rd.ShaderStage.Pixel, "Compute": rd.ShaderStage.Compute}


def flat_vars(vs, base=0, prefix=""):
    for v in vs:
        off = base + v.byteOffset
        nm = prefix + v.name
        if len(v.type.members):
            yield from flat_vars(v.type.members, off, nm + ".")
        else:
            yield nm, v.name, off, v.type.arrayByteStride * max(1, v.type.elements) if v.type.elements > 1 else max(4, v.type.rows * v.type.columns * 4)


def body(ctrl, log):
    os.makedirs(DIR, exist_ok=True)
    rows = list(csv.DictReader(open(os.path.join(OUTDIR, "ubo_scan.csv"))))
    seen = {}
    for r in rows:
        k = (r["stage"], r["shader"], r["block"])
        if k not in seen: seen[k] = r
    out = open(os.path.join(OUTDIR, "ubo_cases.md"), "w", encoding="utf-8")
    out.write("| first EID | stage | shader | block | needed | provided | members beyond bound range | referenced in shader code? |\n|---|---|---|---|---|---|---|---|\n")
    for (stage, sh, block), r in sorted(seen.items(), key=lambda kv: int(kv[1]["eid"])):
        eid = int(r["eid"])
        ctrl.SetFrameEvent(eid, False)
        st = ctrl.GetPipelineState()
        refl = st.GetShaderReflection(STAGE[stage])
        cb = [c for c in refl.constantBlocks if c.name == block][0]
        prov = int(r["provided"])
        beyond = [(n, leaf, o, s) for n, leaf, o, s in flat_vars(cb.variables) if o + s > prov]
        spv = os.path.join(DIR, "%s_%s.spv" % (stage.lower(), sh))
        glsl = spv[:-4] + ".glsl"
        if not os.path.exists(glsl):
            open(spv, "wb").write(bytes(refl.rawBytes))
            subprocess.run([os.path.join(SPIRV_DIR, "spirv-cross.exe"), spv, "--vulkan-semantics", "--output", glsl], capture_output=True, timeout=300)
        code = open(glsl, encoding="utf-8", errors="replace").read() if os.path.exists(glsl) else ""
        # drop struct declarations, keep function bodies
        body_code = code[code.find("void main()"):] if "void main()" in code else code
        used = []
        for n, leaf, o, s in beyond:
            hit = re.search(r"\.%s\b" % re.escape(leaf), body_code) is not None
            used.append("%s@%d%s" % (n, o, " **USED**" if hit else ""))
        anyused = any("USED" in u for u in used)
        out.write("| %d | %s | %s | %s | %s | %d | %s | %s |\n" % (eid, stage, sh, block, r["needed"], prov, ", ".join(used), "**YES**" if anyused else "no"))
        log(eid, stage, sh, block, r["needed"], prov, used)
    out.close()

run("s12_ubo_fields.log", body)
