#!/usr/bin/env python3
"""Build the built-in controls guide EPUB from its Markdown source."""

from datetime import date
from html import escape
from pathlib import Path
import uuid
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile, ZipInfo


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs" / "controls-guide.md"
OUTPUT = ROOT / "data" / "controlsGuide.epub"


def guide_xhtml() -> str:
    body = []
    for line in SOURCE.read_text(encoding="utf-8").splitlines():
        if line.startswith("# "):
            body.append(f"<h1>{escape(line[2:])}</h1>")
        elif line.startswith("## "):
            body.append(f"<h2>{escape(line[3:])}</h2>")
        elif line.startswith("- "):
            body.append(f"<p>• {escape(line[2:])}</p>")
        elif line.strip():
            text = escape(line.strip())
            text = text.replace("**", "")
            body.append(f"<p>{text}</p>")
    return """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN"
  "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
<head><title>Modern Reader controls guide</title></head>
<body>
""" + "\n".join(body) + "\n</body>\n</html>\n"


def write_entry(epub: ZipFile, name: str, content: str, compression=ZIP_DEFLATED) -> None:
    entry = ZipInfo(name, date_time=(2026, 10, 7, 0, 0, 0))
    entry.compress_type = compression
    entry.external_attr = 0o100644 << 16
    epub.writestr(entry, content.encode("utf-8"))


def main() -> None:
    identifier = uuid.uuid5(uuid.NAMESPACE_URL, "modern-reader-controls-guide")
    modified = date.today().isoformat()
    opf = f'''<?xml version="1.0" encoding="utf-8"?>
<package version="2.0" unique-identifier="BookId" xmlns="http://www.idpf.org/2007/opf">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:opf="http://www.idpf.org/2007/opf">
    <dc:identifier opf:scheme="UUID" id="BookId">urn:uuid:{identifier}</dc:identifier>
    <dc:language>en</dc:language>
    <dc:title>Modern Reader controls guide</dc:title>
    <dc:creator>Modern Reader</dc:creator>
    <dc:date opf:event="modification">{modified}</dc:date>
  </metadata>
  <manifest>
    <item id="guide" href="Text/controls-guide.xhtml" media-type="application/xhtml+xml"/>
    <item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>
  </manifest>
  <spine toc="ncx"><itemref idref="guide"/></spine>
</package>
'''
    ncx = f'''<?xml version="1.0" encoding="utf-8"?>
<ncx version="2005-1" xmlns="http://www.daisy.org/z3986/2005/ncx/">
  <head><meta name="dtb:uid" content="urn:uuid:{identifier}"/></head>
  <docTitle><text>Modern Reader controls guide</text></docTitle>
  <navMap>
    <navPoint id="guide" playOrder="1">
      <navLabel><text>Controls guide</text></navLabel>
      <content src="Text/controls-guide.xhtml"/>
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
        write_entry(epub, "OEBPS/Text/controls-guide.xhtml", guide_xhtml())
    print(f"Built {OUTPUT}")


if __name__ == "__main__":
    main()
