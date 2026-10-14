# Descriptors (with buffer offsets/ranges) of the binning dispatches 15760..15884.
import os, sys
sys.path.insert(0, os.environ.get("DOOMFIX_SCRIPTS", "."))  # set by analysis/run_analysis.ps1
import renderdoc as rd
from rdcommon import *


def body(ctrl, log):
    sf = ctrl.GetStructuredFile()
    names = res_names(ctrl)
    for a, p in flatten(ctrl.GetRootActions()):
        if not (15760 <= a.eventId <= 15884 and (a.flags & rd.ActionFlags.Dispatch)): continue
        if a.eventId in range(15783, 15818) and a.eventId not in (15781, 15794, 15807): continue
        if a.eventId in range(15833, 15874) and a.eventId not in (15846, 15861): continue
        ctrl.SetFrameEvent(a.eventId, False)
        st = ctrl.GetPipelineState()
        refl = st.GetShaderReflection(rd.ShaderStage.Compute)
        log("-- %d %s cs=%d" % (a.eventId, a.GetName(sf), int(st.GetShader(rd.ShaderStage.Compute))))
        for kind, lst, arr in (("RO", st.GetReadOnlyResources(rd.ShaderStage.Compute, True), refl.readOnlyResources),
                               ("RW", st.GetReadWriteResources(rd.ShaderStage.Compute, True), refl.readWriteResources)):
            for u in lst:
                r = arr[u.access.index]
                log("   %s %-36s %s @%d+%d" % (kind, r.name, names.get(u.descriptor.resource), u.descriptor.byteOffset, u.descriptor.byteSize))

run("s15_binning_desc.log", body)
