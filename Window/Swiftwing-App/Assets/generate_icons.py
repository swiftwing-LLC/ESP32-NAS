"""Generate Swiftwing desktop and Apple app icons from the V1.9 login motif.

Requires Pillow. The source emblem in Nas_V1.9.zip is ui/login.html:
two orbit circles, a central core, satellite, and north-east arrow.
"""

from __future__ import annotations

import io
import json
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parent
RESAMPLE = Image.Resampling.LANCZOS


SVG_TEMPLATE = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024" role="img" aria-label="Swiftwing app icon">
  <defs>
    <linearGradient id="night" x1="0" y1="0" x2="1" y2="1">
      <stop stop-color="#152a4c"/><stop offset=".48" stop-color="#0b1830"/><stop offset="1" stop-color="#060b17"/>
    </linearGradient>
    <radialGradient id="core" cx="38%" cy="32%" r="74%">
      <stop stop-color="#c0d2f6" stop-opacity=".36"/><stop offset=".5" stop-color="#526c9e" stop-opacity=".28"/>
      <stop offset="1" stop-color="#0b162b" stop-opacity=".2"/>
    </radialGradient>
    <filter id="glow"><feGaussianBlur stdDeviation="11"/></filter>
  </defs>
  <rect width="1024" height="1024" rx="CORNER" fill="url(#night)"/>
  <circle cx="512" cy="512" r="350" fill="none" stroke="#a6bdef" stroke-opacity=".58" stroke-width="8"/>
  <circle cx="512" cy="512" r="286" fill="none" stroke="#9ab0dc" stroke-opacity=".37" stroke-width="7" stroke-dasharray="1 37" stroke-linecap="round"/>
  <circle cx="512" cy="512" r="215" fill="url(#core)" stroke="#b8c9ed" stroke-opacity=".45" stroke-width="6"/>
  <path d="M 580 169 A 350 350 0 0 1 824 353" fill="none" stroke="#c2d5fa" stroke-opacity=".92" stroke-width="11" stroke-linecap="round"/>
  <circle cx="512" cy="162" r="13" fill="#d8e5ff" stroke="#233b67" stroke-width="5"/>
  <circle cx="763" cy="267" r="8" fill="#a9bae6"/>
  <path d="M410 614 614 410 M464 410h150v150" fill="none" stroke="#8caeea" stroke-opacity=".54" stroke-width="44" stroke-linejoin="round" filter="url(#glow)"/>
  <path d="M410 614 614 410 M464 410h150v150" fill="none" stroke="#edf4ff" stroke-width="STROKE" stroke-linecap="square" stroke-linejoin="miter"/>
