#!/usr/bin/env python3
"""Build the on-device firmware-info EPUB from brand.h and the full changelog."""

from datetime import date
from html import escape
from pathlib import Path
import re
import uuid
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile, ZipInfo


ROOT = Path(__file__).resolve().parents[1]
CHANGELOG = ROOT / "CHANGELOG.md"
BRAND_HEADER = ROOT / "include" / "brand.h"
OUTPUT = ROOT / "data" / "firmwareVersion.epub"


def current_version() -> str:
    source = BRAND_HEADER.read_text(encoding="utf-8")
    match = re.search(r'^\s*#define\s+MR_VERSION\s+"([^"]+)"', source, re.MULTILINE)
    if not match:
        raise RuntimeError(f"Could not find MR_VERSION in {BRAND_HEADER}")
    return match.group(1)


def inline_markup(value: str) -> str:
    """Convert the small inline-Markdown subset used by CHANGELOG.md to XHTML."""
    value = escape(value, quote=False)
    value = re.sub(r"`([^`]+)`", r"<code>\1</code>", value)
    value = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", value)
    value = re.sub(r"(?<!\*)\*([^*]+)\*(?!\*)", r"<em>\1</em>", value)
    return value


def changelog_xhtml() -> str:
    lines = CHANGELOG.read_text(encoding="utf-8").splitlines()
    first_release = next((i for i, line in enumerate(lines) if line.startswith("## ")), None)
    if first_release is None:
        raise RuntimeError(f"No version sections found in {CHANGELOG}")

    body = []
    for line in lines[first_release:]:
        if line.startswith("### "):
            body.append(f"<h2>{inline_markup(line[4:])}</h2>")
        elif line.startswith("## "):
            body.append(f"<h1>{inline_markup(line[3:])}</h1>")
        elif line.startswith("- "):
            body.append(f"<p>• {inline_markup(line[2:])}</p>")
        elif line.strip():
            body.append(f"<p>{inline_markup(line.strip())}</p>")

    version = current_version()
    intro = [
        "<h1>Modern Reader</h1>",
        f"<p><strong>Version {escape(version)}</strong></p>",
        "<p>A community firmware fork for the Diptyx E-reader, based on Diptyx firmware 1.0.2.</p>",
        "<p>This is an independent project. It is not affiliated with, endorsed by, or supported by Diptyx.</p>",
        "<h2>Credits and license</h2>",
        "<p>The original Diptyx E-reader hardware and firmware were created by Martijn den Hoed and the Diptyx team.</p>",
        "<p>Original project: https://github.com/MartijndenHoed/Diptyx</p>",
        "<p>Original source code is MIT-licensed. Third-party components keep their own licenses.</p>",
        "<h1>Complete change log</h1>",
        "<p>Every Modern Reader addition compared with the original firmware. Newest release first; earlier releases remain listed below.</p>",
    ]
    return """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN"
  "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
<head><title>Modern Reader firmware info</title></head>
<body>
""" + "\n".join(intro + body) + "\n</body>\n</html>\n"


def write_entry(epub: ZipFile, name: str, content: str, compression=ZIP_DEFLATED) -> None:
    entry = ZipInfo(name, date_time=(2026, 10, 7, 0, 0, 0))
    entry.compress_type = compression
    entry.external_attr = 0o100644 << 16
    epub.writestr(entry, content.encode("utf-8"))


def main() -> None:
    version = current_version()
    identifier = uuid.uuid5(uuid.NAMESPACE_URL, f"modern-reader-firmware-info-{version}")
    modified = date.today().isoformat()
    opf = f'''<?xml version="1.0" encoding="utf-8"?>
<package version="2.0" unique-identifier="BookId" xmlns="http://www.idpf.org/2007/opf">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:opf="http://www.idpf.org/2007/opf">
    <dc:identifier opf:scheme="UUID" id="BookId">urn:uuid:{identifier}</dc:identifier>
    <dc:language>en</dc:language>
    <dc:title>Modern Reader firmware info</dc:title>
    <dc:creator opf:role="aut">Modern Reader, a fork of the Diptyx firmware by Martijn den Hoed</dc:creator>
    <dc:date opf:event="modification">{modified}</dc:date>
  </metadata>
  <manifest>
    <item id="firmware-info" href="Text/firmware-info.xhtml" media-type="application/xhtml+xml"/>
    <item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>
  </manifest>
  <spine toc="ncx"><itemref idref="firmware-info"/></spine>
</package>
'''
    ncx = f'''<?xml version="1.0" encoding="utf-8"?>
<ncx version="2005-1" xmlns="http://www.daisy.org/z3986/2005/ncx/">
  <head><meta name="dtb:uid" content="urn:uuid:{identifier}"/></head>
  <docTitle><text>Modern Reader firmware info</text></docTitle>
  <navMap>
    <navPoint id="firmware-info" playOrder="1">
      <navLabel><text>Firmware info and change log</text></navLabel>
      <content src="Text/firmware-info.xhtml"/>
    </navPoint>
  </navMap>
</ncx>
'''
    container = '''<?xml version="1.0" encoding="utf-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>
'''

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(OUTPUT, "w") as epub:
        write_entry(epub, "mimetype", "application/epub+zip", ZIP_STORED)
        write_entry(epub, "META-INF/container.xml", container)
        write_entry(epub, "OEBPS/content.opf", opf)
        write_entry(epub, "OEBPS/toc.ncx", ncx)
        write_entry(epub, "OEBPS/Text/firmware-info.xhtml", changelog_xhtml())
    print(f"Built {OUTPUT} for Modern Reader {version}")


if __name__ == "__main__":
    main()
