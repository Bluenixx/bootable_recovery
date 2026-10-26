#!/usr/bin/env python3
"""Generates the Bluenixx / Material 3 Expressive recovery bitmaps for every density.

Outputs (per res-<density>/images/): logo_image.png, logo_image_switch.png, fastbootd.png,
ic_back.png, ic_back_sel.png (opaque RGB on the MD3 surface color, same sizes as before) and
brand_wordmark.png (8-bit alpha mask of the wordmark used by the install screen).

Requires Pillow and numpy. Poppins is looked up in /usr/share/fonts/truetype/google-fonts or via
the POPPINS_DIR environment variable.
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
FONT_DIR = os.environ.get('POPPINS_DIR', '/usr/share/fonts/truetype/google-fonts')
SS = 4  # supersampling factor

SURFACE = (15, 20, 32)
ON_SURFACE = (226, 229, 242)
SURFACE_HIGH = (32, 40, 64)
PRIMARY_HI, PRIMARY_LO, ON_PRIMARY = (214, 227, 255), (142, 176, 255), (10, 47, 110)
TERT_HI, TERT_LO = (198, 246, 240), (111, 211, 199)
SEC_CONT, ON_SEC_CONT = (59, 52, 112), (230, 222, 255)
TERT_CONT, ON_TERT_CONT = (15, 79, 74), (168, 242, 234)
SECONDARY, TERTIARY = (196, 184, 255), (127, 219, 208)

DENSITIES = {  # name: (scale vs mdpi, logo size, back icon size)
    'mdpi': (1.0, (256, 160), 34),
    'hdpi': (1.5, (384, 240), 52),
    'xhdpi': (2.0, (512, 320), 80),
    'xxhdpi': (3.0, (768, 480), 120),
    'xxxhdpi': (4.0, (1024, 640), 160),
}


def font(name, size):
    return ImageFont.truetype(os.path.join(FONT_DIR, 'Poppins-%s.ttf' % name), int(round(size)))


def cookie_points(cx, cy, r, lobes, amp, rot=0.0, n=1440):
    pts = []
    for i in range(n):
        t = 2 * math.pi * i / n
        rr = r * (1 + amp * math.cos(lobes * t + rot))
        pts.append((cx + rr * math.cos(t), cy + rr * math.sin(t)))
    return pts


def gradient(size, c1, c2, angle_deg=45):
    w, h = size
    a = math.radians(angle_deg)
    ys, xs = np.mgrid[0:h, 0:w].astype('float32')
    d = (xs - w / 2) * math.cos(a) + (ys - h / 2) * math.sin(a)
    ext = abs(w / 2 * math.cos(a)) + abs(h / 2 * math.sin(a))
    t = np.clip(d / (2 * ext) + 0.5, 0, 1)[..., None]
    arr = np.array(c1, 'float32') + (np.array(c2, 'float32') - np.array(c1, 'float32')) * t
    return Image.fromarray(arr.astype('uint8'), 'RGB')


def paste_gradient(img, mask, c1, c2, angle=45):
    img.paste(gradient(img.size, c1, c2, angle), mask=mask)


def spaced_text_width(draw, text, fnt, spacing):
    return sum(draw.textlength(ch, font=fnt) for ch in text) + spacing * (len(text) - 1)


def draw_spaced(draw, x, y, text, fnt, fill, spacing):
    for ch in text:
        draw.text((x, y), ch, font=fnt, fill=fill)
        x += draw.textlength(ch, font=fnt) + spacing


def make_logo(size, f, label, variant):
    """size in px of the final bitmap, f = density scale vs mdpi."""
    W, H = size[0] * SS, size[1] * SS
    u = f * SS  # one mdpi unit in supersampled pixels
    img = Image.new('RGB', (W, H), SURFACE)
    hi, lo = (PRIMARY_HI, PRIMARY_LO) if variant == 'primary' else (TERT_HI, TERT_LO)
    glow = (47, 107, 255) if variant == 'primary' else (60, 190, 175)

    wm = font('Bold', 29 * u)
    tmp = ImageDraw.Draw(img)
    ww = tmp.textlength('bluenixx', font=wm)
    badge_r = 34 * u
    total = 2 * badge_r + 12 * u + ww
    sx = (W - total) / 2
    bcx, bcy = sx + badge_r, 58 * u

    # soft glow behind the badge
    g = Image.new('L', (W, H), 0)
    ImageDraw.Draw(g).ellipse((bcx - 70 * u, bcy - 70 * u, bcx + 70 * u, bcy + 70 * u), fill=90)
    g = g.filter(ImageFilter.GaussianBlur(26 * u))
    # The bitmap is blitted opaque onto a flat surface, so the glow must fade out before the
    # image borders or a visible rectangle appears.
    ys, xs = np.mgrid[0:H, 0:W].astype('float32')
    edge = np.minimum(np.minimum(xs, W - 1 - xs), np.minimum(ys, H - 1 - ys)) / (34 * u)
    fade = np.clip(edge, 0, 1) ** 2
    g = Image.fromarray((np.array(g, 'float32') * fade).astype('uint8'))
    img.paste(Image.new('RGB', (W, H), glow), mask=g)

    # drop shadow + cookie badge
    m = Image.new('L', (W, H), 0)
    ImageDraw.Draw(m).polygon(cookie_points(bcx, bcy, badge_r, 9, 0.06, 0.2), fill=255)
    sh = m.filter(ImageFilter.GaussianBlur(5 * u)).point(lambda v: v * 55 // 100)
    img.paste(Image.new('RGB', (W, H), (4, 8, 20)), mask=sh)
    paste_gradient(img, m, hi, lo, 50)

    # inner tonal ring for depth
    ring = Image.new('L', (W, H), 0)
    ImageDraw.Draw(ring).polygon(cookie_points(bcx, bcy, badge_r * 0.80, 9, 0.055, 0.2), fill=255)
    ring_inner = Image.new('L', (W, H), 0)
    ImageDraw.Draw(ring_inner).polygon(cookie_points(bcx, bcy, badge_r * 0.74, 9, 0.05, 0.2), fill=255)
    ring = Image.fromarray(np.clip(np.array(ring, 'int16') - np.array(ring_inner, 'int16'), 0, 255).astype('uint8'))
    img.paste(Image.new('RGB', (W, H), (255, 255, 255)), mask=ring.point(lambda v: v * 38 // 100))

    # the "b" glyph
    gf = font('Bold', 56 * u)
    d = ImageDraw.Draw(img)
    bb = d.textbbox((0, 0), 'b', font=gf)
    d.text((bcx - (bb[0] + bb[2]) / 2, bcy - (bb[1] + bb[3]) / 2 - 1 * u), 'b', font=gf, fill=ON_PRIMARY if variant == 'primary' else (0, 55, 51))

    # accents (tertiary cookie + secondary dot)
    acc = Image.new('L', (W, H), 0)
    ImageDraw.Draw(acc).polygon(cookie_points(bcx + badge_r * 0.98, bcy - badge_r * 0.82, 9 * u, 7, 0.12, 0.4), fill=255)
    paste_gradient(img, acc, (200, 190, 255), SECONDARY, 45) if variant != 'primary' else paste_gradient(img, acc, TERT_HI, TERTIARY, 45)
    dot = Image.new('L', (W, H), 0)
    ImageDraw.Draw(dot).ellipse((bcx - badge_r * 0.98 - 4.5 * u, bcy + badge_r * 0.86 - 4.5 * u,
                                 bcx - badge_r * 0.98 + 4.5 * u, bcy + badge_r * 0.86 + 4.5 * u), fill=255)
    img.paste(Image.new('RGB', (W, H), SECONDARY if variant == 'primary' else PRIMARY_LO), mask=dot)

    # wordmark
    d = ImageDraw.Draw(img)
    wb = d.textbbox((0, 0), 'bluenixx', font=wm)
    wx = sx + 2 * badge_r + 12 * u - wb[0]
    wy = bcy - (wb[1] + wb[3]) / 2
    d.text((wx, wy), 'bluenixx', font=wm, fill=ON_SURFACE)

    # label chip
    cf = font('Medium', 11.5 * u)
    spacing = 2.4 * u
    tw = spaced_text_width(d, label, cf, spacing)
    chip_w, chip_h = tw + 30 * u, 25 * u
    ccx, ccy = W / 2, 124 * u
    fill, txt = (SEC_CONT, ON_SEC_CONT) if variant == 'primary' else (TERT_CONT, ON_TERT_CONT)
    d.rounded_rectangle((ccx - chip_w / 2, ccy - chip_h / 2, ccx + chip_w / 2, ccy + chip_h / 2), radius=chip_h / 2, fill=fill)
    cb = d.textbbox((0, 0), 'E', font=cf)
    draw_spaced(d, ccx - tw / 2, ccy - (cb[1] + cb[3]) / 2, label, cf, txt, spacing)

    return img.resize(size, Image.LANCZOS)


def make_back_icon(size, selected):
    S = size * SS
    img = Image.new('RGB', (S, S), SURFACE)
    d = ImageDraw.Draw(img)
    r = S * 0.46
    c = S / 2
    d.ellipse((c - r, c - r, c + r, c + r), fill=PRIMARY_LO if selected else SURFACE_HIGH)
    col = ON_PRIMARY if selected else ON_SURFACE
    w = max(2, int(S * 0.075))
    ax0, ax1 = c - S * 0.20, c + S * 0.21
    d.line((ax0, c, ax1, c), fill=col, width=w)
    hd = S * 0.15
    d.line((ax0, c, ax0 + hd, c - hd), fill=col, width=w)
    d.line((ax0, c, ax0 + hd, c + hd), fill=col, width=w)
    for px, py in ((ax0, c), (ax1, c)):
        d.ellipse((px - w / 2, py - w / 2, px + w / 2, py + w / 2), fill=col)
    for ex, ey in ((ax0 + hd, c - hd), (ax0 + hd, c + hd)):
        d.ellipse((ex - w / 2, ey - w / 2, ex + w / 2, ey + w / 2), fill=col)
    return img.resize((size, size), Image.LANCZOS)


def make_wordmark(f):
    fnt = font('Bold', 24 * f * SS)
    probe = ImageDraw.Draw(Image.new('L', (4, 4)))
    bb = probe.textbbox((0, 0), 'bluenixx', font=fnt)
    pad = 2 * SS
    w, h = bb[2] - bb[0] + 2 * pad, bb[3] - bb[1] + 2 * pad
    img = Image.new('L', (w, h), 0)
    ImageDraw.Draw(img).text((pad - bb[0], pad - bb[1]), 'bluenixx', font=fnt, fill=255)
    return img.resize((max(1, w // SS), max(1, h // SS)), Image.LANCZOS)


def main():
    for name, (f, logo_size, back_size) in DENSITIES.items():
        out = os.path.join(ROOT, 'res-%s' % name, 'images')
        os.makedirs(out, exist_ok=True)
        logo = make_logo(logo_size, f, 'RECOVERY', 'primary')
        logo.save(os.path.join(out, 'logo_image.png'), optimize=True)
        logo.save(os.path.join(out, 'logo_image_switch.png'), optimize=True)
        make_logo(logo_size, f, 'FASTBOOTD', 'tertiary').save(os.path.join(out, 'fastbootd.png'), optimize=True)
        make_back_icon(back_size, False).save(os.path.join(out, 'ic_back.png'), optimize=True)
        make_back_icon(back_size, True).save(os.path.join(out, 'ic_back_sel.png'), optimize=True)
        make_wordmark(f).save(os.path.join(out, 'brand_wordmark.png'), optimize=True)
        print('generated', name)


if __name__ == '__main__':
    sys.exit(main())
