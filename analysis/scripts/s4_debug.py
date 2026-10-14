# Full dump of one draw (pipeline, shaders, descriptors, UBO contents) + Debug Pixel trace + shader export.
# Cases are read from analysis/s4_cases.txt: "<tag> <x> <y> <target-tex-suffix> [<draw eid>|-]".
# Writes analysis/<tag>_*.txt and analysis/shaders/*.
import sys, os, math, subprocess
sys.path.insert(0, os.environ.get("DOOMFIX_SCRIPTS", "."))  # set by analysis/run_analysis.ps1
import renderdoc as rd
from rdcommon import *

SUB = rd.Subresource(0, 0, 0)
SPIRV_DIR = os.path.join(RENDERDOC_DIR, "plugins", "spirv")
SHADER_DIR = os.path.join(OUTDIR, "shaders")
CASES = [("weapon_dark", 1437, 917, "53374", 15976)]
_cf = os.path.join(OUTDIR, "s4_cases.txt")
if os.path.exists(_cf):
    CASES = []
    for line in open(_cf):
        p = line.split()
        if len(p) >= 4 and not line.startswith("#"):
            CASES.append((p[0], int(p[1]), int(p[2]), p[3], int(p[4]) if len(p) > 4 and p[4] != "-" else None))


def fmt_num(x):
    if isinstance(x, float):
        if math.isnan(x): return "NaN"
        if math.isinf(x): return "+Inf" if x > 0 else "-Inf"
        return "%.6g" % x
    return str(x)


def var_vals(v):
    n = max(1, v.rows) * max(1, v.columns)
    t = v.type
    if t in (rd.VarType.Float, rd.VarType.Half):
        return list(v.value.f32v[:n])
    if t == rd.VarType.Double:
        return list(v.value.f64v[:n])
    if t in (rd.VarType.SInt, rd.VarType.SShort, rd.VarType.SByte, rd.VarType.SLong):
        return list(v.value.s32v[:n])
    if t == rd.VarType.Bool:
        return [bool(x) for x in v.value.u32v[:n]]
    return list(v.value.u32v[:n])


def var_str(v, indent="", out=None):
    if out is None: out = []
    if len(v.members):
        out.append("%s%s:" % (indent, v.name))
        for m in v.members:
            var_str(m, indent + "  ", out)
    else:
        vals = var_vals(v)
        tname = str(v.type).split(".")[-1]
        flag = ""
        if any(isinstance(x, float) and (math.isnan(x) or math.isinf(x)) for x in vals): flag = "   <-- NaN/Inf"
        out.append("%s%s [%s %dx%d] = %s%s" % (indent, v.name, tname, v.rows, v.columns, ", ".join(fmt_num(x) for x in vals), flag))
    return out


def var_str_diff(a, b):
    la, lb = [], []
    for v in a: var_str(v, "        ", la)
    for v in b: var_str(v, "        ", lb)
    return [line + ("" if i < len(la) and la[i] == line else "   <-- differs (beyond bound range)") for i, line in enumerate(lb)]


def tex_desc(texs, bufs, names, d):
    rid = d.resource
    s = "%s (id %d)" % (names.get(rid, "?"), int(rid))
    if rid in texs:
        t = texs[rid]
        s += " | %s %dx%dx%d arr=%d mips=%d fmt=%s" % (str(t.type).split(".")[-1], t.width, t.height, t.depth, t.arraysize, t.mips, t.format.Name())
        if d.format.Name() not in ("Unknown", t.format.Name()):
            s += " viewfmt=%s" % d.format.Name()
        s += " view mips %d..+%d" % (d.firstMip, d.numMips)
    elif rid in bufs:
        b = bufs[rid]
        s += " | buffer len=%d off=%d range=%d" % (b.length, d.byteOffset, d.byteSize)
        if d.format.Name() != "Unknown": s += " texelfmt=%s" % d.format.Name()
    return s


def safe(f, default="?"):
    try:
        return f()
    except Exception as e:
        return "%s(%s)" % (default, e)


