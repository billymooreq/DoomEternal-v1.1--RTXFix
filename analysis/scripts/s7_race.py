# 1) who reads/writes the light-list buffer / ambient atlases / light UBOs during the frame
# 2) contents of the cluster light-list word at several events
# 3) repeatability of GPU results of the weapon draw (original shader, and instrumented variants)
# 4) Debug Vertex for the triangle under the weapon pixel + GPU post-VS SH outputs
import sys, os, struct
sys.path.insert(0, r"C:\Users\vippr\Documents\doomfix\analysis\scripts")
import renderdoc as rd
from rdcommon import *
g = {"_IMPORT_ONLY": True, "__file__": r"C:\Users\vippr\Documents\doomfix\analysis\scripts\s4_debug.py"}
exec(open(g["__file__"]).read(), g)
var_str = g["var_str"]

SUB = rd.Subresource(0, 0, 0)
DRAW, PRIM, X, Y = 15976, 3081, 1437, 917
WATCH = [1753, 201672, 201675, 1062, 1063, 1060, 1061, 59303, 345131, 345270, 259, 1993]
LIGHTLIST_OFF, PROBELIST_OFF, CLUSTER = 384000, 1152000, 2736


def body(ctrl, log):
    sf = ctrl.GetStructuredFile()
    names = res_names(ctrl)
    byid = {int(r): r for r in names}
    acts = {a.eventId: a for a, p in flatten(ctrl.GetRootActions())}
    log("== resource usage over the frame ==")
    for rid in WATCH:
        r = byid.get(rid)
        if r is None: continue
        us = ctrl.GetUsage(r)
        kinds = {}
        for u in us:
            kinds.setdefault(str(u.usage).split(".")[-1], []).append(u.eventId)
        log("%s (%d): %d usages" % (names[r], rid, len(us)))
        for k, v in kinds.items():
            if any(w in k for w in ("RW", "Copy", "Clear", "Resolve", "GenMips", "Discard", "CPUWrite", "Barrier")) or len(v) <= 6:
                log("    %-22s n=%-4d %s%s" % (k, len(v), v[:40], " ..." if len(v) > 40 else ""))
            else:
                log("    %-22s n=%-4d first=%d last=%d" % (k, len(v), v[0], v[-1]))
    log("\n== light/probe list word for cluster %d at various events ==" % CLUSTER)
    buf = byid[1753]
    for eid in [1, 15947, 15975, 15976, 18974, 27838, 27839, 27840, 29213]:
        ctrl.SetFrameEvent(eid, True)
        lw = struct.unpack("<5I", ctrl.GetBufferData(buf, LIGHTLIST_OFF + CLUSTER * 64 * 4, 20))
        pw = struct.unpack("<2I", ctrl.GetBufferData(buf, PROBELIST_OFF + CLUSTER * 64 * 4, 8))
        log("  after eid %5d: lightlist[%d*64..] = %s   probelist hash = %s" % (eid, CLUSTER, lw, pw))

    log("\n== repeatability of the weapon draw output at (%d,%d) ==" % (X, Y))
    tgt = [r for r, n in names.items() if n.endswith(" 53374")][0]
    ps = byid[15507]
    src = open(os.path.join(OUTDIR, "shaders", "frag_15507.glsl"), encoding="utf-8").read()
    final = "    out_FragColor0 = vec4(inputs.output_lighting, 0.0);"
    variants = [("original SPIR-V", None),
                ("numLights", src.replace(final, "    out_FragColor0 = vec4(float(numLights), float(lightHash & 255u), 0.25, 0.0);")),
                ("output_x1000", src.replace(final, "    out_FragColor0 = vec4(abs(inputs.output_lighting) * 1000.0, 0.0);")),
                ("recompiled_unmodified", src)]
    for name, code in variants:
        sid = None
        if code is not None:
            sid, err = ctrl.BuildTargetShader("main", rd.ShaderEncoding.GLSL, code.encode("utf-8"), rd.ShaderCompileFlags(), rd.ShaderStage.Pixel)
            ctrl.ReplaceResource(ps, sid)
        vals = []
        for it in range(4):
            ctrl.SetFrameEvent(DRAW - 1 if it % 2 else 29213, True)   # move away so the next call replays again
            ctrl.SetFrameEvent(DRAW, True)
            vals.append(tuple(ctrl.PickPixel(tgt, X, Y, SUB, rd.CompType.Typeless).floatValue[:3]))
        if sid is not None:
            ctrl.RemoveReplacement(ps); ctrl.FreeTargetResource(sid)
        log("  %-24s %s" % (name, "  ".join("(%.5g %.5g %.5g)" % v for v in vals)))

    log("\n== Debug Vertex for primitive %d ==" % PRIM)
    ctrl.SetFrameEvent(DRAW, True)
    st = ctrl.GetPipelineState()
    ib = st.GetIBuffer()
    a = acts[DRAW]
    stride = ib.byteStride if ib.byteStride else 4
    fmt = {2: "<3H", 4: "<3I"}[stride]
    idxs = struct.unpack(fmt, ctrl.GetBufferData(ib.resourceId, ib.byteOffset + (a.indexOffset + PRIM * 3) * stride, 3 * stride))
    log("index buffer %s stride %d -> vertex indices %s (baseVertex %d)" % (names.get(ib.resourceId), stride, idxs, a.baseVertex))
    refl = st.GetShaderReflection(rd.ShaderStage.Vertex)
    pipe = st.GetGraphicsPipelineObject()
    disasm = ctrl.DisassembleShader(pipe, refl, ctrl.GetDisassemblyTargets(True)[0]).split("\n")
    # GPU post-VS outputs
    mf = ctrl.GetPostVSData(0, 0, rd.MeshDataStage.VSOut)
    log("postVS: vb=%d off=%d stride=%d ib=%d ibstride=%d numIndices=%d" % (int(mf.vertexResourceId), mf.vertexByteOffset, mf.vertexByteStride, int(mf.indexResourceId), mf.indexByteStride, mf.numIndices))
    layout, off = [], 0
    for s in refl.outputSignature:
        layout.append((s.varName, off, s.compCount)); off += 4 * s.compCount
    log("assumed postVS layout:", layout, "total", off)
    for k, vi in enumerate(idxs):
        slot = PRIM * 3 + k
        if mf.indexResourceId != rd.ResourceId.Null():
            isz = mf.indexByteStride or 4
            slot = struct.unpack("<I" if isz == 4 else "<H", ctrl.GetBufferData(mf.indexResourceId, mf.indexByteOffset + (PRIM * 3 + k) * isz, isz))[0]
        raw = ctrl.GetBufferData(mf.vertexResourceId, mf.vertexByteOffset + slot * mf.vertexByteStride, mf.vertexByteStride)
        outs = {}
        for nm, o, c in layout:
            if o + 4 * c <= len(raw):
                outs[nm] = struct.unpack_from("<%df" % c, raw, o)
        log("  GPU VS out, vertex %d: pos=%s SH0=%s SH1=%s SH2=%s" % (vi, outs.get("gl_Position"), outs.get("vofi_TexCoord8"), outs.get("vofi_TexCoord9"), outs.get("vofi_TexCoord10")))
        trace = ctrl.DebugVertex(vi + a.baseVertex, 0, a.indexOffset + PRIM * 3 + k, 0)
        if trace is None or trace.debugger is None:
            log("  DebugVertex failed for", vi); continue
        inst2line = {ii.instruction: ii.lineInfo.disassemblyLine for ii in trace.instInfo}
        path = os.path.join(OUTDIR, "weapon_vs_draw%d_vtx%d_trace.txt" % (DRAW, vi))
        f = open(path, "w", encoding="utf-8")
        f.write("# Debug Vertex event %d vertex %d (index slot %d)\n## inputs\n" % (DRAW, vi, a.indexOffset + PRIM * 3 + k))
        for v in trace.inputs:
            for l in var_str(v, "  "): f.write(l + "\n")
        f.write("## steps\n")
        n = 0
        while True:
            states = ctrl.ContinueDebug(trace.debugger)
            if not states: break
            for s in states:
                n += 1
                ch = [c.after for c in s.changes if c.after.name and len(s.changes) <= 12]
                for v in ch:
                    f.write("        => " + "; ".join(var_str(v)) + "\n")
                ln = inst2line.get(s.nextInstruction, -1)
                f.write("[%5d] inst %4d line %5d | %s\n" % (s.stepIndex, s.nextInstruction, ln, disasm[ln - 1].strip() if 0 < ln <= len(disasm) else "?"))
        f.close()
        ctrl.FreeTrace(trace)
        log("  Debug Vertex %d: %d steps -> %s" % (vi, n, path))

run("s7_race.log", body)
