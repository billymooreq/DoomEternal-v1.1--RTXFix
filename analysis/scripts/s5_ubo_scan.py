# Scan every draw/dispatch for uniform buffers bound with a range smaller than the shader's block size.
import sys
sys.path.insert(0, r"C:\Users\vippr\Documents\doomfix\analysis\scripts")
import renderdoc as rd
from rdcommon import *

STAGES = [rd.ShaderStage.Vertex, rd.ShaderStage.Hull, rd.ShaderStage.Domain, rd.ShaderStage.Geometry, rd.ShaderStage.Pixel, rd.ShaderStage.Compute]


def body(ctrl, log):
    sf = ctrl.GetStructuredFile()
    flat = flatten(ctrl.GetRootActions())
    acts = [a for a, p in flat if a.flags & (rd.ActionFlags.Drawcall | rd.ActionFlags.Dispatch)]
    log("actions to scan:", len(acts))
    csv = open(OUTDIR + r"\ubo_scan.csv", "w")
    csv.write("eid,kind,pipeline,stage,shader,entry,block,set,binding,needed,provided,buffer,offset\n")
    agg = {}
    psuse = {}
    for a in acts:
        ctrl.SetFrameEvent(a.eventId, False)
        st = ctrl.GetPipelineState()
        comp = bool(a.flags & rd.ActionFlags.Dispatch)
        pipe = st.GetComputePipelineObject() if comp else st.GetGraphicsPipelineObject()
        for stage in ([rd.ShaderStage.Compute] if comp else STAGES[:5]):
            sh = st.GetShader(stage)
            if sh == rd.ResourceId.Null():
                continue
            refl = st.GetShaderReflection(stage)
            if stage == rd.ShaderStage.Pixel:
                psuse.setdefault(int(sh), []).append(a.eventId)
            for u in st.GetConstantBlocks(stage, True):
                idx = u.access.index
                if idx >= len(refl.constantBlocks):
                    continue
                cb = refl.constantBlocks[idx]
                if not cb.bufferBacked:
                    continue
                need = cb.byteSize; prov = u.descriptor.byteSize
                if prov < need:
                    csv.write("%d,%s,%d,%s,%d,%s,%s,%d,%d,%d,%d,%d,%d\n" % (a.eventId, "dispatch" if comp else "draw", int(pipe), str(stage).split(".")[-1],
                              int(sh), refl.entryPoint, cb.name, cb.fixedBindSetOrSpace, cb.fixedBindNumber, need, prov, int(u.descriptor.resource), u.descriptor.byteOffset))
                    k = (str(stage).split(".")[-1], int(sh), cb.name, need, prov)
                    agg.setdefault(k, []).append(a.eventId)
    csv.close()
    log("\n== aggregated (stage, shader, block, needed, provided): count, eids ==")
    for k, v in sorted(agg.items(), key=lambda kv: kv[1][0]):
        log(k, len(v), v[:12], "..." if len(v) > 12 else "")
    log("\n== pixel shader usage (ps id: count, first eids) ==")
    for k, v in sorted(psuse.items(), key=lambda kv: -len(kv[1])):
        log(k, len(v), v[:8])

run("s5_ubo_scan.log", body)
