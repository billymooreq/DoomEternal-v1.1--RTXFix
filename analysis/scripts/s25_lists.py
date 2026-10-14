# Generic version of s9/s11/s14 for any capture.
#   DOOMFIX_BUILDERS  dispatches that build the cluster lists (their RW bindings are printed)
#   DOOMFIX_LISTBUF   buffer holding the lists, DOOMFIX_LISTS "name:offset,..." (range 768000 each)
#   DOOMFIX_LIST_EID  event after which the lists are final
#   DOOMFIX_ARGS      indirect-args buffers, DOOMFIX_ARGS_EIDS events at which to print them
#   DOOMFIX_PHASE     "eid,..." events to replay to (each after a full-frame replay) and report how much the
#                     binning wrote into the light tile buckets (+1920000) and light bitfield (+0)
import sys, os, struct
sys.path.insert(0, r"C:\Users\vippr\Documents\doomfix\analysis\scripts")
import renderdoc as rd
from rdcommon import *

RANGE = 768000


def env_list(name, conv=int):
    v = os.environ.get(name, "")
    return [conv(x) for x in v.split(",") if x]


def body(ctrl, log):
    sf = ctrl.GetStructuredFile()
    names = res_names(ctrl)
    byid = {int(r): r for r in names}
    acts = {a.eventId: a for a, p in flatten(ctrl.GetRootActions())}
    last = max(acts)
    for eid in env_list("DOOMFIX_BUILDERS"):
        ctrl.SetFrameEvent(eid, False)
        st = ctrl.GetPipelineState()
        refl = st.GetShaderReflection(rd.ShaderStage.Compute)
        b = ["%s=%d@%d+%d" % (refl.readWriteResources[u.access.index].name, int(u.descriptor.resource), u.descriptor.byteOffset, u.descriptor.byteSize)
             for u in st.GetReadWriteResources(rd.ShaderStage.Compute, True)]
        log("builder %d %s cs=%d: %s" % (eid, acts[eid].GetName(sf), int(st.GetShader(rd.ShaderStage.Compute)), b))
    args = env_list("DOOMFIX_ARGS")
    for eid in env_list("DOOMFIX_ARGS_EIDS"):
        ctrl.SetFrameEvent(eid, True)
        row = ["%d:%s" % (a, [struct.unpack_from("<I", ctrl.GetBufferData(byid[a], 0, 128), i * 16)[0] for i in range(8)]) for a in args]
        log("args after %d: %s" % (eid, "  ".join(row)))
    lb = os.environ.get("DOOMFIX_LISTBUF")
    if lb:
        buf = byid[int(lb)]
        ctrl.SetFrameEvent(int(os.environ["DOOMFIX_LIST_EID"]), True)
        for item in os.environ["DOOMFIX_LISTS"].split(","):
            nm, off = item.split(":")
            d = ctrl.GetBufferData(buf, int(off), RANGE)
            hist, ids, total = {}, {}, 0
            for c in range(RANGE // 256):
                n = struct.unpack_from("<I", d, c * 256)[0] & 255
                hist[n] = hist.get(n, 0) + 1
                total += n
                for i in range(1, min(n, 63) + 1):
                    lid = struct.unpack_from("<I", d, c * 256 + i * 4)[0]
                    ids[lid] = ids.get(lid, 0) + 1
            nc = RANGE // 256
            log("%s list @%s: clusters %d, mean entries/cluster %.2f, max %d, empty %d; histogram %s; distinct ids %d" % (
                nm, off, nc, total / float(nc), max(k for k in hist if hist[k]), hist.get(0, 0), dict(sorted(hist.items())), len(ids)))
            log("   cluster 2736: %s" % (struct.unpack_from("<8I", d, 2736 * 256),))
    for eid in env_list("DOOMFIX_PHASE"):
        bufid = int(lb) if lb else 1753
        ctrl.SetFrameEvent(last, True)
        ctrl.SetFrameEvent(eid, True)
        bk = ctrl.GetBufferData(byid[bufid], 1920000, 4587520)
        bf = ctrl.GetBufferData(byid[bufid], 0, 384000)
        log("replay to %d (%s): light tile buckets non-zero bytes %d, light bitfield non-zero bytes %d" % (
            eid, acts[eid].GetName(sf) if eid in acts else "api event", len(bk) - bk.count(0), len(bf) - bf.count(0)))

run(os.environ.get("DOOMFIX_LOG", "s25_lists.log"), body)
