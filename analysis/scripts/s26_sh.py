# Ambient SH of the weapon: GPU post-VS SH outputs of the triangle under the pixel, Debug Vertex of one
# vertex (octree walk samples, SH key, SH atlas samples), atlas texel contents at the key, atlas checksums.
# Env: DOOMFIX_DRAW, DOOMFIX_PIXEL="x,y", DOOMFIX_TARGET (HDR target id for Pixel History).
import sys, os, struct, hashlib
sys.path.insert(0, r"C:\Users\vippr\Documents\doomfix\analysis\scripts")
import renderdoc as rd
from rdcommon import *

SUB = rd.Subresource(0, 0, 0)
DRAW = int(os.environ["DOOMFIX_DRAW"])
X, Y = [int(v) for v in os.environ.get("DOOMFIX_PIXEL", "1437,917").split(",")]
TARGET = int(os.environ["DOOMFIX_TARGET"])


def half(h):
    return struct.unpack("<e", struct.pack("<H", h))[0]


def fvals(v):
    n = max(1, v.rows) * max(1, v.columns)
    return list(v.value.f32v[:n])


def body(ctrl, log):
    names = res_names(ctrl); texs = tex_map(ctrl)
    acts = {a.eventId: a for a, p in flatten(ctrl.GetRootActions())}
    tgt = [r for r in names if int(r) == TARGET][0]
    prim = None
    for m in ctrl.PixelHistory(tgt, X, Y, SUB, rd.CompType.Typeless):
        if m.eventId == DRAW and m.Passed():
            prim = m.primitiveID
    log("draw %d pixel (%d,%d): primitive %s" % (DRAW, X, Y, prim))
    ctrl.SetFrameEvent(DRAW, True)
    st = ctrl.GetPipelineState()
    refl = st.GetShaderReflection(rd.ShaderStage.Vertex)
    a = acts[DRAW]
    # 3D atlases bound to the VS
    atl = []
    for u in st.GetReadOnlyResources(rd.ShaderStage.Vertex, True):
        t = texs.get(u.descriptor.resource)
        if t and t.type == rd.TextureType.Texture3D:
            atl.append((u.descriptor.resource, t))
            data = ctrl.GetTextureData(u.descriptor.resource, SUB)
            log("VS 3D texture %s %dx%dx%d %s md5=%s nonzero=%.1f%%" % (names[u.descriptor.resource], t.width, t.height, t.depth,
                t.format.Name(), hashlib.md5(data).hexdigest(), 100.0 * (len(data) - data.count(0)) / len(data)))
    vs_ubo = []
    pipe = st.GetGraphicsPipelineObject()
    sh = st.GetShader(rd.ShaderStage.Vertex)
    # SH outputs = vec4 outputs at locations 10, 11, 12
    sig = [(s.varName, s.regIndex, s.compCount) for s in refl.outputSignature]
    shouts = [s for s in sig if s[1] in (10, 11, 12) and s[2] == 4]
    log("VS outputs at loc 10-12:", shouts)
    ib = st.GetIBuffer()
    stride = ib.byteStride or 2
    idxs = struct.unpack({2: "<3H", 4: "<3I"}[stride], ctrl.GetBufferData(ib.resourceId, ib.byteOffset + (a.indexOffset + prim * 3) * stride, 3 * stride))
    mf = ctrl.GetPostVSData(0, 0, rd.MeshDataStage.VSOut)
    off = 0; lay = {}
    for nm, reg, c in sig:
        lay[nm] = (off, c); off += 4 * c
    for k, vi in enumerate(idxs):
        isz = mf.indexByteStride or 4
        slot = struct.unpack("<I" if isz == 4 else "<H", ctrl.GetBufferData(mf.indexResourceId, mf.indexByteOffset + (prim * 3 + k) * isz, isz))[0]
        raw = ctrl.GetBufferData(mf.vertexResourceId, mf.vertexByteOffset + slot * mf.vertexByteStride, mf.vertexByteStride)
        vals = [struct.unpack_from("<4f", raw, lay[nm][0]) for nm, reg, c in shouts]
        allv = {nm: tuple(round(x, 5) for x in struct.unpack_from("<%df" % c, raw, lay[nm][0])) for nm, reg, c in sig}
        log("  GPU post-VS vertex %d: SH = %s" % (vi, [tuple(round(x, 5) for x in v) for v in vals]))
        log("      all outputs: %s" % allv)
    # Debug Vertex: collect every 3D sample result in order
    trace = ctrl.DebugVertex(idxs[0] + a.baseVertex, 0, a.indexOffset + prim * 3, 0)
    disasm = ctrl.DisassembleShader(pipe, refl, ctrl.GetDisassemblyTargets(True)[0]).split("\n")
    inst2line = {ii.instruction: ii.lineInfo.disassemblyLine for ii in trace.instInfo}
    samples = []
    outnames = set(nm for nm, reg, c in sig) | {"gl_Position"}
    emu_out = {}
    pending = None
    final_out = {}
    while True:
        states = ctrl.ContinueDebug(trace.debugger)
        if not states: break
        for s in states:
            for c in s.changes:
                v = c.after
                if v.name in outnames:
                    emu_out[v.name] = tuple(round(x, 5) for x in fvals(v))
                for m in v.members:
                    if m.name in outnames:
                        emu_out[m.name] = tuple(round(x, 5) for x in fvals(m))
            if pending is not None:
                for c in s.changes:
                    if c.after.name and c.after.rows * c.after.columns == 4:
                        samples.append(fvals(c.after)); break
                pending = None
            ln = inst2line.get(s.nextInstruction, -1)
            src = disasm[ln - 1] if 0 < ln <= len(disasm) else ""
            if "ImageSampleExplicitLod" in src or "ImageSampleImplicitLod" in src:
                pending = src
    ctrl.FreeTrace(trace)
    log("Debug Vertex %d: %d texture samples" % (idxs[0], len(samples)))
    log("   emulated outputs: %s" % emu_out)
    for i, smp in enumerate(samples):
        log("   sample[%d] = %s" % (i, [round(x, 5) for x in smp]))
    # octree leaf = last sample with w>0 before the three SH samples
    if len(samples) >= 4:
        leaf = samples[-4]
        key = (int(round(leaf[3] * 255)) << 24) | (int(round(leaf[2] * 255)) << 16) | (int(round(leaf[1] * 255)) << 8) | int(round(leaf[0] * 255))
        kx, ky, kz = (key >> 19) & 4095, (key >> 7) & 4095, key & 127
        log("octree leaf %s -> key 0x%08x -> SH texel base (%d,%d,%d)" % ([round(x, 4) for x in leaf], key, kx, ky, kz))
        shatl = [x for x in atl if x[1].format.Name().startswith("R16G16B16A16")]
        if shatl:
            r, t = shatl[0]
            data = ctrl.GetTextureData(r, SUB)
            W, H = t.width, t.height
            for dz in range(6):
                row = []
                for dy in range(2):
                    for dx in range(2):
                        o = (((kz + dz) * H + ky + dy) * W + kx + dx) * 8
                        row.append(tuple(round(half(v), 4) for v in struct.unpack_from("<4H", data, o)))
                log("   z=%d %s" % (kz + dz, row))
            # how many SH cubes (2x2x6) in this atlas are entirely zero vs non-zero
            nz = z = 0
            for cz in range(0, t.depth - 5, 6):
                for cy in range(0, H, 2):
                    for cx in range(0, W, 2):
                        o = ((cz * H + cy) * W + cx) * 8
                        if data[o:o + 8] == b"\0" * 8: z += 1
                        else: nz += 1
            log("   SH atlas cubes (sampled at corner texel): non-zero %d, zero %d" % (nz, z))

run("s26_sh.log", body)
