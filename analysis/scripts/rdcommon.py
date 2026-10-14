# Common helpers for running analysis under `qrenderdoc --python`.
# The capture is opened read-only through the replay API (never saved).
import os, sys, time, traceback
import renderdoc as rd

CAPTURE = r"C:\Users\vippr\Documents\doomfix\capture1.rdc"
OUTDIR = r"C:\Users\vippr\Documents\doomfix\analysis"


class Log:
    def __init__(self, name):
        self.f = open(os.path.join(OUTDIR, name), "w", encoding="utf-8")
        self.t0 = time.time()

    def __call__(self, *a):
        self.f.write(" ".join(str(x) for x in a) + "\n")
        self.f.flush()

    def close(self):
        self.f.close()


def open_capture(log):
    cap = rd.OpenCaptureFile()
    res = cap.OpenFile(CAPTURE, "", None)
    if res != rd.ResultCode.Succeeded:
        raise RuntimeError("OpenFile failed: %s" % res)
    if not cap.LocalReplaySupport():
        raise RuntimeError("no local replay support")
    t = time.time()
    res, ctrl = cap.OpenCapture(rd.ReplayOptions(), None)
    if res != rd.ResultCode.Succeeded:
        raise RuntimeError("OpenCapture failed: %s" % res)
    log("capture opened in %.1fs" % (time.time() - t))
    return cap, ctrl


def run(logname, body):
    """Run body(ctrl, log) and always terminate the qrenderdoc process."""
    log = Log(logname)
    cap = ctrl = None
    try:
        cap, ctrl = open_capture(log)
        body(ctrl, log)
    except Exception:
        log("EXCEPTION:\n" + traceback.format_exc())
    finally:
        try:
            if ctrl: ctrl.Shutdown()
            if cap: cap.Shutdown()
        except Exception:
            pass
        log("done in %.1fs" % (time.time() - log.t0))
        log.close()
        os._exit(0)


def flatten(actions, out=None, parents=()):
    if out is None:
        out = []
    for a in actions:
        out.append((a, parents))
        flatten(a.children, out, parents + (a,))
    return out


def res_names(ctrl):
    return {r.resourceId: r.name for r in ctrl.GetResources()}


def tex_map(ctrl):
    return {t.resourceId: t for t in ctrl.GetTextures()}


def buf_map(ctrl):
    return {b.resourceId: b for b in ctrl.GetBuffers()}
