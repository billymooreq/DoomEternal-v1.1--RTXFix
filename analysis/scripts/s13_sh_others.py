# Do other draws that sample texture_ambientshatlas in the vertex shader get non-zero ambient SH?
import sys, os, struct
sys.path.insert(0, r"C:\Users\vippr\Documents\doomfix\analysis\scripts")
import renderdoc as rd
from rdcommon import *


def body(ctrl, log):
    flat = flatten(ctrl.GetRootActions())
    draws = [a for a, p in flat if (a.flags & rd.ActionFlags.Drawcall) and 15948 < a.eventId < 18974]
    seen_vs = {}
    for a in draws:
        ctrl.SetFrameEvent(a.eventId, False)
        st = ctrl.GetPipelineState()
        vs = int(st.GetShader(rd.ShaderStage.Vertex))
        if vs in seen_vs and len(seen_vs[vs]) >= 3: continue
        refl = st.GetShaderReflection(rd.ShaderStage.Vertex)
        if not any(r.name == "texture_ambientshatlas" for r in refl.readOnlyResources): continue
        sig = [(s.varName, s.compCount) for s in refl.outputSignature]
        if not any(n == "vofi_TexCoord8" for n, c in sig): continue
        seen_vs.setdefault(vs, []).append(a.eventId)
        ctrl.SetFrameEvent(a.eventId, True)
        mf = ctrl.GetPostVSData(0, 0, rd.MeshDataStage.VSOut)
        off = 0; lay = {}
        for n, c in sig:
            lay[n] = (off, c); off += 4 * c
        nv = min(64, mf.numIndices)
        raw = ctrl.GetBufferData(mf.vertexResourceId, mf.vertexByteOffset, mf.vertexByteStride * nv)
        nz = 0; mx = 0.0
        for i in range(nv):
            o, c = lay["vofi_TexCoord8"]
            v = struct.unpack_from("<4f", raw, i * mf.vertexByteStride + o)
            if any(abs(x) > 0 for x in v): nz += 1
            mx = max(mx, max(abs(x) for x in v))
        log("draw %d vs=%d ps=%d: %d/%d vertices with non-zero SH0, max |SH0| = %.4g" % (a.eventId, vs, int(st.GetShader(rd.ShaderStage.Pixel)), nz, nv, mx))

run("s13_sh_others.log", body)
