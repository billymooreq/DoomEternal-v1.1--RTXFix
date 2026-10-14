# Compact API sequence around the clustered binning: barriers (stages/access per barrier), dispatches with
# their compute shader and indirect buffer. Range from env DOOMFIX_RANGE="first,last".
import sys, os, re
sys.path.insert(0, r"C:\Users\vippr\Documents\doomfix\analysis\scripts")
import renderdoc as rd
from rdcommon import *
g = {"_IMPORT_ONLY": True}
exec(open(r"C:\Users\vippr\Documents\doomfix\analysis\scripts\s8_barriers_atlas.py").read().split("def body")[0], g)
sd_str = g["sd_str"]

FIRST, LAST = [int(x) for x in os.environ.get("DOOMFIX_RANGE", "15755,15890").split(",")]


def short(m):
    return m.replace("VK_PIPELINE_STAGE_", "").replace("VK_ACCESS_", "").replace("_BIT", "").replace(" ", "")


def body(ctrl, log):
    sf = ctrl.GetStructuredFile()
    acts = {a.eventId: a for a, p in flatten(ctrl.GetRootActions())}
    for a, p in flatten(ctrl.GetRootActions()):
        if not (FIRST <= a.eventId <= LAST): continue
        for ev in a.events:
            if ev.chunkIndex >= len(sf.chunks): continue
            ch = sf.chunks[ev.chunkIndex]
            if ch.name == "vkCmdPipelineBarrier":
                s = sd_str(ch, 0, 8)
                src = short(re.search(r"srcStageMask=([^,]*)", s).group(1))
                dst = short(re.search(r"destStageMask=([^,]*)", s).group(1))
                mems = re.findall(r"srcAccessMask=([^,]*), dstAccessMask=([^,}]*)", s)
                nb = re.search(r"bufferMemoryBarrierCount=(\d+)", s).group(1)
                ni = re.search(r"imageMemoryBarrierCount=(\d+)", s).group(1)
                log("%6d BARRIER %s -> %s | access: %s | buf=%s img=%s" % (
                    ev.eventId, src, dst, "; ".join("%s->%s" % (short(x), short(y)) for x, y in mems[:3]), nb, ni))
            elif "Dispatch" in ch.name:
                ctrl.SetFrameEvent(ev.eventId, False)
                cs = int(ctrl.GetPipelineState().GetShader(rd.ShaderStage.Compute))
                s = sd_str(ch, 0, 3)
                buf = re.search(r"buffer=(\d+), offset=(\d+)", s)
                nm = acts[ev.eventId].GetName(sf) if ev.eventId in acts else ch.name
                log("%6d %-36s cs=%d%s" % (ev.eventId, nm[:36], cs, " args=%s+%s" % buf.groups() if buf else ""))
            elif ch.name in ("vkCmdFillBuffer", "vkCmdUpdateBuffer", "vkCmdCopyBuffer", "vkQueueSubmit",
                             "vkCmdBeginRenderPass", "vkEndCommandBuffer", "vkBeginCommandBuffer"):
                log("%6d %s" % (ev.eventId, sd_str(ch, 0, 3)[:200]))

run("s21_binning_seq.log", body)
