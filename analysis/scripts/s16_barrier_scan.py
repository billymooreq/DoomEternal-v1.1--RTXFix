# All vkCmdPipelineBarrier calls in the frame whose dstStageMask contains DRAW_INDIRECT: is
# VK_ACCESS_INDIRECT_COMMAND_READ_BIT present in the dst access masks? Plus a list of indirect commands.
import os, sys, re
sys.path.insert(0, os.environ.get("DOOMFIX_SCRIPTS", "."))  # set by analysis/run_analysis.ps1
import renderdoc as rd
from rdcommon import *
g = {"_IMPORT_ONLY": True}
exec(open(os.path.join(SCRIPTS_DIR, "s8_barriers_atlas.py")).read().split("def body")[0], g)
sd_str = g["sd_str"]


def body(ctrl, log):
    sf = ctrl.GetStructuredFile()
    flat = flatten(ctrl.GetRootActions())
    nb = bad = 0
    ind = {}
    rows = []
    for a, p in flat:
        nm = a.GetName(sf)
        if "Indirect" in nm:
            ind.setdefault(nm.split("(")[0], []).append(a.eventId)
        for ev in a.events:
            if ev.chunkIndex >= len(sf.chunks): continue
            ch = sf.chunks[ev.chunkIndex]
            if ch.name not in ("vkCmdPipelineBarrier", "vkCmdPipelineBarrier2", "vkCmdPipelineBarrier2KHR"): continue
            s = sd_str(ch, 0, 8)
            m = re.search(r"destStageMask=([^,]*)", s) or re.search(r"dstStageMask=([^,]*)", s)
            if not m or "DRAW_INDIRECT" not in s: continue
            nb += 1
            dst_access = re.findall(r"dstAccessMask=([^,}]*)", s)
            ok = all("INDIRECT_COMMAND_READ" in d for d in dst_access) and dst_access
            if not ok:
                bad += 1
                rows.append("%d %s dstAccess=%s" % (ev.eventId, ch.name, dst_access[:2]))
    log("barriers with DRAW_INDIRECT in dst stage: %d, lacking INDIRECT_COMMAND_READ: %d" % (nb, bad))
    for r in rows: log("   " + r[:300])
    for k, v in ind.items():
        log("%s: %d calls, eids %s%s" % (k, len(v), v[:30], " ..." if len(v) > 30 else ""))

run("s16_barrier_scan.log", body)
