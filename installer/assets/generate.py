"""Generates the installer artwork: wizard backgrounds, buttons and other controls, and the "DF" icon.

Everything is drawn procedurally (Pillow + numpy) from fixed random seeds, so every run produces the same
images; no third-party artwork is used. The only external asset is the Inter Display Black font (SIL OFL 1.1,
see fonts/OFL.txt), which is baked into the images and is not shipped in the installer.

Run it through scripts\\build_assets.ps1, which sets up the Python environment. Output, next to this file:
  back_welcome_<w>x<h>.png  welcome and finish pages: fire, large DOOMFIX logo and RTX tag, the content panel
  back_inner_<w>x<h>.png    other pages: fire, small DOOMFIX logo and RTX tag at the top left, the content panel
                            with its red heading line
  ui/<name>_<w>.png         for each background width w: the buttons (button_*, browse_*: normal, hover, pressed,
                            focus, disabled; without text), the path field (edit), the check boxes (check_on_*,
                            check_off_*) and the heading lines of the welcome and finish pages (line_red, line_green)
  doomfix.ico               the icon of setup.exe and doomfix_launcher.exe, 16 to 256 px

The background sizes are the wizard image areas of Inno Setup 6.7 at 100, 125, 150, 175 and 200% DPI (see the
WizardBackImageFile help topic); Setup picks the best match. The panels use the coordinates of the 497x360
design, the controls pixels of the 596x432 client area at 100% (installer/doomfix.iss places everything with the
same numbers).
"""
import math
import random
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent
FONT = HERE / "fonts" / "InterDisplay-Black.ttf"

BASE_W, BASE_H = 497, 360
SIZES = [(596, 432), (796, 576), (994, 720), (1193, 864), (1272, 922)]
ICON_SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]

LOGO_RED = (235, 30, 25)
LOGO_STROKE = (10, 4, 3)
LOGO_GLOW = (255, 60, 10)
GREEN = (60, 220, 60)
TAG_FILL = (12, 12, 12)


def font(size):
    from PIL import ImageFont
    return ImageFont.truetype(str(FONT), max(1, int(round(size))))


# --- Noise and fire (from design-reference/gen3.py) ------------------------------------------

