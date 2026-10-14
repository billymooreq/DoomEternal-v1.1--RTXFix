# Did the first binning phase (indirect dispatches 15781..15817) actually do any work in replay?
# Compare the tile-work-bucket regions of Buffer 1753 before/after it, over several replays.
import sys, os, struct, hashlib
sys.path.insert(0, r"C:\Users\vippr\Documents\doomfix\analysis\scripts")
import renderdoc as rd
from rdcommon import *

REGIONS = {"decalbuckets": (8043520, 9175040), "visboxbuckets": (17218560, 9175040), "lightbuckets": (1920000, 4587520),
           "lightbitfield": (0, 384000), "decalbitfield": (6507520, 768000)}


def summarize(data):
    nz = len(data) - data.count(0)
    return "md5=%s nonzero_bytes=%d" % (hashlib.md5(data).hexdigest()[:8], nz)


def body(ctrl, log):
    names = res_names(ctrl)
    buf = [r for r in names if int(r) == 1753][0]
    for it in range(3):
        log("--- replay %d" % it)
        for eid in (15778, 15817, 15828, 15873, 15883):
            ctrl.SetFrameEvent(29213, True)
            ctrl.SetFrameEvent(eid, True)
            log("  after %d: %s" % (eid, "  ".join("%s[%s]" % (k, summarize(ctrl.GetBufferData(buf, o, s))) for k, (o, s) in REGIONS.items())))

run("s14_phase1.log", body)
