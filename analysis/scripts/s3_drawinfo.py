import sys, struct, zlib
sys.path.insert(0, r"C:\Users\vippr\Documents\doomfix\analysis\scripts")
import renderdoc as rd
from rdcommon import *

DRAW = 15976
SUB = rd.Subresource(0, 0, 0)


def write_png(path, w, h, rows):  # rows: list of bytes (RGB)
    raw = b"".join(b"\x00" + r for r in rows)
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def overlay_mask(ctrl, log, eid, texid, w, h):
    ctrl.SetFrameEvent(eid, True)
    out = ctrl.CreateOutput(rd.CreateHeadlessWindowingData(w, h), rd.ReplayOutputType.Texture)
    td = rd.TextureDisplay()
    td.resourceId = texid
    td.overlay = rd.DebugOverlay.Depth
    out.SetTextureDisplay(td)
    out.Display()
    ov = out.GetDebugOverlayTexID()
    log("overlay tex", int(ov))
    t = type("T", (), {"width": w, "height": h})
    data = ctrl.GetTextureData(ov, SUB)
    out.Shutdown()
    return t, data


def body(ctrl, log):
    sf = ctrl.GetStructuredFile()
    names = res_names(ctrl); texs = tex_map(ctrl)
    flat = flatten(ctrl.GetRootActions())
    # find enclosing render pass
    begin = end = None
    for a, p in flat:
        if a.eventId <= DRAW and (a.flags & rd.ActionFlags.BeginPass) and not (a.flags & rd.ActionFlags.CommandBufferBoundary):
            begin = a.eventId
        if a.eventId >= DRAW and (a.flags & rd.ActionFlags.EndPass) and end is None and not (a.flags & rd.ActionFlags.CommandBufferBoundary):
            end = a.eventId
    log("render pass containing %d: %s..%s" % (DRAW, begin, end))
    draws = [a for a, p in flat if begin < a.eventId < end and (a.flags & rd.ActionFlags.Drawcall)]
    log("draws in pass:", len(draws))
    for a in draws[:40]:
        ctrl.SetFrameEvent(a.eventId, False)
        st = ctrl.GetPipelineState()
        vs = st.GetShader(rd.ShaderStage.Vertex); ps = st.GetShader(rd.ShaderStage.Pixel)
        log("  %6d idx=%d inst=%d vs=%d ps=%d pipe=%d" % (a.eventId, a.numIndices, a.numInstances, int(vs), int(ps), int(st.GetGraphicsPipelineObject())))
    # overlay mask of DRAW on the colour target
    tgt = [r for r, n in names.items() if n.endswith(" 53374")][0]
    t, data = overlay_mask(ctrl, log, DRAW, tgt, 1920, 1080)
    bpp = len(data) // (t.width * t.height)
    log("overlay bytes", len(data), "bpp", bpp)
    W, H = t.width, t.height
    mask = bytearray(W * H)
    cnt = 0; x0 = y0 = 1 << 30; x1 = y1 = -1
    rows = []
    for y in range(H):
        row = bytearray(W * 3)
        for x in range(W):
            o = (y * W + x) * bpp
            if bpp == 8:
                r, g, b, a_ = struct.unpack_from("<4e", data, o)
            else:
                r, g, b, a_ = [c / 255.0 for c in data[o:o + 4]]
            if a_ > 0 and g > 0.5 and r < 0.5:   # green = depth passed
                mask[y * W + x] = 1; cnt += 1
                x0 = min(x0, x); x1 = max(x1, x); y0 = min(y0, y); y1 = max(y1, y)
                row[x * 3:x * 3 + 3] = b"\x00\xff\x00"
            elif a_ > 0 and r > 0.5:
                mask[y * W + x] = 2
                row[x * 3:x * 3 + 3] = b"\xff\x00\x00"
        rows.append(bytes(row))
    log("depth-pass pixels of draw %d: %d bbox x %d..%d y %d..%d; mask[1437,917]=%d" % (DRAW, cnt, x0, x1, y0, y1, mask[917 * W + 1437]))
    write_png(OUTDIR + r"\draw%d_depth_overlay.png" % DRAW, W, H, rows)
    with open(OUTDIR + r"\draw%d_mask.bin" % DRAW, "wb") as f:
        f.write(mask)
    # save colour target at end of this pass and final swapchain for the report
    for eid, suf, fn in [(end, "53374", "scene_hdr_after_pass.png"), (29213, "685990", "final_swapchain.png"), (29213, "53390", "post_53390.png")]:
        ctrl.SetFrameEvent(eid, True)
        rid = [r for r, n in names.items() if n.endswith(" " + suf)][0]
        ts = rd.TextureSave()
        ts.resourceId = rid
        ts.destType = rd.FileType.PNG
        ts.alpha = rd.AlphaMapping.Discard
        ts.comp.blackPoint = 0.0
        ts.comp.whitePoint = 1.0
        log("save", fn, ctrl.SaveTexture(ts, OUTDIR + "\\" + fn))
    # also dump raw float values of 53374 inside mask for lit/dark selection: sample every 4 px
    ctrl.SetFrameEvent(end, True)
    samples = []
    for y in range(y0, y1 + 1, 4):
        for x in range(x0, x1 + 1, 4):
            if mask[y * W + x] == 1:
                v = ctrl.PickPixel(tgt, x, y, SUB, rd.CompType.Typeless).floatValue
                samples.append((x, y, v[0], v[1], v[2]))
    log("sampled", len(samples))
    with open(OUTDIR + r"\draw%d_samples.csv" % DRAW, "w") as f:
        f.write("x,y,r,g,b\n")
        for s in samples:
            f.write("%d,%d,%.6g,%.6g,%.6g\n" % s)

run("s3_drawinfo.log", body)
