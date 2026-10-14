# 1) Every barrier of the frame in a normalized, version-independent form (for diffing 1.1 vs current).
# 2) For every indirect command: the args buffer, its last writer before the command and the barriers /
#    submit boundaries between them — does any make the write visible to INDIRECT_COMMAND_READ?
import os, sys, re
sys.path.insert(0, os.environ.get("DOOMFIX_SCRIPTS", "."))  # set by analysis/run_analysis.ps1
import renderdoc as rd
from rdcommon import *
g = {"_IMPORT_ONLY": True}
exec(open(os.path.join(SCRIPTS_DIR, "s8_barriers_atlas.py")).read().split("def body")[0], g)
sd_str = g["sd_str"]

WRITE_USAGE = ("RWResource", "CopyDst", "Clear", "ResolveDst", "GenMips", "CPUWrite", "ColorTarget", "DepthStencilTarget")


def short(m):
    return m.replace("VK_PIPELINE_STAGE_", "").replace("VK_ACCESS_", "").replace("VK_IMAGE_LAYOUT_", "").replace("_BIT", "").replace(" ", "")


def body(ctrl, log):
    sf = ctrl.GetStructuredFile()
    names = res_names(ctrl); texs = tex_map(ctrl); bufs = buf_map(ctrl)
    flat = flatten(ctrl.GetRootActions())
    events = []   # (eid, chunk name, sd)
    for a, p in flat:
        for ev in a.events:
            if ev.chunkIndex < len(sf.chunks):
                ch = sf.chunks[ev.chunkIndex]
                events.append((ev.eventId, ch.name, ch))
    acts = {a.eventId: a for a, p in flat}
    # --- 1. normalized barrier list
    out = open(os.path.join(OUTDIR, "barriers_normalized.txt"), "w", encoding="utf-8")
    barriers = []
    nxt_name = {}
    eids = [e[0] for e in events]
    for i, (eid, nm, ch) in enumerate(events):
        if nm != "vkCmdPipelineBarrier": continue
        s = sd_str(ch, 0, 9)
        src = short(re.search(r"srcStageMask=([^,]*)", s).group(1))
        dst = short(re.search(r"destStageMask=([^,]*)", s).group(1))
        acc = [(short(x), short(y)) for x, y in re.findall(r"srcAccessMask=([^,]*), dstAccessMask=([^,}]*)", s)]
        lay = [(short(x), short(y)) for x, y in re.findall(r"oldLayout=([^,]*), newLayout=([^,]*)", s)]
        imgs = re.findall(r"image=(\d+)", s)
        imgdesc = []
        for im in imgs:
            t = [t for r, t in texs.items() if int(r) == int(im)]
            imgdesc.append("%dx%d %s" % (t[0].width, t[0].height, t[0].format.Name()) if t else "?")
        follow = ""
        for e2 in events[i + 1:i + 12]:
            if e2[1].startswith("vkCmdDispatch") or e2[1].startswith("vkCmdDraw") or e2[1].startswith("vkCmdBeginRenderPass") or e2[1].startswith("vkCmdCopy"):
                follow = e2[1]; break
        line = "%s -> %s | %s | img: %s | next: %s" % (
            src, dst, "; ".join("%s->%s" % a for a in acc),
            "; ".join("%s[%s->%s]" % (d, l[0], l[1]) for d, l in zip(imgdesc, lay)), follow)
        barriers.append((eid, src, dst, acc, line))
        out.write("%6d %s\n" % (eid, line))
    out.close()
    log("barriers: %d (normalized list in barriers_normalized.txt)" % len(barriers))

    # heuristic: destination stages that read buffers/images but no matching read access
    log("\n== heuristic stage/access mismatches ==")
    for eid, src, dst, acc, line in barriers:
        dacc = " ".join(a[1] for a in acc)
        issues = []
        if "DRAW_INDIRECT" in dst and "INDIRECT_COMMAND_READ" not in dacc and "MEMORY_READ" not in dacc: issues.append("DRAW_INDIRECT w/o INDIRECT_COMMAND_READ")
        if "VERTEX_INPUT" in dst and not re.search("INDEX_READ|VERTEX_ATTRIBUTE_READ|MEMORY_READ", dacc): issues.append("VERTEX_INPUT w/o INDEX/VERTEX_ATTRIBUTE_READ")
        if "TRANSFER" in dst and acc and not re.search("TRANSFER_READ|TRANSFER_WRITE|MEMORY_", dacc): issues.append("TRANSFER w/o TRANSFER_* access")
        if "HOST" in dst and acc and not re.search("HOST_READ|MEMORY_READ", dacc): issues.append("HOST w/o HOST_READ")
        if acc and not dacc.strip(): issues.append("empty dstAccess")
        if issues: log("  %6d %s\n         %s" % (eid, ", ".join(issues), line))

    # --- 2. indirect args hazards
    log("\n== indirect commands: args buffer, last writer, synchronization ==")
    submits = [e[0] for e in events if e[1] == "vkQueueSubmit"]
    cb_bounds = [e[0] for e in events if e[1] in ("vkBeginCommandBuffer", "vkEndCommandBuffer")]
    summary = {}
    for eid, nm, ch in events:
        if "Indirect" not in nm or not nm.startswith("vkCmd"): continue
        s = sd_str(ch, 0, 3)
        m = re.search(r"buffer=(\d+), offset=(\d+)", s)
        if not m: continue
        bid = int(m.group(1))
        r = [x for x in names if int(x) == bid]
        if not r: continue
        writes = [u for u in ctrl.GetUsage(r[0]) if any(w in str(u.usage) for w in WRITE_USAGE) and u.eventId < eid]
        if not writes:
            key = (nm, bid, None)
            summary.setdefault(key, []).append(eid)
            continue
        w = writes[-1]
        between = [b for b in barriers if w.eventId < b[0] < eid]
        ok = [b for b in between if "DRAW_INDIRECT" in b[2] and any(("INDIRECT_COMMAND_READ" in a[1] or "MEMORY_READ" in a[1]) for a in b[3])]
        exec_only = [b for b in between if "DRAW_INDIRECT" in b[2] or "ALL_COMMANDS" in b[2]]
        crossing = [x for x in submits if w.eventId < x < eid]
        verdict = "OK (barrier %d)" % ok[0][0] if ok else ("submit boundary %s" % crossing[:2] if crossing else ("NO VISIBILITY (stage-only barriers %s)" % [b[0] for b in exec_only] if exec_only else "NO BARRIER WITH DRAW_INDIRECT"))
        key = (nm.split("(")[0], bid, "%s@%d %s" % (str(w.usage).split(".")[-1], w.eventId, verdict))
        summary.setdefault(key, []).append(eid)
    for (nm, bid, info), lst in sorted(summary.items(), key=lambda kv: kv[1][0]):
        log("  %s args=%s (%d calls, first %d, last %d): last write %s" % (nm, names.get([x for x in names if int(x) == bid][0], bid), len(lst), lst[0], lst[-1], info or "none in frame (written in a previous frame / by CPU)"))

run("s22_indirect_hazards.log", body)