def fbm(w, h, seed, octaves=5, base=4):
    rng = np.random.default_rng(seed)
    acc = np.zeros((h, w))
    amp, tot = 1.0, 0.0
    for o in range(octaves):
        cw, ch = base * 2 ** o, max(2, int(base * 2 ** o * h / w))
        n = rng.random((ch, cw))
        im = Image.fromarray((n * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC)
        acc += np.asarray(im, float) / 255 * amp
        tot += amp
        amp *= 0.5
    return acc / tot


def ramp(t, stops):
    t = np.clip(t, 0, 1)
    out = np.zeros(t.shape + (3,))
    for (p0, c0), (p1, c1) in zip(stops, stops[1:]):
        m = (t >= p0) & (t <= p1)
        k = ((t - p0) / (p1 - p0 + 1e-9))[..., None]
        out[m] = (np.array(c0) * (1 - k) + np.array(c1) * k)[m]
    return out


def ridge(w, h, base_y, rough, seed, step):
    rnd = random.Random(seed)
    pts, y = [], base_y
    x = -step
    while x <= w + step:
        y += rnd.uniform(-rough, rough)
        y = min(max(y, base_y - rough * 4), base_y + rough * 2)
        spike = rnd.random() < 0.18
        pts.append((x, y - (rough * rnd.uniform(2, 4) if spike else 0)))
        x += step * rnd.uniform(0.6, 1.4)
    return pts


def glow(img, mask, color, radius, strength=1.0):
    blur = mask.filter(ImageFilter.GaussianBlur(radius))
    a = np.asarray(blur, float)[..., None] / 255 * strength
    out = np.asarray(img, float) + np.array(color, float) * a
    return Image.fromarray(out.clip(0, 255).astype(np.uint8))


def hellscape(w, h, seed=7, embers=True):
    """Sky with smoke, three rock ridges with hot rims, lava with a soft edge, embers, vignette.

    Noise and positions are defined on the 596x432 grid and scaled, so every size shows the same picture."""
    k = w / 596
    y = np.linspace(0, 1, h)[:, None] * np.ones((1, w))
    clouds = fbm(w, h, seed, 6, 3)
    sky_t = np.clip(y * 1.25 + (clouds - 0.5) * 0.45, 0, 1)
    sky = ramp(sky_t, [(0, (35, 4, 2)), (0.35, (120, 18, 6)), (0.62, (230, 70, 12)), (0.8, (255, 150, 40)),
                       (1, (255, 205, 110))])
    smoke = np.clip((clouds - 0.48) * 2.4, 0, 1) * np.clip(1.2 - y * 1.3, 0, 1)
    sky *= (1 - smoke * 0.75)[..., None]
    img = Image.fromarray(sky.clip(0, 255).astype(np.uint8))

    layers = [(0.50, 0.025, (95, 22, 10), 41, 0.05),
              (0.60, 0.035, (50, 10, 6), 42, 0.06),
              (0.70, 0.05, (16, 5, 4), 43, 0.07)]
    for by, rough, col, sd, step in layers:
        pts = [(x * k, yy * k) for x, yy in ridge(596, 432, 432 * by, 596 * rough, sd, 596 * step)]
        poly = pts + [(w + 50, h), (-50, h)]
        m = Image.new("L", (w, h), 0)
        ImageDraw.Draw(m).polygon(poly, fill=255)
        img.paste(Image.new("RGB", (w, h), col), (0, 0), m)
        rim = Image.new("L", (w, h), 0)
        ImageDraw.Draw(rim).line(pts, fill=200, width=max(1, round(596 / 160 * k)))
        img = glow(img, rim, (255, 120, 30), w * 0.01, 0.8)

    ly = int(h * 0.80)
    band = h * 0.10
    lava_n = fbm(w, h, seed + 5, 5, 6)
    veins = np.abs(lava_n - 0.5) * 2
    lava = ramp(1 - veins, [(0, (40, 6, 2)), (0.55, (150, 25, 5)), (0.8, (255, 110, 15)), (0.93, (255, 200, 80)),
                            (1, (255, 245, 190))])
    edge_noise = (fbm(w, h, seed + 31, 4, 8) - 0.5) * band * 1.6
    yy = np.arange(h)[:, None] * np.ones((1, w))
    t = np.clip((yy - (ly - band) + edge_noise) / (band * 1.6), 0, 1)
    t = t * t * (3 - 2 * t)
    base = np.asarray(img, float)
    lit = base + np.array([255, 90, 20]) * (np.clip((yy - (ly - band * 2.2)) / (band * 2.2), 0, 1) ** 2 * 0.45)[..., None]
    mix = lit * (1 - t[..., None]) + lava * t[..., None]
    img = Image.fromarray(mix.clip(0, 255).astype(np.uint8))
    hg = Image.new("L", (w, h), 0)
    ImageDraw.Draw(hg).rectangle([0, ly - band * 0.6, w, ly + band * 0.3], fill=255)
    img = glow(img, hg, (255, 90, 20), h * 0.07, 0.45)

    if embers:
        # Positions on the 596x432 grid, so the sparks stay in place at every size.
        rnd = random.Random(seed + 9)
        em = Image.new("L", (w, h), 0)
        d = ImageDraw.Draw(em)
        for _ in range(int(596 * 432 / 900)):
            x, ey = rnd.uniform(0, 596) * k, 432 * rnd.random() ** 0.7 * k
            r = rnd.uniform(0.5, 1.6) * 596 / 330 * k
            d.ellipse([x - r, ey - r, x + r, ey + r], fill=int(rnd.uniform(120, 255)))
        img = glow(img, em, (255, 110, 20), w / 160, 1.4)
        img = glow(img, em, (255, 230, 150), 0.3 * k, 1.0)

    vx = np.linspace(-1, 1, w)[None, :]
    vy = np.linspace(-1, 1, h)[:, None]
    vig = 1 - 0.45 * np.clip(vx ** 2 * 0.8 + vy ** 2 * 0.5, 0, 1)
    arr = np.asarray(img, float) * vig[..., None] * 1.1
    return Image.fromarray(arr.clip(0, 255).astype(np.uint8))


# --- Lettering (from design-reference/gen2.py and gen5.py) ------------------------------------

def spaced(d, xy, text, fnt, fill, track=0.0, anchor_center=None):
    x, y = xy
    widths = [d.textlength(ch, font=fnt) + track for ch in text]
    total = sum(widths) - track
    if anchor_center is not None:
        x = anchor_center - total / 2
    for ch, wd in zip(text, widths):
        d.text((x, y), ch, font=fnt, fill=fill)
        x += wd
    return total


def logo(img, text, x, y, size, s, center=False, stroke=7):
    """Red letters with a dark outline and a fiery glow. Coordinates are in design units, s = pixels per unit."""
    m = Image.new("L", img.size, 0)
    md = ImageDraw.Draw(m)
    fnt = font(size * s)
    if center:
        spaced(md, (0, y * s), text, fnt, 255, track=1.2 * s, anchor_center=x * s)
    else:
        spaced(md, (x * s, y * s), text, fnt, 255, track=1.0 * s)
    img = glow(img, m, LOGO_GLOW, 6 * s, 1.1)
    img.paste(LOGO_STROKE, (0, 0), m.filter(ImageFilter.MaxFilter(int(stroke * s) | 1)))
    img.paste(LOGO_RED, (0, 0), m)
    return img


def tag(img, x_center, y, size, s, text="RTX"):
    d = ImageDraw.Draw(img)
    fnt = font(size * s)
    tw = sum(d.textlength(c, font=fnt) + 1 * s for c in text)
    x0 = x_center * s - tw / 2 - 8 * s
    d.rectangle([x0, y * s, x0 + tw + 16 * s, (y + size + 7) * s], fill=TAG_FILL, outline=GREEN,
                width=max(1, round(s)))
    spaced(d, (0, (y + 3) * s), text, fnt, GREEN, track=1 * s, anchor_center=x_center * s)
    return img


# Layout shared with the "Look" part of installer/doomfix.iss. Two kinds of units: design units of the 497x360
# layout (panels, labels; s = pixels per unit) and pixels of the window at 100% DPI, whose client area is 596x432
# (button, field and chamfer sizes; p = pixels per pixel at 100%). The panels are part of the background image: the
# custom style draws transparent controls over the window background, so a panel made of a control would not show
# behind every control.
WELCOME_PANEL = (40, 104, 455, 279)   # welcome and finish pages
WELCOME_SEPARATOR_Y = 144
INNER_PANEL = (18, 50, 479, 306)      # all other pages
INNER_SEPARATOR_Y = 102
PANEL_PADDING = 16

BORDER = (0x7A, 0x5F, 0x57)
RED = (0xD8, 0x26, 0x1A)
DARK_RED = (0x8E, 0x14, 0x0C)
GREEN_LINE = (0x3D, 0xDC, 0x56)
SS = 4                                # supersampling of the antialiased shapes


def chamfer_points(x0, y0, x1, y1, tl, tr, br, bl):
    """A rectangle with its corners cut at 45 degrees; tl..bl are the cut lengths along each side."""
    return [(x0 + tl, y0), (x1 - tr, y0), (x1, y0 + tr), (x1, y1 - br), (x1 - br, y1), (x0 + bl, y1),
            (x0, y1 - bl), (x0, y0 + tl)]


def shape_mask(size, points, inset=0.0):
    """Antialiased coverage (0..1, float array) of a polygon, optionally shrunk by `inset` pixels."""
    w, h = size
    k = int(round(inset * SS))
    pad = k + SS  # empty margin, so that the shrinking also works where the shape touches the image edge
    m = Image.new("L", (w * SS + 2 * pad, h * SS + 2 * pad), 0)
    ImageDraw.Draw(m).polygon([(x * SS + pad, y * SS + pad) for x, y in points], fill=255)
    if k > 0:
        m = m.filter(ImageFilter.MinFilter(2 * k + 1))
    m = m.crop((pad, pad, pad + w * SS, pad + h * SS))
    return np.asarray(m.resize((w, h), Image.BOX), float) / 255


def outline_mask(size, points, width):
    """The band of `width` pixels just inside the polygon's edge."""
    return np.clip(shape_mask(size, points) - shape_mask(size, points, width), 0, 1)


def over(base, color, alpha):
    """Composites a color (RGB tuple or HxWx3 array) with per-pixel alpha (HxW) over an RGB float array."""
    a = alpha[..., None]
    return base * (1 - a) + np.asarray(color, float) * a


def scanlines(h, w, p, y0=0):
    """1 = row of a scanline: 1 px lines every 3 px at 100%, scaled."""
    step = max(2, round(3 * p))
    lh = max(1, round(p))
    rows = ((np.arange(h) - y0) % step) < lh
    return np.repeat(rows[:, None], w, axis=1).astype(float)


def vgradient(h, w, top, bottom, y0=0, y1=None):
    """HxWx3 colors from `top` at row y0 to `bottom` at row y1."""
    y1 = h - 1 if y1 is None else y1
    t = np.clip((np.arange(h) - y0) / max(1, y1 - y0), 0, 1)[:, None, None]
    col = np.asarray(top, float) * (1 - t) + np.asarray(bottom, float) * t
    return np.repeat(col, w, axis=1)


def panel(img, box, s, p):
    """Dark see-through panel: chamfered corners (18 px top left and bottom right, 6 px elsewhere), 1 px frame,
    vertical gradient, faint scanlines and a faint inner line 5 px from the edge."""
    w, h = img.size
    x0, y0, x1, y1 = (v * s for v in box)
    big, small = 18 * p, 6 * p
    lw = max(1, round(p))
    pts = chamfer_points(x0, y0, x1, y1, big, small, big, small)
    fill = shape_mask((w, h), pts)
    base = np.asarray(img, float)
    ya, yb = int(y0), int(y1)
    color = vgradient(h, w, (28, 22, 21), (16, 12, 12), ya, yb)
    alpha = (0.86 + 0.04 * np.clip((np.arange(h) - ya) / max(1, yb - ya), 0, 1))[:, None]
    base = over(base, color, fill * alpha)
    base = over(base, (255, 255, 255), fill * scanlines(h, w, p, ya) * 0.045)
    d = 5 * p
    inner = chamfer_points(x0 + d, y0 + d, x1 - d, y1 - d, 14 * p, 4 * p, 14 * p, 4 * p)
    base = over(base, (255, 255, 255), outline_mask((w, h), inner, lw) * 0.06)
    base = over(base, BORDER, outline_mask((w, h), pts, lw))
    return Image.fromarray(base.clip(0, 255).astype(np.uint8))


def separator_line(length, height, start):
    """RGBA line under a heading: `start` color to faint white at 60% of the length, then fading out."""
    t = np.linspace(0, 1, length)
    k = np.clip(t / 0.6, 0, 1)[:, None]
    rgb = np.asarray(start, float) * (1 - k) + np.array([255.0, 255, 255]) * k
    a = np.where(t <= 0.6, 1 - 0.88 * np.clip(t / 0.6, 0, 1), 0.12 * (1 - (t - 0.6) / 0.4))
    row = np.concatenate([rgb, (a * 255)[:, None]], axis=1)
    return Image.fromarray(np.repeat(row[None], height, axis=0).clip(0, 255).astype(np.uint8), "RGBA")


def separator_geometry(box, sep_y, s, p):
    x0, _, x1, _ = box
    left = round((x0 + PANEL_PADDING) * s)
    return left, round(sep_y * s), round((x1 - PANEL_PADDING) * s) - left, max(1, round(p))


def background(kind, w, h):
    s, p = w / BASE_W, w / 596
    if kind == "welcome":
        # The heading line is not part of this image: it is red on the welcome page and green on the finish page
        # (ui/line_*.png, placed by installer/doomfix.iss).
        img = hellscape(w, h, seed=7)
        img = logo(img, "DOOMFIX", BASE_W / 2, 14, 34, s, center=True)
        img = tag(img, BASE_W / 2, 60, 11, s)
        img = panel(img, WELCOME_PANEL, s, p)
    else:
        img = hellscape(w, h, seed=19)
        img = logo(img, "DOOMFIX", 18, 10, 16, s, stroke=5)
        img = tag(img, 136, 11, 9, s)
        img = panel(img, INNER_PANEL, s, p)
        x, y, length, lh = separator_geometry(INNER_PANEL, INNER_SEPARATOR_Y, s, p)
        img = img.convert("RGBA")
        img.alpha_composite(separator_line(length, lh, RED), (x, y))
        img = img.convert("RGB")
    return img


# --- Buttons and the path field ------------------------------------------------------------------
# Drawn without text; installer/doomfix.iss writes the caption on them at run time (the captions depend on the
# language and the page). One set per background size, named after the background width.

BUTTON_W, BROWSE_W, BUTTON_H = 104, 96, 34
EDIT_W = 410                          # inner page content width (514 px) - Browse - 8 px gap
BUTTON_STATES = ("normal", "hover", "pressed", "focus", "disabled")


def button_image(w, h, p, state):
    """A stretched hexagon: the left and right sides meet in a point 10 px deep."""
    d = 10 * p
    pts = [(d, 0), (w - d, 0), (w, h / 2), (w - d, h), (d, h), (0, h / 2)]
    lw = max(1, round(p))
    fill = shape_mask((w, h), pts)
    base = np.zeros((h, w, 3))
    if state == "hover":
        base = vgradient(h, w, RED, DARK_RED)
        border = (0xFF, 0xB0, 0x8A)
    elif state == "pressed":
        base[:] = DARK_RED
        border = (0xFF, 0xB0, 0x8A)
    else:
        base = vgradient(h, w, (0x2C, 0x24, 0x23), (0x1A, 0x15, 0x15))
        border = (0xF0, 0xE6, 0xE2) if state == "focus" else BORDER
    base = over(base, (255, 255, 255), scanlines(h, w, p) * 0.045)
    base = over(base, border, np.clip(outline_mask((w, h), pts, lw) / np.maximum(fill, 1e-6), 0, 1))
    alpha = fill * (0.45 if state == "disabled" else 1.0)
    rgba = np.concatenate([base, (alpha * 255)[..., None]], axis=2)
    return Image.fromarray(rgba.clip(0, 255).astype(np.uint8), "RGBA")


def edit_frame(w, h, p):
    """The path field: 1 px frame, 8 px chamfers at the top left and bottom right, dark fill."""
    c = 8 * p
    pts = chamfer_points(0, 0, w, h, c, 0, c, 0)
    fill = shape_mask((w, h), pts)
    base = np.zeros((h, w, 3))
    base[:] = (0x12, 0x0E, 0x0E)
    base = over(base, BORDER, np.clip(outline_mask((w, h), pts, max(1, round(p))) / np.maximum(fill, 1e-6), 0, 1))
    rgba = np.concatenate([base, (fill * 255)[..., None]], axis=2)
    return Image.fromarray(rgba.clip(0, 255).astype(np.uint8), "RGBA")


CHECK_SIZE = 16


def check_box(n, p, checked, state):
    """A check box: dark square with a 1 px frame, a red tick when checked."""
    lw = max(1, round(p))
    pts = [(0, 0), (n, 0), (n, n), (0, n)]
    fill = shape_mask((n, n), pts)
    base = np.zeros((n, n, 3))
    base[:] = (0x12, 0x0E, 0x0E)
    border = {"hover": (0xFF, 0xB0, 0x8A), "focus": (0xF0, 0xE6, 0xE2)}.get(state, BORDER)
    if checked:
        tick = Image.new("L", (n * SS, n * SS), 0)
        k = n * SS / 16
        ImageDraw.Draw(tick).line([(3.5 * k, 8.5 * k), (6.5 * k, 11.5 * k), (12.5 * k, 4.5 * k)], fill=255,
                                  width=max(2, round(2.2 * k)), joint="curve")
        base = over(base, RED, np.asarray(tick.resize((n, n), Image.BOX), float) / 255)
    base = over(base, border, outline_mask((n, n), pts, lw))
    rgba = np.concatenate([base, (fill * 255)[..., None]], axis=2)
    return Image.fromarray(rgba.clip(0, 255).astype(np.uint8), "RGBA")


def ui_images(w):
    """{file name: image} for the background width w."""
    s, p = w / BASE_W, w / 596
    bh = round(BUTTON_H * p)
    out = {}
    for state in BUTTON_STATES:
        out[f"button_{state}_{w}.png"] = button_image(round(BUTTON_W * p), bh, p, state)
        out[f"browse_{state}_{w}.png"] = button_image(round(BROWSE_W * p), bh, p, state)
    out[f"edit_{w}.png"] = edit_frame(round(EDIT_W * p), bh, p)
    for checked in (False, True):
        for state in ("normal", "hover", "focus"):
            out[f"check_{'on' if checked else 'off'}_{state}_{w}.png"] = check_box(round(CHECK_SIZE * p), p, checked,
                                                                                    state)
    _, _, length, lh = separator_geometry(WELCOME_PANEL, WELCOME_SEPARATOR_Y, s, p)
    out[f"line_red_{w}.png"] = separator_line(length, lh, RED)
    out[f"line_green_{w}.png"] = separator_line(length, lh, GREEN_LINE)
    return out


# --- Icon ---------------------------------------------------------------------------------------

def icon_image(size):
    """"DF" on fire. Small sizes get a calm background and heavier letters so that they stay readable."""
    big = size * 4
    small = size <= 32
    if small:
        # Almost black with a band of fire at the bottom: bright letters on a dark ground read at 16 px.
        y = np.linspace(0, 1, big)[:, None] * np.ones((1, big))
        bg = ramp(y, [(0, (24, 6, 4)), (0.62, (52, 10, 6)), (0.85, (150, 38, 8)), (1, (240, 120, 25))])
        bg = Image.fromarray(bg.clip(0, 255).astype(np.uint8))
    else:
        bg = hellscape(big, big, seed=87, embers=size >= 64)
    # The largest font size at which "DF" fits into 84% of the width and 62% of the height.
    m = Image.new("L", (big, big), 0)
    md = ImageDraw.Draw(m)
    fs = big * 0.8
    while True:
        fnt = font(fs)
        bb = md.textbbox((0, 0), "DF", font=fnt)
        tw, th = bb[2] - bb[0], bb[3] - bb[1]
        if (tw <= big * 0.84 and th <= big * 0.62) or fs < 4:
            break
        fs *= 0.97
    ty = (big * (0.44 if small else 0.5)) - th / 2 - bb[1]
    md.text(((big - tw) / 2 - bb[0], ty), "DF", font=fnt, fill=255)
    if not small:
        bg = glow(bg, m, LOGO_GLOW, big * 0.03, 1.1)
    if size >= 24:
        bg.paste(LOGO_STROKE, (0, 0), m.filter(ImageFilter.MaxFilter(int(big * (0.06 if small else 0.05)) | 1)))
    bg.paste((255, 62, 45) if small else LOGO_RED, (0, 0), m)
    d = ImageDraw.Draw(bg)
    d.rounded_rectangle([2, 2, big - 3, big - 3], radius=int(big * 0.12), outline=(40, 10, 6),
                        width=max(4, big // 64))
    mask = Image.new("L", (big, big), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, big - 1, big - 1], radius=int(big * 0.12), fill=255)
    out = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    out.paste(bg, (0, 0), mask)
    return out.resize((size, size), Image.LANCZOS)


def main():
    for kind in ("welcome", "inner"):
        for w, h in SIZES:
            path = HERE / f"back_{kind}_{w}x{h}.png"
            background(kind, w, h).save(path, optimize=True)
            print(f"{path.name}: {path.stat().st_size // 1024} KB")
    ui = HERE / "ui"
    ui.mkdir(exist_ok=True)
    total = 0
    for w, _ in SIZES:
        for name, im in ui_images(w).items():
            im.save(ui / name, optimize=True)
            total += (ui / name).stat().st_size
    print(f"ui/: {len(list(ui.glob('*.png')))} files, {total // 1024} KB")
    icons = [icon_image(sz) for sz in ICON_SIZES]
    ico = HERE / "doomfix.ico"
    icons[-1].save(ico, format="ICO", sizes=[(sz, sz) for sz in ICON_SIZES], append_images=icons[:-1])
    print(f"{ico.name}: {ico.stat().st_size // 1024} KB")


if __name__ == "__main__":
    main()
