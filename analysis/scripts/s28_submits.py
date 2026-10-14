# Queue submissions of the frame: queue, wait semaphores + dst stage masks, signal semaphores, and the
# command-buffer EID ranges they contain. Plus the chain for the DrawIndexedIndirect args producer.
# Env: DOOMFIX_WRITER (eid writing the args), DOOMFIX_READER (first indirect draw).
import sys, os, re
sys.path.insert(0, os.environ.get("DOOMFIX_SCRIPTS", "."))  # set by analysis/run_analysis.ps1
import renderdoc as rd
from rdcommon import *
g = {"_IMPORT_ONLY": True}
exec(open(os.path.join(SCRIPTS_DIR, "s8_barriers_atlas.py")).read().split("def body")[0], g)
sd_str = g["sd_str"]

WRITER = int(os.environ["DOOMFIX_WRITER"]); READER = int(os.environ["DOOMFIX_READER"])


def body(ctrl, log):
    sf = ctrl.GetStructuredFile()
    flat = flatten(ctrl.GetRootActions())
    # command buffer boundary actions carry "vkQueueSubmit(N)[i]" in their names
    cbs = []
    for a, p in flat:
        nm = a.GetName(sf)
        m = re.match(r"=> vkQueueSubmit\((\d+)\)\[(\d+)\]: vk(Begin|End)CommandBuffer\(ResourceId::(\d+)", nm)
        if m:
            cbs.append((a.eventId, int(m.group(1)), int(m.group(2)), m.group(3), int(m.group(4))))
    ranges = {}
    for eid, sub, idx, be, cb in cbs:
        r = ranges.setdefault((sub, idx, cb), [None, None])
        r[0 if be == "Begin" else 1] = eid
    def where(eid):
        for (sub, idx, cb), (b, e) in ranges.items():
            if b is not None and e is not None and b <= eid <= e:
                return sub, idx, cb
        return None
    for a, p in flat:
        for ev in a.events:
            if ev.chunkIndex >= len(sf.chunks): continue
            ch = sf.chunks[ev.chunkIndex]
            if ch.name != "vkQueueSubmit": continue
            s = sd_str(ch, 0, 9)
            q = re.search(r"queue=(\d+)", s).group(1)
            waits = re.findall(r"pWaitSemaphores\{([^}]*)\}", s)
            masks = re.findall(r"pWaitDstStageMask\{([^}]*)\}", s)
            sigs = re.findall(r"pSignalSemaphores\{([^}]*)\}", s)
            ncb = re.findall(r"commandBufferCount=(\d+)", s)
            log("%6d vkQueueSubmit queue=%s batches=%d cbs=%s waits=%s masks=%s signals=%s" % (
                ev.eventId, q, len(ncb), ncb, [w.replace("$el=", "") for w in waits],
                [m.replace("$el=", "").replace("VK_PIPELINE_STAGE_", "").replace("_BIT", "") for m in masks],
                [x.replace("$el=", "") for x in sigs]))
    log("\ncommand buffers (submit index, position, cb id): eid range")
    for k, (b, e) in sorted(ranges.items(), key=lambda kv: kv[1][0] or 0):
        log("  %s: %s..%s" % (k, b, e))
    log("\nargs writer %d in %s ; first indirect draw %d in %s" % (WRITER, where(WRITER), READER, where(READER)))

run("s28_submits.log", body)
