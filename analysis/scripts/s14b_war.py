# Did the first binning phase (indirect dispatches 15781..15817) actually do any work in replay?
# Compare the tile-work-bucket regions of Buffer 1753 before/after it, over several replays.
import sys, os, struct, hashlib
sys.path.insert(0, os.environ.get("DOOMFIX_SCRIPTS", "."))  # set by analysis/run_analysis.ps1
import renderdoc as rd
from rdcommon import *

REGIONS = {"lightnumentries1102": None, "decalbuckets": (8043520, 9175040), "visboxbuckets": (17218560, 9175040), "lightbuckets": (1920000, 4587520),
           "lightbitfield": (0, 384000), "decalbitfield": (6507520, 768000)}


def summarize(data):
    nz = len(data) - data.count(0)
    return "md5=%s nonzero_bytes=%d" % (hashlib.md5(data).hexdigest()[:8], nz)


def body(ctrl, log):
    names = res_names(ctrl)
    buf = [r for r in names if int(r) == 1753][0]
    for it in range(2):
        log("--- replay %d" % it)
        for eid in (15817, 15818, 15821, 15824, 15826, 15827, 15828):
            ctrl.SetFrameEvent(29213, True)
            ctrl.SetFrameEvent(eid, True)
            parts = []
            for k, v in REGIONS.items():
                if v is None:
                    b = [r for r in names if int(r) == 1102][0]
                    a = [r for r in names if int(r) == 1105][0]
                    parts.append("1102[%s] 1105args=%s" % (summarize(ctrl.GetBufferData(b, 0, 112)), [struct.unpack_from("<I", ctrl.GetBufferData(a, 0, 128), i * 16)[0] for i in range(8)]))
                else:
                    parts.append("%s[%s]" % (k, summarize(ctrl.GetBufferData(buf, v[0], v[1]))))
            log("  after %d: %s" % (eid, "  ".join(parts)))

run("s14b_war.log", body)
