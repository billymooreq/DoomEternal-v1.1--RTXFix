"""Generates the installer artwork: wizard backgrounds and the "DF" icon.

Everything is drawn procedurally (Pillow + numpy) from fixed random seeds, so every run produces the same
images; no third-party artwork is used. The only external asset is the Inter Display Black font (SIL OFL 1.1,
see fonts/OFL.txt), which is baked into the images and is not shipped in the installer.

Run it through scripts\\build_assets.ps1, which sets up the Python environment. Output, next to this file:
  back_welcome_<w>x<h>.png  welcome and finish pages: fire, large DOOMFIX logo and RTX tag, the content panel
  back_inner_<w>x<h>.png    other pages: fire, small DOOMFIX logo and RTX tag at the top left, the content panel
  doomfix.ico               the icon of setup.exe and doomfix_launcher.exe, 16 to 256 px

The background sizes are the wizard image areas of Inno Setup 6.7 at 100, 125, 150, 175 and 200% DPI (see the
WizardBackImageFile help topic); Setup picks the best match. The layout uses the coordinates of the 497x360
design (installer/doomfix.iss places the panels and controls with the same numbers).
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


# Layout shared with the "Look" part of installer/doomfix.iss (design units). The panels are part of the image:
# the custom style draws transparent controls over the window background, so a panel made of a control would not
# show behind every control.
PANEL = (34, 30, 30)
PANEL_BORDER = (140, 70, 50)
SEPARATOR = (150, 130, 125)
WELCOME_PANEL = (40, 104, 457, 306)   # welcome and finish pages
WELCOME_SEPARATOR_Y = 144
INNER_PANEL = (18, 50, 479, 316)      # all other pages
INNER_SEPARATOR_Y = 102
PANEL_PADDING = 16


def panel(img, box, sep_y, s):
    d = ImageDraw.Draw(img)
    lw = max(1, round(s))
    x0, y0, x1, y1 = (round(v * s) for v in box)
    d.rectangle([x0, y0, x1 - 1, y1 - 1], fill=PANEL, outline=PANEL_BORDER, width=lw)
    sy = round(sep_y * s)
    d.line([x0 + round(PANEL_PADDING * s), sy, x1 - round(PANEL_PADDING * s), sy], fill=SEPARATOR, width=lw)
    return img


def background(kind, w, h):
    s = w / BASE_W
    if kind == "welcome":
        img = hellscape(w, h, seed=7)
        img = logo(img, "DOOMFIX", BASE_W / 2, 14, 34, s, center=True)
        img = tag(img, BASE_W / 2, 60, 11, s)
        img = panel(img, WELCOME_PANEL, WELCOME_SEPARATOR_Y, s)
    else:
        img = hellscape(w, h, seed=19)
        img = logo(img, "DOOMFIX", 18, 10, 16, s, stroke=5)
        img = tag(img, 136, 11, 9, s)
        img = panel(img, INNER_PANEL, INNER_SEPARATOR_Y, s)
    return img


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
    icons = [icon_image(sz) for sz in ICON_SIZES]
    ico = HERE / "doomfix.ico"
    icons[-1].save(ico, format="ICO", sizes=[(sz, sz) for sz in ICON_SIZES], append_images=icons[:-1])
    print(f"{ico.name}: {ico.stat().st_size // 1024} KB")


if __name__ == "__main__":
    main()