</svg>
'''


def write_svg() -> None:
    (ROOT / "swiftwing-icon.svg").write_text(
        SVG_TEMPLATE.replace("CORNER", "180").replace("STROKE", "34"), encoding="utf-8"
    )
    (ROOT / "swiftwing-icon-ios.svg").write_text(
        SVG_TEMPLATE.replace("CORNER", "0").replace("STROKE", "34"), encoding="utf-8"
    )


def render_icon(size: int, *, desktop: bool, compact: bool = False) -> Image.Image:
    # Paint large, then downsample for smooth contours at every supplied size.
    side = 2048 if size >= 256 else 1024
    scale = side / 1024
    tile = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    pixels = tile.load()
    for y in range(side):
        t = y / max(side - 1, 1)
        for x in range(side):
            diagonal = (x / max(side - 1, 1) + t) / 2
            c = (
                round(21 * (1 - diagonal) + 6 * diagonal),
                round(42 * (1 - diagonal) + 11 * diagonal),
                round(76 * (1 - diagonal) + 23 * diagonal),
                255,
            )
            pixels[x, y] = c

    if desktop:
        mask = Image.new("L", (side, side), 0)
        md = ImageDraw.Draw(mask)
        md.rounded_rectangle((0, 0, side - 1, side - 1), radius=180 * scale, fill=255)
        tile.putalpha(mask)

    marks = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    d = ImageDraw.Draw(marks)
    def box(radius: float) -> tuple[int, int, int, int]:
        r = radius * scale
        c = side / 2
        return (round(c - r), round(c - r), round(c + r), round(c + r))

    orbit_width = round((15 if compact else 8) * scale)
    d.ellipse(box(350), outline=(166, 189, 239, 180), width=orbit_width)

    if not compact:
        for n in range(48):
            angle = 2 * math.pi * n / 48
            cx = (512 + 286 * math.cos(angle)) * scale
            cy = (512 + 286 * math.sin(angle)) * scale
            dot = 3.1 * scale
            d.ellipse((cx-dot, cy-dot, cx+dot, cy+dot), fill=(154, 176, 220, 108))

    # Soft core and a clear circular edge.
    core_glow = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    gd = ImageDraw.Draw(core_glow)
    gd.ellipse(box(221), fill=(103, 143, 218, 70))
    core_glow = core_glow.filter(ImageFilter.GaussianBlur(31 * scale))
    marks = Image.alpha_composite(marks, core_glow)
    d = ImageDraw.Draw(marks)
    d.ellipse(box(215), fill=(31, 54, 92, 205), outline=(184, 201, 237, 130), width=round(6 * scale))
    for radius in range(212, 20, -3):
        strength = int(11 * (1 - radius / 215))
        d.ellipse(box(radius), fill=(128, 160, 219, strength))

    # A partial sweep, satellite, and distant node preserve the web emblem.
    d.arc(box(350), start=281, end=332, fill=(194, 213, 250, 238), width=round(11 * scale))
    d.ellipse(((512-13)*scale, (162-13)*scale, (512+13)*scale, (162+13)*scale),
              fill=(216, 229, 255, 255), outline=(35, 59, 103, 255), width=round(5*scale))
    if not compact:
        d.ellipse(((763-8)*scale, (267-8)*scale, (763+8)*scale, (267+8)*scale),
                  fill=(169, 186, 230, 255))

    arrow_points = [((410, 614), (614, 410)), ((464, 410), (614, 410), (614, 560))]
    arrow_glow = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    ag = ImageDraw.Draw(arrow_glow)
    for pts in arrow_points:
        ag.line([(round(x*scale), round(y*scale)) for x, y in pts],
                fill=(140, 174, 234, 160), width=round((85 if compact else 64)*scale), joint="curve")
    marks = Image.alpha_composite(marks, arrow_glow.filter(ImageFilter.GaussianBlur(13*scale)))
    d = ImageDraw.Draw(marks)
    for pts in arrow_points:
        d.line([(round(x*scale), round(y*scale)) for x, y in pts],
               fill=(237, 244, 255, 255), width=round((58 if compact else 34)*scale), joint="curve")

    tile = Image.alpha_composite(tile, marks)
    if desktop:
        # Reapply the tile mask so no glow leaks into transparent corners.
        tile.putalpha(mask)
    return tile.resize((size, size), RESAMPLE)


def png_bytes(image: Image.Image) -> bytes:
    buffer = io.BytesIO()
    image.save(buffer, format="PNG", optimize=True)
    return buffer.getvalue()


def write_ico(path: Path, images: list[Image.Image]) -> None:
    # Each ICO entry embeds its own tailored PNG. Pillow's save path would
    # derive small entries from one large image and lose the compact variant.
    payloads = [png_bytes(im) for im in images]
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries = []
    for image, payload in zip(images, payloads):
        size = image.width
        entries.append(struct.pack("<BBBBHHII", 0 if size == 256 else size,
                                   0 if size == 256 else size, 0, 0, 1, 32,
                                   len(payload), offset))
        offset += len(payload)
    path.write_bytes(header + b"".join(entries) + b"".join(payloads))


def write_catalogs(mac_images: dict[int, Image.Image], ios_image: Image.Image) -> None:
    ios = ROOT / "ios" / "AppIcon.appiconset"
    ios.mkdir(parents=True, exist_ok=True)
    ios_image.save(ios / "AppIcon.png", optimize=True)
    (ios / "Contents.json").write_text(json.dumps({
        "images": [{"filename": "AppIcon.png", "idiom": "universal",
                    "platform": "ios", "size": "1024x1024"}],
        "info": {"author": "xcode", "version": 1}
    }, indent=2) + "\n", encoding="utf-8")

    mac = ROOT / "macos" / "AppIcon.appiconset"
    mac.mkdir(parents=True, exist_ok=True)
    for size, image in mac_images.items():
        image.save(mac / f"icon_{size}.png", optimize=True)
    images = []
    for logical in (16, 32, 128, 256, 512):
        for scale in (1, 2):
            images.append({"filename": f"icon_{logical * scale}.png",
                           "idiom": "mac", "scale": f"{scale}x",
                           "size": f"{logical}x{logical}"})
    (mac / "Contents.json").write_text(json.dumps({
        "images": images, "info": {"author": "xcode", "version": 1}
    }, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    ROOT.mkdir(parents=True, exist_ok=True)
    write_svg()
    ios_image = render_icon(1024, desktop=False)
    ios_image.save(ROOT / "swiftwing-icon.png", optimize=True)
    sizes = (16, 24, 32, 48, 64, 128, 256, 512, 1024)
    mac_images = {size: render_icon(size, desktop=True, compact=size <= 32) for size in sizes}
    mac_images[1024].save(ROOT / "swiftwing-icon-macos.png", optimize=True)
    write_ico(ROOT / "swiftwing.ico", [mac_images[size] for size in (16, 24, 32, 48, 64, 128, 256)])
    mac_images[1024].save(ROOT / "swiftwing.icns", format="ICNS")
    write_catalogs(mac_images, ios_image)


if __name__ == "__main__":
    main()
