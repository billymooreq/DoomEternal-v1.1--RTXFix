# Locate the light list the shader debugger saw ([4, 0, 1, 23, 29]) inside Buffer 1753 and check the
# first-person shadow / light parameters of light 0 and light 29.
import sys, os, struct
sys.path.insert(0, r"C:\Users\vippr\Documents\doomfix\analysis\scripts")
import renderdoc as rd
from rdcommon import *


def body(ctrl, log):
    names = res_names(ctrl)
    byid = {int(r): r for r in names}
    buf = byid[1753]
    for eid in (1, 15976, 27840, 29213):
        ctrl.SetFrameEvent(eid, True)
        data = ctrl.GetBufferData(buf, 0, 0)
        n = len(data) // 4
        words = struct.unpack("<%dI" % n, data)
        hits = [i for i in range(n - 4) if words[i] & 255 == 4 and words[i + 1:i + 5] == (0, 1, 23, 29)]
        log("after eid %d: buffer %d bytes; [hash&255=4, 0,1,23,29] found at word indices %s" % (eid, len(data), hits[:20]))
        for h in hits[:5]:
            region = "lightbitfield" if h * 4 < 384000 else "lightlist" if h * 4 < 1152000 else "probelist" if h * 4 < 1920000 else "other"
            log("    word %d = byte %d -> %s region, cluster idx if lightlist: %s; words=%s" % (h, h * 4, region, (h * 4 - 384000) / 256.0, words[h:h + 6]))
        log("    word at byte 700416 (index 175104 without descriptor offset): %s" % (words[175104:175110],))
        log("    word at byte 1084416 (index 175104 with offset 384000):     %s" % (words[271104:271110],))
        log("    word at byte 1852416 (probelist w/ offset):                 %s" % (words[463104:463106],))
    # light parameters (UBO 1062/1063 = lightparms1/2, 4 float4 each)
    for rid, nm in ((1062, "lightparms1"), (1063, "lightparms2")):
        d = ctrl.GetBufferData(byid[rid], 0, 0)
        for lid in (0, 1, 23, 29):
            v = struct.unpack_from("<16f", d, lid * 64)
            u = struct.unpack_from("<16I", d, lid * 64)
            log("%s[light %d] = %s  (w0 as uint=0x%08x, [3].x uint=0x%08x)" % (nm, lid, tuple(round(x, 4) for x in v), u[3], u[12]))

run("s10_findseq.log", body)