def dump_draw(ctrl, log, tag, eid):
    sf = ctrl.GetStructuredFile()
    names = res_names(ctrl); texs = tex_map(ctrl); bufs = buf_map(ctrl)
    ctrl.SetFrameEvent(eid, True)
    st = ctrl.GetPipelineState()
    vk = ctrl.GetVulkanPipelineState()
    act = get_action(ctrl, eid)
    out = open(os.path.join(OUTDIR, "%s_draw%d_state.txt" % (tag, eid)), "w", encoding="utf-8")
    w = lambda *a: out.write(" ".join(str(x) for x in a) + "\n")
    w("Event %d: %s  indices=%d instances=%d indexOffset=%d baseVertex=%d" % (eid, act.GetName(sf), act.numIndices, act.numInstances, act.indexOffset, act.baseVertex))
    gp = vk.graphics
    w("Graphics pipeline: %s (id %d)" % (names.get(gp.pipelineResourceId, "?"), int(gp.pipelineResourceId)))
    w("Render pass: %s  framebuffer: %s" % (safe(lambda: names.get(vk.currentPass.renderpass.resourceId, "?")), safe(lambda: names.get(vk.currentPass.framebuffer.resourceId, "?"))))
    for i, o in enumerate(st.GetOutputTargets()):
        if o.resource != rd.ResourceId.Null():
            w("  colour[%d]: %s" % (i, tex_desc(texs, bufs, names, o)))
    w("  depth   : %s" % tex_desc(texs, bufs, names, st.GetDepthTarget()))
    w(safe(lambda: "Depth test=%s write=%s func=%s ; stencil=%s" % (vk.depthStencil.depthTestEnable, vk.depthStencil.depthWriteEnable, vk.depthStencil.depthFunction, vk.depthStencil.stencilTestEnable)))
    w(safe(lambda: "Cull=%s frontCCW=%s depthBias=%s" % (vk.rasterizer.cullMode, vk.rasterizer.frontCCW, vk.rasterizer.depthBias)))
    for i, b in enumerate(safe(lambda: vk.colorBlend.blends, [])):
        w(safe(lambda: "Blend[%d]: enable=%s color=%s/%s/%s alpha=%s/%s/%s writeMask=%x" % (i, b.enabled, b.colorBlend.source, b.colorBlend.destination, b.colorBlend.operation,
          b.alphaBlend.source, b.alphaBlend.destination, b.alphaBlend.operation, b.writeMask)))
    issues = []
    shaders = {}
    pipe = st.GetGraphicsPipelineObject()
    for stage in [rd.ShaderStage.Vertex, rd.ShaderStage.Pixel]:
        sh = st.GetShader(stage)
        refl = st.GetShaderReflection(stage)
        if refl is None: continue
        shaders[stage] = (sh, refl)
        sn = str(stage).split(".")[-1]
        w("\n================ %s shader %s (id %d) entry=%s encoding=%s debugInfo files=%d ================" % (
            sn, names.get(sh, "?"), int(sh), refl.entryPoint, refl.encoding, len(refl.debugInfo.files)))
        w("Inputs:")
        for s in refl.inputSignature:
            w("   loc%d %s %s x%d" % (s.regIndex, s.varName, s.varType, s.compCount))
        w("Outputs:")
        for s in refl.outputSignature:
            w("   loc%d %s %s x%d" % (s.regIndex, s.varName, s.varType, s.compCount))
        w("-- Read-only resources (dynamically used):")
        for u in st.GetReadOnlyResources(stage, True):
            r = refl.readOnlyResources[u.access.index] if u.access.index < len(refl.readOnlyResources) else None
            w("   set%s b%s [%d] %-28s %s" % (r.fixedBindSetOrSpace if r else "?", r.fixedBindNumber if r else "?", u.access.arrayElement,
              r.name if r else "?", tex_desc(texs, bufs, names, u.descriptor)))
        w("-- Read-write resources:")
        for u in st.GetReadWriteResources(stage, True):
            r = refl.readWriteResources[u.access.index] if u.access.index < len(refl.readWriteResources) else None
            w("   set%s b%s [%d] %-28s %s" % (r.fixedBindSetOrSpace if r else "?", r.fixedBindNumber if r else "?", u.access.arrayElement,
              r.name if r else "?", tex_desc(texs, bufs, names, u.descriptor)))
        w("-- Samplers:")
        for u in st.GetSamplers(stage, True):
            r = refl.samplers[u.access.index] if u.access.index < len(refl.samplers) else None
            sd = u.sampler
            w(safe(lambda: "   set%s b%s %-20s filter=%s addr=%s/%s/%s compare=%s lod %.2f..%.2f bias %.2f aniso %.1f" % (
                r.fixedBindSetOrSpace if r else "?", r.fixedBindNumber if r else "?", r.name if r else "?", sd.filter.minify, sd.addressU, sd.addressV, sd.addressW,
                sd.compareFunction, sd.minLOD, sd.maxLOD, sd.mipBias, sd.maxAnisotropy)))
        w("-- Constant blocks:")
        for u in st.GetConstantBlocks(stage, False):
            ci = u.access.index
            if ci >= len(refl.constantBlocks): continue
            cb = refl.constantBlocks[ci]
            d = u.descriptor
            need = cb.byteSize
            if cb.bufferBacked:
                prov = d.byteSize
                blen = bufs[d.resource].length if d.resource in bufs else 0
                hdr = "   [%d] %s set%d b%d: buffer %s off=%d range=%d (VkBuffer len %d), shader needs %d bytes, %d vars" % (
                    ci, cb.name, cb.fixedBindSetOrSpace, cb.fixedBindNumber, names.get(d.resource, "?"), d.byteOffset, prov, blen, need, len(cb.variables))
                if prov < need:
                    hdr += "   *** %d bytes needed, %d provided ***" % (need, prov)
                    issues.append("%s %s: %s (set%d b%d) %d bytes needed, %d provided (VkBuffer len %d, offset %d)" % (
                        sn, refl.entryPoint, cb.name, cb.fixedBindSetOrSpace, cb.fixedBindNumber, need, prov, blen, d.byteOffset))
                w(hdr)
                vars_ = ctrl.GetCBufferVariableContents(pipe, sh, stage, refl.entryPoint, ci, d.resource, d.byteOffset, prov)
                for v in vars_:
                    for line in var_str(v, "        "): w(line)
                if prov < need:
                    avail = max(0, min(need, blen - d.byteOffset))
                    w("      -- same block read with the shader's full size (%d bytes, %d available in the VkBuffer) --" % (need, avail))
                    vars2 = ctrl.GetCBufferVariableContents(pipe, sh, stage, refl.entryPoint, ci, d.resource, d.byteOffset, avail)
                    for line in var_str_diff(vars_, vars2): w(line)
            else:
                w("   [%d] %s (push/spec constants), %d bytes, %d vars" % (ci, cb.name, need, len(cb.variables)))
                vars_ = ctrl.GetCBufferVariableContents(pipe, sh, stage, refl.entryPoint, ci, rd.ResourceId.Null(), 0, 0)
                for v in vars_:
                    for line in var_str(v, "        "): w(line)
    w("\n================ ISSUES ================")
    for i in issues: w("  " + i)
    out.close()
    log("[%s] draw %d dumped; issues: %s" % (tag, eid, issues))
    return shaders, issues


