import sys
sys.path.insert(0, r"C:\Users\vippr\Documents\doomfix\analysis\scripts")
import renderdoc as rd
from rdcommon import *

X, Y = 1437, 917
SUB = rd.Subresource(0, 0, 0)


def find_tex(names, suffix):
    for rid, n in names.items():
        if n.endswith(" " + suffix):
            return rid


def fv(v):
    return "(%.5g, %.5g, %.5g, %.5g)" % tuple(v.floatValue[:4])


def body(ctrl, log):
    sf = ctrl.GetStructuredFile()
    names = res_names(ctrl); texs = tex_map(ctrl)
    flat = flatten(ctrl.GetRootActions())
    byeid = {a.eventId: a for a, p in flat}
    # who writes 1771 (dispatches before 29103)
    for a, p in flat:
        if 27000 <= a.eventId <= 29100 and (a.flags & rd.ActionFlags.Dispatch):
            ctrl.SetFrameEvent(a.eventId, True)
            st = ctrl.GetPipelineState()
            log("-- dispatch %d" % a.eventId)
            for u in st.GetReadOnlyResources(rd.ShaderStage.Compute, True):
                r = u.descriptor.resource
                t = texs.get(r)
                log("   RO", int(r), names.get(r), ("%dx%d %s" % (t.width, t.height, t.format.Name())) if t else "")
            for u in st.GetReadWriteResources(rd.ShaderStage.Compute, True):
                r = u.descriptor.resource
                t = texs.get(r)
                log("   RW", int(r), names.get(r), ("%dx%d %s" % (t.width, t.height, t.format.Name())) if t else "")
    for suf in ["1771", "1779", "1783", "53374", "53398", "53390", "685990"]:
        rid = find_tex(names, suf)
        if rid is None:
            log("no tex", suf); continue
        ctrl.SetFrameEvent(29213, True)
        for (x, y) in [(X, Y), (1290, 805)]:
            v = ctrl.PickPixel(rid, x, y, SUB, rd.CompType.Typeless)
            log("final %s @%d,%d = %s" % (suf, x, y, fv(v)))
        log("\n=== PixelHistory %s @%d,%d ===" % (suf, X, Y))
        hist = ctrl.PixelHistory(rid, X, Y, SUB, rd.CompType.Typeless)
        for m in hist:
            a = byeid.get(m.eventId)
            log("eid %6d %-28s prim=%d frag=%d passed=%s direct=%s unbound=%s pre=%s shaderOut=%s post=%s depth pre/post=%.5g/%.5g flags[cull=%s depthclip=%s depthtest=%s stencil=%s scissor=%s backface=%s]" % (
                m.eventId, a.GetName(sf)[:28] if a else "?", m.primitiveID, m.fragIndex, m.Passed(), m.directShaderWrite, m.unboundPS,
                fv(m.preMod.col), fv(m.shaderOut.col), fv(m.postMod.col), m.preMod.depth, m.postMod.depth,
                m.sampleMasked, m.depthClipped, m.depthTestFailed, m.stencilTestFailed, m.scissorClipped, m.backfaceCulled))

run("s2_history.log", body)
