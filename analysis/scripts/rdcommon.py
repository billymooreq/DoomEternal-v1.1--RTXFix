# Common helpers for running analysis under `qrenderdoc --python`.
# The capture is opened read-only through the replay API (never saved).
import os, sys, time, traceback
import renderdoc as rd

# Paths come from the environment; analysis/run_analysis.ps1 sets them and checks them first:
#   DOOMFIX_SCRIPTS    this folder (qrenderdoc --python provides neither __file__ nor argv)
#   DOOMFIX_CAPTURE    the .rdc to open
#   DOOMFIX_OUT        optional subfolder of analysis/ for the results (default: analysis/ itself)
#   DOOMFIX_RENDERDOC  RenderDoc install folder (default: the folder of the running qrenderdoc.exe)
SCRIPTS_DIR = os.environ.get("DOOMFIX_SCRIPTS", "")
CAPTURE = os.environ.get("DOOMFIX_CAPTURE", "")
RENDERDOC_DIR = os.environ.get("DOOMFIX_RENDERDOC") or os.path.dirname(sys.executable)


def _fail(message):
    """qrenderdoc has no console: leave the error in %TEMP% and quit instead of hanging."""
    path = os.path.join(os.environ.get("TEMP", "."), "doomfix_analysis_error.txt")
    with open(path, "w", encoding="utf-8") as f:
        f.write(message + "\n")
    os._exit(1)


if not SCRIPTS_DIR or not os.path.isfile(os.path.join(SCRIPTS_DIR, "rdcommon.py")):
    _fail("DOOMFIX_SCRIPTS is not set to analysis/scripts. Run the scripts through analysis/run_analysis.ps1.")
if not CAPTURE or not os.path.isfile(CAPTURE):
    _fail("DOOMFIX_CAPTURE is not set or the file does not exist: %r. "
          "Pass -Capture <path to .rdc> to analysis/run_analysis.ps1." % CAPTURE)
BASE_OUTDIR = os.path.dirname(os.path.abspath(SCRIPTS_DIR))
OUTDIR = os.path.join(BASE_OUTDIR, os.environ["DOOMFIX_OUT"]) if os.environ.get("DOOMFIX_OUT") else BASE_OUTDIR
os.makedirs(OUTDIR, exist_ok=True)


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
    log("capture %s opened in %.1fs" % (CAPTURE, time.time() - t))
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