def export_shader(ctrl, log, sh, refl, pipe):
    os.makedirs(SHADER_DIR, exist_ok=True)
    kind = {rd.ShaderStage.Pixel: "frag", rd.ShaderStage.Vertex: "vert"}.get(refl.stage, "shader")
    base = os.path.join(SHADER_DIR, "%s_%d" % (kind, int(sh)))
    if os.path.exists(base + ".spv"):
        return base
    with open(base + ".spv", "wb") as f:
        f.write(bytes(refl.rawBytes))
    targets = ctrl.GetDisassemblyTargets(True)
    with open(base + ".rd_disasm.txt", "w", encoding="utf-8") as f:
        f.write(ctrl.DisassembleShader(pipe, refl, targets[0]))
    for args in [["spirv-dis.exe", base + ".spv", "-o", base + ".spvasm"],
                 ["spirv-cross.exe", base + ".spv", "--vulkan-semantics", "--output", base + ".glsl"],
                 ["spirv-cross.exe", base + ".spv", "--hlsl", "--shader-model", "50", "--output", base + ".hlsl"]]:
        args[0] = os.path.join(SPIRV_DIR, args[0])
        try:
            r = subprocess.run(args, capture_output=True, text=True, timeout=300)
            log("  %s rc=%d %s" % (os.path.basename(args[0]), r.returncode, r.stderr[:300]))
        except Exception as e:
            log("  %s failed: %s" % (args[0], e))
    log("exported shader", base, "targets:", list(targets))
    return base


