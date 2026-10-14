# Which synchronization entry points does the game record in this frame, and is synchronization2 enabled?
import sys
sys.path.insert(0, r"C:\Users\vippr\Documents\doomfix\analysis\scripts")
import renderdoc as rd
from rdcommon import *
g = {"_IMPORT_ONLY": True}
exec(open(r"C:\Users\vippr\Documents\doomfix\analysis\scripts\s8_barriers_atlas.py").read().split("def body")[0], g)
sd_str = g["sd_str"]

WATCH = ("vkCmdPipelineBarrier", "vkCmdPipelineBarrier2", "vkCmdPipelineBarrier2KHR", "vkCmdWaitEvents",
         "vkCmdWaitEvents2", "vkCmdWaitEvents2KHR", "vkCmdSetEvent", "vkCmdSetEvent2", "vkCmdSetEvent2KHR",
         "vkCmdResetEvent", "vkCmdDispatchIndirect", "vkCmdDrawIndirect", "vkCmdDrawIndexedIndirect",
         "vkCmdDrawIndirectCount", "vkCmdDrawIndexedIndirectCount", "vkCmdExecuteCommands", "vkBeginCommandBuffer")


def body(ctrl, log):
    sf = ctrl.GetStructuredFile()
    counts = {}
    for ch in sf.chunks:
        counts[ch.name] = counts.get(ch.name, 0) + 1
        if ch.name == "vkCreateDevice":
            s = sd_str(ch, 0, 6)
            log("vkCreateDevice: synchronization2 mentioned: %s" % ("synchronization2" in s.lower()))
            i = s.find("ppEnabledExtensionNames")
            log("  " + s[i:i + 1500])
        if ch.name == "vkBeginCommandBuffer" and counts[ch.name] <= 3:
            log("  " + sd_str(ch, 0, 5)[:400])
    for n in WATCH:
        log("%-32s %d" % (n, counts.get(n, 0)))
    log("all chunk names with 'Event' or 'Barrier':", {k: v for k, v in counts.items() if "Event" in k or "Barrier" in k})

run("s17_sync_usage.log", body)
