"""Inline and gzip the editable Swiftwing Master UI into its Arduino sketch."""

from __future__ import annotations

import gzip
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parent
UI_DIR = ROOT / "ui"
SKETCH = ROOT / "Swiftwing_Master.ino"
GZIP_PAGES = {
    "WIFI_SETUP_PAGE": "wifi_setup",
    "MAIN_PAGE": "main",
    "STATUS_PAGE": "status",
    "LOGIN_PAGE": "login",
}


def build_page(name: str) -> str:
    html = (UI_DIR / f"{name}.html").read_text(encoding="utf-8-sig").strip()
    css = (UI_DIR / f"{name}.css").read_text(encoding="utf-8-sig").strip()
    html, count = re.subn(
        r"<style>.*?</style>",
        lambda _match: f"<style>\n{css}\n</style>",
        html,
        count=1,
        flags=re.IGNORECASE | re.DOTALL,
    )
    if count != 1:
        raise ValueError(f"{name}.html must contain exactly one inline style block")
    return html + "\n"


def compressed_array(name: str, data: bytes) -> str:
    lines = []
    for start in range(0, len(data), 20):
        block = data[start : start + 20]
        lines.append("  " + ", ".join(f"0x{byte:02X}" for byte in block) + ",")
    return f"static const uint8_t {name}[] PROGMEM = {{\n" + "\n".join(lines) + "\n};"


def replace_once(source: str, pattern: str, replacement: str, label: str) -> str:
    result, count = re.subn(pattern, lambda _match: replacement, source, count=1, flags=re.DOTALL)
    if count != 1:
        raise ValueError(f"could not find one {label} declaration in {SKETCH.name}")
    return result


def main() -> None:
    sketch = SKETCH.read_text(encoding="utf-8-sig")

    for declaration, source_name in GZIP_PAGES.items():
        page = build_page(source_name)
        payload = gzip.compress(page.encode("utf-8"), compresslevel=9, mtime=0)
        pattern = rf"static const uint8_t {declaration}\[\] PROGMEM = \{{.*?\n\}};"
        sketch = replace_once(
            sketch,
            pattern,
            compressed_array(declaration, payload),
            declaration,
        )

    SKETCH.write_text(sketch, encoding="utf-8", newline="\n")
    print("Embedded Swiftwing UI pages:")
    for source_name in GZIP_PAGES.values():
        page = build_page(source_name)
        packed = gzip.compress(page.encode("utf-8"), compresslevel=9, mtime=0)
        print(f"  {source_name}: {len(page.encode('utf-8')):,} B HTML -> {len(packed):,} B gzip")


if __name__ == "__main__":
    main()
