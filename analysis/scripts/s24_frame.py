# Save the final swapchain image and the HDR scene target, and run Pixel History at given pixels.
# Env: DOOMFIX_TARGET=<HDR target resource id>, DOOMFIX_PIXELS="x,y;x,y".
import sys, os
sys.path.insert(0, r"C:\Users\vippr\Documents\doomfix\analysis\scripts")
import renderdoc as rd
from rdcommon import *

SUB = rd.Subresource(0, 0, 0)
TARGET = int(os.environ.get("DOOMFIX_TARGET", "53374"))
PIXELS = [tuple(int(v) for v in p.split(",")) for p in os.environ.get("DOOMFIX_PIXELS", "1437,917").split(";")]


def body(ctrl, log):
    sf = ctrl.GetStructuredFile()
    names = res_names(ctrl)
    flat = flatten(ctrl.GetRootActions())
    acts = {a.eventId: a for a, p in flat}
    last = flat[-1][0].eventId
    ctrl.SetFrameEvent(last, True)
    sw = [r for r, n in names.items() if n.startswith("Swapchain Image")]
    tgt = [r for r in names if int(r) == TARGET][0]
    for rid, fn in [(tgt, "scene_hdr_final.png")] + [(r, "swapchain_%d.png" % int(r)) for r in sw]:
        ts = rd.TextureSave()
        ts.resourceId = rid
        ts.destType = rd.FileType.PNG
        ts.alpha = rd.AlphaMapping.Discard
        if rid == tgt:
            ts.comp.blackPoint, ts.comp.whitePoint = 0.0, 0.02   # HDR values are exposure-scaled
        log("save %s -> %s" % (names.get(rid), ctrl.SaveTexture(ts, os.path.join(OUTDIR, fn))))
    for (x, y) in PIXELS:
        log("\n== Pixel History %s @%d,%d ==" % (names.get(tgt), x, y))
        for m in ctrl.PixelHistory(tgt, x, y, SUB, rd.CompType.Typeless):
            a = acts.get(m.eventId)
            if not m.Passed() and not (a and a.flags & rd.ActionFlags.Drawcall): continue
            ps = ""
            if a and a.flags & rd.ActionFlags.Drawcall:
                ctrl.SetFrameEvent(m.eventId, False)
                st = ctrl.GetPipelineState()
                ps = "vs=%d ps=%d pipe=%d idx=%d" % (int(st.GetShader(rd.ShaderStage.Vertex)), int(st.GetShader(rd.ShaderStage.Pixel)),
                                                    int(st.GetGraphicsPipelineObject()), a.numIndices)
            log("  eid %6d %-26s passed=%-5s depth=%.5f shaderOut=(%.5g %.5g %.5g) post=(%.5g %.5g %.5g) %s" % (
                m.eventId, a.GetName(sf)[:26] if a else "?", m.Passed(), m.postMod.depth,
                *m.shaderOut.col.floatValue[:3], *m.postMod.col.floatValue[:3], ps))

run("s24_frame.log", body)
