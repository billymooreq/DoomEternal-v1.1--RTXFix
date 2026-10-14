import os, sys, struct, zlib
sys.path.insert(0, os.environ.get("DOOMFIX_SCRIPTS", "."))  # set by analysis/run_analysis.ps1
import renderdoc as rd
from rdcommon import *
exec(open(os.path.join(SCRIPTS_DIR, "s3_drawinfo.py")).read().split("def body")[0].split("from rdcommon import *")[1])

DRAWS = [15960, 15965, 15970, 15976, 15981, 15986, 15991, 15996]
PASS_END = 18974
W, H = 1920, 1080


def f11(v, mbits):
    e = (v >> mbits) & 31; m = v & ((1 << mbits) - 1)
    if e == 0: return m / float(1 << mbits) * 2.0 ** -14
    if e == 31: return float("nan") if m else float("inf")
    return (1 + m / float(1 << mbits)) * 2.0 ** (e - 15)


def r11g11b10(u):
    return f11(u & 0x7ff, 6), f11((u >> 11) & 0x7ff, 6), f11((u >> 22) & 0x3ff, 5)


def body(ctrl, log):
    names = res_names(ctrl)
    tgt = [r for r, n in names.items() if n.endswith(" 53374")][0]
    ctrl.SetFrameEvent(PASS_END, True)
    col = ctrl.GetTextureData(tgt, SUB)
    log("53374 bytes", len(col))
    owner = {}
    for d in DRAWS:
        t, data = overlay_mask(ctrl, log, d, tgt, W, H)
        n = 0
        for i in range(W * H):
            r, g, b, a_ = struct.unpack_from("<4e", data, i * 8)
            if a_ > 0 and g > 0.5 and r < 0.5:
                owner[i] = d; n += 1   # later draws override (approximation of final visibility)
        log("draw %d depth-pass px %d" % (d, n))
    stats = {}
    best = {}
    rows = [bytearray(W * 3) for _ in range(H)]
    for i, d in owner.items():
        u = struct.unpack_from("<I", col, i * 4)[0]
        r, g, b = r11g11b10(u)
        s = stats.setdefault(d, [0, 0, 0.0])
        s[0] += 1
        lum = r + g + b
        if lum > 0: s[1] += 1
        s[2] = max(s[2], lum)
        if lum > best.get(d, (0,))[0]:
            best[d] = (lum, i % W, i // W, r, g, b)
        y, x = divmod(i, W)
        v = min(255, int(lum * 2000)) if lum > 0 else 0
        rows[y][x * 3:x * 3 + 3] = bytes((v, v, 255 if lum == 0 else v))
    for d in DRAWS:
        log("draw %d: px=%s nonzero=%s max=%s best=%s" % (d, *(stats.get(d, [0, 0, 0])), best.get(d)))
    write_png(OUTDIR + r"\weapon_draws_zero_map.png", W, H, [bytes(r) for r in rows])
    # any nonzero pixel in whole weapon bbox region at all?
    nz = []
    for y in range(600, 1080, 2):
        for x in range(1000, 1920, 2):
            u = struct.unpack_from("<I", col, (y * W + x) * 4)[0]
            if u and (y * W + x) in owner:
                nz.append((x, y))
    log("nonzero weapon pixels (2px grid):", len(nz), nz[:20])

run("s3b_weapon_masks.log", body)
