# 1) API calls (incl. pipeline barriers) between the light-culling / skinning compute and the forward pass
# 2) contents of the ambient SH atlas at the key used by the weapon vertices
# 3) GPU post-VS ambient SH of a correctly lit enemy for comparison
import sys, os, struct
sys.path.insert(0, os.environ.get("DOOMFIX_SCRIPTS", "."))  # set by analysis/run_analysis.ps1
import renderdoc as rd
from rdcommon import *

SUB = rd.Subresource(0, 0, 0)
RANGE = (15770, 15962)


def sd_str(o, depth=0, maxd=6):
    """Compact recursive dump of an SDObject."""
    name = o.name
    if o.NumChildren() == 0:
        bt = o.type.basetype
        try:
            if bt == rd.SDBasic.String: v = o.AsString()
            elif bt == rd.SDBasic.Enum or bt == rd.SDBasic.UnsignedInteger: v = o.AsString() if o.type.flags & rd.SDTypeFlags.HasCustomString else str(o.AsInt())
            elif bt == rd.SDBasic.SignedInteger: v = str(o.AsInt())
            elif bt == rd.SDBasic.Float: v = "%g" % o.AsFloat()
            elif bt == rd.SDBasic.Boolean: v = str(o.AsBool())
            elif bt == rd.SDBasic.Resource: v = str(int(o.AsResourceId()))
            else: v = o.AsString()
        except Exception:
            v = "?"
        return "%s=%s" % (name, v)
    if depth >= maxd:
        return "%s{...}" % name
    return "%s{%s}" % (name, ", ".join(sd_str(o.GetChild(i), depth + 1, maxd) for i in range(o.NumChildren())))


def half(h):
    return struct.unpack("<e", struct.pack("<H", h))[0]


def body(ctrl, log):
    sf = ctrl.GetStructuredFile()
    names = res_names(ctrl)
    byid = {int(r): r for r in names}
    flat = flatten(ctrl.GetRootActions())
    out = open(os.path.join(OUTDIR, "api_15770_15962.txt"), "w", encoding="utf-8")
    for a, p in flat:
        if not (RANGE[0] <= a.eventId <= RANGE[1]): continue
        for ev in a.events:
            ch = sf.chunks[ev.chunkIndex]
            out.write("%6d %s\n" % (ev.eventId, sd_str(ch, 0, 7) if "Barrier" in ch.name or "Dispatch" in ch.name or "BeginRenderPass" in ch.name or "Submit" in ch.name or "Event" in ch.name else ch.name))
    out.close()
    log("API dump written: api_15770_15962.txt")

    # usage of skinning buffers
    for rid in (1033, 1035, 216817, 216818):
        r = byid.get(rid)
        if r is None: continue
        kinds = {}
        for u in ctrl.GetUsage(r):
            kinds.setdefault(str(u.usage).split(".")[-1], []).append(u.eventId)
        log("%s: %s" % (names[r], {k: (len(v), v[:12]) for k, v in kinds.items()}))

    # ambient SH atlas
    ctrl.SetFrameEvent(15976, True)
    texs = tex_map(ctrl)
    for rid, bpp in ((201675, 8), (201672, 4)):
        r = byid[rid]; t = texs[r]
        data = ctrl.GetTextureData(r, SUB)
        W, H, D = t.width, t.height, t.depth
        log("%s %dx%dx%d %s: %d bytes (expected %d)" % (names[r], W, H, D, t.format.Name(), len(data), W * H * D * bpp))
        n = len(data) // bpp
        nz = 0
        zslices = []
        for z in range(D):
            sl = data[z * W * H * bpp:(z + 1) * W * H * bpp]
            c = len(sl) - sl.count(0)
            zslices.append(c)
            nz += (c > 0)
        log("   non-zero bytes per slice:", zslices)
        if rid == 201675:
            key = (134 << 24) | (96 << 16) | (56 << 8) | 78
            kx, ky, kz = (key >> 19) & 4095, (key >> 7) & 4095, key & 127
            log("   weapon SH key texel base = (%d,%d,%d)" % (kx, ky, kz))
            for dz in range(0, 6):
                row = []
                for dy in range(2):
                    for dx in range(2):
                        x, y, z = kx + dx, ky + dy, kz + dz
                        o = ((z * H + y) * W + x) * 8
                        row.append(tuple(round(half(v), 4) for v in struct.unpack_from("<4H", data, o)))
                log("     z=%d: %s" % (kz + dz, row))
    # enemy for comparison
    tgt = [r for r, n in names.items() if n.endswith(" 53374")][0]
    for (x, y) in [(905, 620), (1205, 520)]:
        hist = ctrl.PixelHistory(tgt, x, y, SUB, rd.CompType.Typeless)
        acts = {a.eventId: a for a, p in flat}
        best = None
        for m in hist:
            a = acts.get(m.eventId)
            if a and (a.flags & rd.ActionFlags.Drawcall) and m.Passed():
                best = m
        if not best:
            log("no draw at", x, y); continue
        ctrl.SetFrameEvent(best.eventId, True)
        st = ctrl.GetPipelineState()
        vs = st.GetShader(rd.ShaderStage.Vertex); ps = st.GetShader(rd.ShaderStage.Pixel)
        refl = st.GetShaderReflection(rd.ShaderStage.Vertex)
        sig = [(s.varName, s.compCount) for s in refl.outputSignature]
        mf = ctrl.GetPostVSData(0, 0, rd.MeshDataStage.VSOut)
        raw = ctrl.GetBufferData(mf.vertexResourceId, mf.vertexByteOffset, mf.vertexByteStride * 4)
        log("pixel (%d,%d): draw %d vs=%d ps=%d out=%s shaderOut=%s" % (x, y, best.eventId, int(vs), int(ps), sig[:4], list(best.shaderOut.col.floatValue[:3])))
        off = 0; lay = {}
        for nm, c in sig:
            lay[nm] = (off, c); off += 4 * c
        for vi in range(4):
            vals = {nm: struct.unpack_from("<%df" % c, raw, vi * mf.vertexByteStride + o) for nm, (o, c) in lay.items() if "TexCoord8" in nm or "TexCoord9" in nm}
            log("    vertex slot %d: %s" % (vi, vals))

run("s8_barriers_atlas.log", body)
