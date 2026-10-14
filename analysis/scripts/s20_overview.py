# Overview of a capture for cross-version comparison: device/API info, sync & indirect call statistics,
# indirect commands with their bound buffers, barriers relevant to indirect args, main render passes.
# Usage: DOOMFIX_CAPTURE=<rdc> DOOMFIX_OUT=<subdir> qrenderdoc --python s20_overview.py
import os, sys, re
sys.path.insert(0, os.environ.get("DOOMFIX_SCRIPTS", "."))  # set by analysis/run_analysis.ps1
import renderdoc as rd
from rdcommon import *
g = {"_IMPORT_ONLY": True}
exec(open(os.path.join(SCRIPTS_DIR, "s8_barriers_atlas.py")).read().split("def body")[0], g)
sd_str = g["sd_str"]

BARRIER_CHUNKS = ("vkCmdPipelineBarrier", "vkCmdPipelineBarrier2", "vkCmdPipelineBarrier2KHR",
                  "vkCmdWaitEvents", "vkCmdWaitEvents2", "vkCmdWaitEvents2KHR")


def body(ctrl, log):
    sf = ctrl.GetStructuredFile()
    names = res_names(ctrl); texs = tex_map(ctrl); bufs = buf_map(ctrl)
    props = ctrl.GetAPIProperties()
    log("API: %s vendor=%s localRenderer=%s" % (props.pipelineType, props.vendor, props.localRenderer))
    counts = {}
    for ch in sf.chunks:
        counts[ch.name] = counts.get(ch.name, 0) + 1
        if ch.name in ("vkCreateInstance", "vkCreateDevice"):
            s = sd_str(ch, 0, 6)
            for key in ("apiVersion", "ppEnabledExtensionNames"):
                i = s.find(key)
                if i >= 0: log("%s %s" % (ch.name, s[i:i + 900]))
    for n in sorted(counts):
        if any(k in n for k in ("Barrier", "Event", "Indirect", "Dispatch", "Submit", "ExecuteCommands", "BeginCommandBuffer")):
            log("  %-36s %d" % (n, counts[n]))

    flat = flatten(ctrl.GetRootActions())
    log("total actions: %d" % len(flat))
    # barriers in the order they appear, with the following action
    ev_barrier = []   # (eid, chunk name, sd string)
    for a, p in flat:
        for ev in a.events:
            if ev.chunkIndex >= len(sf.chunks): continue
            ch = sf.chunks[ev.chunkIndex]
            if ch.name in BARRIER_CHUNKS:
                ev_barrier.append((ev.eventId, ch.name, sd_str(ch, 0, 8)))
    log("barrier calls in frame: %d" % len(ev_barrier))
    ind = [(a, p) for a, p in flat if "Indirect" in a.GetName(sf)]
    log("\n== indirect commands: %d ==" % len(ind))
    seen_prev = set()
    for a, p in ind:
        nm = a.GetName(sf)
        prev = [b for b in ev_barrier if b[0] < a.eventId]
        pb = prev[-1] if prev else None
        line = "%6d %-44s" % (a.eventId, nm[:44])
        if a.flags & rd.ActionFlags.Dispatch:
            ctrl.SetFrameEvent(a.eventId, False)
            st = ctrl.GetPipelineState()
            refl = st.GetShaderReflection(rd.ShaderStage.Compute)
            rw = []
            for u in st.GetReadWriteResources(rd.ShaderStage.Compute, True):
                r = refl.readWriteResources[u.access.index]
                rw.append("%s=%s@%d" % (r.name, int(u.descriptor.resource), u.descriptor.byteOffset))
            line += " cs=%d rw=[%s]" % (int(st.GetShader(rd.ShaderStage.Compute)), ", ".join(rw)[:400])
        log(line)
        if pb and pb[0] not in seen_prev:
            seen_prev.add(pb[0])
            s = pb[2]
            src = re.findall(r"srcStageMask=([^,]*)", s)
            dst = re.findall(r"(?:destStageMask|dstStageMask)=([^,]*)", s)
            da = re.findall(r"dstAccessMask=([^,}]*)", s)
            log("        preceding barrier %d %s: src=%s dst=%s dstAccess=%s" % (pb[0], pb[1], src[:3], dst[:3], da[:3]))
    log("\n== barriers with DRAW_INDIRECT in dst stage ==")
    nb = bad = 0
    for eid, nm, s in ev_barrier:
        dst = " ".join(re.findall(r"(?:destStageMask|dstStageMask)=([^,]*)", s))
        if "DRAW_INDIRECT" not in dst: continue
        nb += 1
        da = re.findall(r"dstAccessMask=([^,}]*)", s)
        ok = bool(da) and all(("INDIRECT_COMMAND_READ" in d or "MEMORY_READ" in d) for d in da)
        bad += not ok
        log("  %6d %s %s dst=%s dstAccess=%s" % (eid, nm, "OK " if ok else "BAD", dst[:120], da[:3]))
    log("  total %d, lacking INDIRECT_COMMAND_READ/MEMORY_READ: %d" % (nb, bad))

    log("\n== render passes (>= 5 draws) ==")
    cur = None
    for a, p in flat:
        if (a.flags & rd.ActionFlags.BeginPass) and not (a.flags & rd.ActionFlags.CommandBufferBoundary):
            cur = [a.eventId, 0, [int(o) for o in a.outputs if int(o)], int(a.depthOut)]
        elif (a.flags & rd.ActionFlags.EndPass) and not (a.flags & rd.ActionFlags.CommandBufferBoundary) and cur:
            if cur[1] >= 5:
                outs = []
                for o in cur[2]:
                    t = [t for r, t in texs.items() if int(r) == o]
                    outs.append("%d(%s)" % (o, "%dx%d %s" % (t[0].width, t[0].height, t[0].format.Name()) if t else "?"))
                log("  pass %d..%d draws=%d outs=%s depth=%d" % (cur[0], a.eventId, cur[1], outs, cur[3]))
            cur = None
        elif cur and (a.flags & rd.ActionFlags.Drawcall):
            cur[1] += 1
    log("\n== last 40 actions ==")
    for a, p in flat[-40:]:
        log("  %6d %s outs=%s" % (a.eventId, a.GetName(sf)[:70], [int(o) for o in a.outputs if int(o)]))

run("s20_overview.log", body)
