# Indirect-argument buffers of the light/decal/probe binning dispatches and the resulting cluster lists.
import sys, os, struct
sys.path.insert(0, r"C:\Users\vippr\Documents\doomfix\analysis\scripts")
import renderdoc as rd
from rdcommon import *

ARGS = [1079, 1091, 1105]
LISTS = {"light": 384000, "probe": 1152000, "decal": 7275520}
RANGE = 768000


def body(ctrl, log):
    names = res_names(ctrl)
    byid = {int(r): r for r in names}
    sf = ctrl.GetStructuredFile()
    acts = {a.eventId: a for a, p in flatten(ctrl.GetRootActions())}
    for rid in ARGS:
        r = byid[rid]
        kinds = {}
        for u in ctrl.GetUsage(r):
            kinds.setdefault(str(u.usage).split(".")[-1], []).append(u.eventId)
        log("%s: %s" % (names[r], {k: v[:30] for k, v in kinds.items()}))
    log("\nindirect args (x,y,z,pad) x8 at selected events:")
    for eid in (1, 15770, 15777, 15781, 15817, 15827, 15831, 15873, 15947):
        ctrl.SetFrameEvent(eid, True)
        row = []
        for rid in ARGS:
            d = ctrl.GetBufferData(byid[rid], 0, 128)
            row.append("%d:%s" % (rid, [struct.unpack_from("<I", d, i * 16)[0] for i in range(8)]))
        log("  after %5d  %s" % (eid, "  ".join(row)))
    ctrl.SetFrameEvent(15947, True)
    buf = byid[1753]
    for nm, off in LISTS.items():
        d = ctrl.GetBufferData(buf, off, RANGE)
        hist = {}
        ids = {}
        for c in range(RANGE // 256):
            h = struct.unpack_from("<I", d, c * 256)[0]
            n = h & 255
            hist[n] = hist.get(n, 0) + 1
            for i in range(1, min(n, 63) + 1):
                lid = struct.unpack_from("<I", d, c * 256 + i * 4)[0]
                ids[lid] = ids.get(lid, 0) + 1
        log("%s list: clusters by count %s ; distinct ids %d: %s" % (nm, dict(sorted(hist.items())), len(ids), dict(sorted(ids.items())[:40])))
    # how many lights exist (light parms with non-zero colour/intensity)
    d1 = ctrl.GetBufferData(byid[1062], 0, 0); d2 = ctrl.GetBufferData(byid[1063], 0, 0)
    act = []
    for lid in range(len(d1) // 64):
        col = struct.unpack_from("<3f", d2, lid * 64 + 48)
        typ = struct.unpack_from("<I", d1, lid * 64 + 12)[0] & 7
        pos = struct.unpack_from("<3f", d1, lid * 64)
        if any(abs(c) > 0 for c in col):
            act.append((lid, typ, tuple(round(p, 1) for p in pos), tuple(round(c, 3) for c in col)))
    log("lights with non-zero colour: %d -> %s" % (len(act), act[:60]))

run("s11_indirect.log", body)
