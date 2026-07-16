#!/usr/bin/env python3
"""Generate original public-release Desktop Pet Maker brand assets."""

from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
RESOURCE_DIR = ROOT / "resources"
BRANDING_DIR = RESOURCE_DIR / "branding"

APP_ICON_SVG = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256">
  <rect x="18" y="18" width="220" height="220" rx="48" fill="#202327"/>
  <rect x="30" y="30" width="196" height="196" rx="39" fill="#FFD43B"/>
  <path d="M76 86 112 44l12 39 39-25-16 43 42 2-36 29 25 35-45-10-18 42-18-42-45 10 25-35-36-29 42-2-16-43 39 25z" fill="#FFFDF5"/>
  <rect x="59" y="88" width="138" height="105" rx="50" fill="#202327"/>
  <circle cx="105" cy="137" r="12" fill="#35C6D0"/>
  <circle cx="151" cy="137" r="12" fill="#35C6D0"/>
  <path d="M106 160c14 12 30 12 44 0" fill="none" stroke="#FFFDF5" stroke-width="7" stroke-linecap="round"/>
</svg>
"""
TRAY_ICON_SVG = APP_ICON_SVG
BRAND_MARK_SVG = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 760 180">
  <rect width="760" height="180" rx="28" fill="#FFFFFF"/>
  <g transform="translate(14 14) scale(.594)">
    <rect x="18" y="18" width="220" height="220" rx="48" fill="#202327"/>
    <rect x="30" y="30" width="196" height="196" rx="39" fill="#FFD43B"/>
    <path d="M76 86 112 44l12 39 39-25-16 43 42 2-36 29 25 35-45-10-18 42-18-42-45 10 25-35-36-29 42-2-16-43 39 25z" fill="#FFFDF5"/>
    <rect x="59" y="88" width="138" height="105" rx="50" fill="#202327"/>
    <circle cx="105" cy="137" r="12" fill="#35C6D0"/>
    <circle cx="151" cy="137" r="12" fill="#35C6D0"/>
    <path d="M106 160c14 12 30 12 44 0" fill="none" stroke="#FFFDF5" stroke-width="7" stroke-linecap="round"/>
  </g>
  <text x="190" y="82" font-family="Segoe UI, Microsoft YaHei UI, sans-serif" font-size="46" font-weight="700" fill="#202327">Desktop Pet Maker</text>
  <text x="192" y="128" font-family="Microsoft YaHei UI, Noto Sans CJK SC, Segoe UI, sans-serif" font-size="26" fill="#687078">桌宠制作 · AI 互动创作平台</text>
</svg>
"""

def create_icon(size=1024):
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    s = size / 256
    box = lambda v: tuple(round(x * s) for x in v)
    draw.rounded_rectangle(box((18, 18, 238, 238)), radius=round(48*s), fill="#202327")
    draw.rounded_rectangle(box((30, 30, 226, 226)), radius=round(39*s), fill="#FFD43B")
    points = [(76,86),(112,44),(124,83),(163,58),(147,101),(189,103),(153,132),(178,167),(133,157),(115,199),(97,157),(52,167),(77,132),(41,103),(83,101),(67,58),(106,83)]
    draw.polygon([(round(x*s), round(y*s)) for x,y in points], fill="#FFFDF5")
    draw.rounded_rectangle(box((59,88,197,193)), radius=round(50*s), fill="#202327")
    draw.ellipse(box((93,125,117,149)), fill="#35C6D0")
    draw.ellipse(box((139,125,163,149)), fill="#35C6D0")
    draw.arc(box((103,142,153,178)), start=18, end=162, fill="#FFFDF5", width=max(1, round(7*s)))
    return image

def main():
    BRANDING_DIR.mkdir(parents=True, exist_ok=True)
    RESOURCE_DIR.mkdir(parents=True, exist_ok=True)
    master = create_icon()
    (BRANDING_DIR / "app_icon.svg").write_text(APP_ICON_SVG, encoding="utf-8", newline="\n")
    (BRANDING_DIR / "tray_icon.svg").write_text(TRAY_ICON_SVG, encoding="utf-8", newline="\n")
    (BRANDING_DIR / "brand_mark.svg").write_text(BRAND_MARK_SVG, encoding="utf-8", newline="\n")
    for size in (16,32,48,64,128,256):
        master.resize((size,size), Image.Resampling.LANCZOS).save(BRANDING_DIR / f"app_icon_{size}.png", optimize=True)
    icon = master.resize((256,256), Image.Resampling.LANCZOS)
    icon.save(BRANDING_DIR / "app_icon.png", optimize=True)
    icon.save(RESOURCE_DIR / "app_icon.png", optimize=True)
    master.save(BRANDING_DIR / "app_icon.ico", format="ICO", sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
    master.save(RESOURCE_DIR / "app_icon.ico", format="ICO", sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
    print("PASS generated original public-release brand assets")

if __name__ == "__main__":
    main()
