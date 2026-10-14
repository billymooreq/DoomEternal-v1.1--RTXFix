# Is the clustered light/probe list content stable across replays? Which dispatches write it?
import sys, os, struct, hashlib
sys.path.insert(0, os.environ.get("DOOMFIX_SCRIPTS", "."))  # set by analysis/run_analysis.ps1
import renderdoc as rd
from rdcommon import *

LIGHTLIST_OFF, PROBELIST_OFF, LIST_RANGE, CLUSTER = 384000, 1152000, 768000, 2736
SUB = rd.Subresource(0, 0, 0)


def body(ctrl, log):
    names = res_names(ctrl)
    byid = {int(r): r for r in names}
    buf = byid[1753]
    sf = ctrl.GetStructuredFile()
    acts = {a.eventId: a for a, p in flatten(ctrl.GetRootActions())}
    ws = sorted(set(u.eventId for u in ctrl.GetUsage(buf) if "CS_RW" in str(u.usage)))
    log("dispatches with 1753 bound as RW:", ws)
    # bindings (offset/range) of 1753 in each writer
    for eid in ws:
        if eid > 16000: continue
        ctrl.SetFrameEvent(eid, False)
        st = ctrl.GetPipelineState()
        refl = st.GetShaderReflection(rd.ShaderStage.Compute)
        b = []
        for u in st.GetReadWriteResources(rd.ShaderStage.Compute, True):
            if u.descriptor.resource == buf:
                r = refl.readWriteResources[u.access.index]
                b.append("%s@%d+%d" % (r.name, u.descriptor.byteOffset, u.descriptor.byteSize))
        log("  %d %s cs=%d %s" % (eid, acts[eid].GetName(sf), int(st.GetShader(rd.ShaderStage.Compute)), b))
    for eid in (15947, 15976):
        log("\nrepeated replays, state after eid %d:" % eid)
        for it in range(5):
            ctrl.SetFrameEvent(29213 if it % 2 == 0 else 1, True)
            ctrl.SetFrameEvent(eid, True)
            ll = ctrl.GetBufferData(buf, LIGHTLIST_OFF, LIST_RANGE)
            pl = ctrl.GetBufferData(buf, PROBELIST_OFF, LIST_RANGE)
            cl = struct.unpack_from("<8I", ll, CLUSTER * 256)
            nonempty = sum(1 for c in range(LIST_RANGE // 256) if struct.unpack_from("<I", ll, c * 256)[0] & 255)
            pne = sum(1 for c in range(LIST_RANGE // 256) if struct.unpack_from("<I", pl, c * 256)[0] & 255)
            log("  run %d: lightlist md5=%s nonempty clusters=%d  cluster%d=%s | probelist md5=%s nonempty=%d" % (
                it, hashlib.md5(ll).hexdigest()[:10], nonempty, CLUSTER, cl, hashlib.md5(pl).hexdigest()[:10], pne))

run("s9_listrepeat.log", body)