def debug_pixel(ctrl, log, tag, eid, x, y, sh, refl, pipe):
    ctrl.SetFrameEvent(eid, True)
    inp = rd.DebugPixelInputs()
    trace = ctrl.DebugPixel(x, y, inp)
    path = os.path.join(OUTDIR, "%s_draw%d_px%d_%d_trace.txt" % (tag, eid, x, y))
    out = open(path, "w", encoding="utf-8")
    w = lambda *a: out.write(" ".join(str(z) for z in a) + "\n")
    if trace is None or trace.debugger is None:
        w("DebugPixel failed (no trace)")
        out.close(); log("[%s] debug failed" % tag)
        return None
    disasm = ctrl.DisassembleShader(pipe, refl, ctrl.GetDisassemblyTargets(True)[0]).split("\n")
    inst2line = {ii.instruction: ii.lineInfo.disassemblyLine for ii in trace.instInfo}
    w("# Debug Pixel event %d pixel (%d,%d)" % (eid, x, y))
    w("## inputs")
    for v in trace.inputs:
        for l in var_str(v, "  "): w(l)
    w("## constant blocks as seen by the debugger")
    for v in trace.constantBlocks:
        for l in var_str(v, "  "): w(l)
    w("## source variable mapping (%d)" % len(trace.sourceVars))
    for sv in trace.sourceVars[:2000]:
        w("  %s -> %s" % (sv.name, ", ".join("%s[%d]" % (r.name, r.component) for r in sv.variables)))
    w("## steps  (lines '      x = ...' are the results of the instruction printed above them)")
    nsteps = 0
    nan_steps = []
    while True:
        states = ctrl.ContinueDebug(trace.debugger)
        if not states: break
        for s in states:
            nsteps += 1
            for c in s.changes:
                txt = "; ".join(var_str(c.after))
                w("      %s" % txt)
                if "NaN" in txt or "Inf" in txt: nan_steps.append((s.stepIndex, txt[:200]))
            ln = inst2line.get(s.nextInstruction, -1)
            src = disasm[ln - 1].strip() if 0 < ln <= len(disasm) else "?"
            w("[%5d] inst %4d line %5d | %s" % (s.stepIndex, s.nextInstruction, ln, src))
    w("## total steps %d" % nsteps)
    w("## NaN/Inf occurrences: %d" % len(nan_steps))
    for n in nan_steps[:300]: w("  step %d: %s" % n)
    out.close()
    ctrl.FreeTrace(trace)
    log("[%s] debug trace %d steps -> %s ; NaN/Inf=%d" % (tag, nsteps, path, len(nan_steps)))
    return path


def find_draw(ctrl, log, names, suffix, x, y):
    tgt = [r for r, n in names.items() if n.endswith(" " + suffix)][0]
    hist = ctrl.PixelHistory(tgt, x, y, SUB, rd.CompType.Typeless)
    best = None
    for m in hist:
        a = get_action(ctrl, m.eventId)
        if a and (a.flags & rd.ActionFlags.Drawcall) and m.Passed() and not m.directShaderWrite:
            best = m
        log("   hist eid %d passed=%s out=%s post=%s" % (m.eventId, m.Passed(), list(m.shaderOut.col.floatValue[:3]), list(m.postMod.col.floatValue[:3])))
    return best.eventId if best else None


ACTS = {}


def get_action(ctrl, eid):
    if not ACTS:
        for a, p in flatten(ctrl.GetRootActions()):
            ACTS[a.eventId] = a
    return ACTS.get(eid)


def body(ctrl, log):
    names = res_names(ctrl)
    for tag, x, y, suffix, eid in CASES:
        log("=== case %s (%d,%d) on %s" % (tag, x, y, suffix))
        if eid is None:
            eid = find_draw(ctrl, log, names, suffix, x, y)
            log("   last passing draw:", eid)
            if eid is None: continue
        shaders, issues = dump_draw(ctrl, log, tag, eid)
        ctrl.SetFrameEvent(eid, True)
        pipe = ctrl.GetPipelineState().GetGraphicsPipelineObject()
        for stage, (sh, refl) in shaders.items():
            export_shader(ctrl, log, sh, refl, pipe)
        sh, refl = shaders[rd.ShaderStage.Pixel]
        debug_pixel(ctrl, log, tag, eid, x, y, sh, refl, pipe)

if not globals().get("_IMPORT_ONLY"):
    run("s4_debug.log", body)
