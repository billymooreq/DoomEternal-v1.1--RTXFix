import sys, os
sys.path.insert(0, os.environ.get("DOOMFIX_SCRIPTS", "."))  # set by analysis/run_analysis.ps1
import renderdoc as rd
from rdcommon import *


def desc_str(ctrl, d, names, texs, bufs):
    rid = d.resource
    s = "%s '%s'" % (int(rid), names.get(rid, "?"))
    if rid in texs:
        t = texs[rid]
        s += " TEX %dx%dx%d mips=%d %s fmt=%s" % (t.width, t.height, t.depth, t.mips, t.type, t.format.Name())
        if d.format.Name() != "Unknown" and d.format.Name() != t.format.Name():
            s += " view=%s" % d.format.Name()
    elif rid in bufs:
        s += " BUF size=%d off=%d range=%d" % (bufs[rid].length, d.byteOffset, d.byteSize)
    return s


def body(ctrl, log):
    sf = ctrl.GetStructuredFile()
    names = res_names(ctrl); texs = tex_map(ctrl); bufs = buf_map(ctrl)
    flat = flatten(ctrl.GetRootActions())
    log("total actions:", len(flat))
    # print markers / passes overview
    for a, par in flat:
        if a.children:
            log("PASS eid %d-%d '%s' depth=%d nchild=%d" % (a.eventId, a.children[-1].eventId if a.children else a.eventId, a.GetName(sf), len(par), len(a.children)))
    log("\n=== actions eid >= 27000 ===")
    for a, par in flat:
        if a.eventId >= 27000 and not a.children:
            log("%6d %-60s flags=%s outs=%s depth=%s" % (a.eventId, a.GetName(sf)[:60], str(a.flags).replace("ActionFlags.", ""),
                [int(o) for o in a.outputs if int(o)], int(a.depthOut)))
    log("\n=== compute descriptors eid 29100..29190 ===")
    for a, par in flat:
        if 29100 <= a.eventId <= 29190 and (a.flags & rd.ActionFlags.Dispatch):
            ctrl.SetFrameEvent(a.eventId, True)
            st = ctrl.GetPipelineState()
            log("-- %d %s" % (a.eventId, a.GetName(sf)))
            for u in st.GetReadOnlyResources(rd.ShaderStage.Compute, True):
                log("   RO  ", desc_str(ctrl, u.descriptor, names, texs, bufs))
            for u in st.GetReadWriteResources(rd.ShaderStage.Compute, True):
                log("   RW  ", desc_str(ctrl, u.descriptor, names, texs, bufs))
    ctrl.SetFrameEvent(29186, True)
    for rid, t in texs.items():
        if names.get(rid, "").endswith("53390"):
            v = ctrl.PickPixel(rid, 1437, 917, rd.Subresource(0, 0, 0), rd.CompType.Typeless)
            log("53390 @1437,917 after 29186:", list(v.floatValue))

run("s1_explore.log", body)
