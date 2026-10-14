# Visbox bitfield fill after binning + who else reads the cluster light list (e.g. volumetrics).
import sys, os
sys.path.insert(0, os.environ.get("DOOMFIX_SCRIPTS", "."))  # set by analysis/run_analysis.ps1
import renderdoc as rd
from rdcommon import *

def body(ctrl, log):
    names = res_names(ctrl); byid = {int(r): r for r in names}
    sf = ctrl.GetStructuredFile()
    acts = {a.eventId: a for a, p in flatten(ctrl.GetRootActions())}
    vb, eid, lb = int(os.environ["DOOMFIX_VISBOX"]), int(os.environ["DOOMFIX_LIST_EID"]), int(os.environ["DOOMFIX_LISTBUF"])
    ctrl.SetFrameEvent(eid, True)
    d = ctrl.GetBufferData(byid[vb], 0, 0)
    log("visbox bitfield %s: %d bytes, non-zero bytes %d, set bits %d" % (names[byid[vb]], len(d), len(d) - d.count(0), sum(bin(x).count("1") for x in d)))
    users = {}
    for u in ctrl.GetUsage(byid[lb]):
        k = str(u.usage).split(".")[-1]
        users.setdefault(k, []).append(u.eventId)
    for k, v in users.items():
        cs = [e for e in v if e in acts and acts[e].flags & rd.ActionFlags.Dispatch]
        log("list buffer %s usage %s: n=%d, dispatches %s" % (lb, k, len(v), sorted(set(cs))[:12]))

run("s29_visbox.log", body)
