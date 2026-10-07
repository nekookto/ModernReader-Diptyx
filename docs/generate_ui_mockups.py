"""Generate source-derived Modern Reader UI mockups for the README.

These illustrations are not screenshots from an e-reader. They are lightweight,
reproducible previews based on the menu layout and labels in firmware_source.
Requires Pillow: python -m pip install Pillow
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "images"
PAGE_W, PAGE_H = 480, 648
INK = "#171717"
PAPER = "#fcfcf7"
MID = "#85857d"
LIGHT = "#ddddda"

FONT_REGULAR = "/usr/share/fonts/TTF/DejaVuSans.ttf"
FONT_BOLD = "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf"


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    path = FONT_BOLD if bold else FONT_REGULAR
    try:
        return ImageFont.truetype(path, size)
    except OSError:
        return ImageFont.load_default()


def text(draw: ImageDraw.ImageDraw, xy, value: str, size=16, bold=False, fill=INK):
    draw.text(xy, value, font=font(size, bold), fill=fill)


def fitted(draw, value: str, max_width: int, size=16, bold=False) -> str:
    f = font(size, bold)
    if draw.textbbox((0, 0), value, font=f)[2] <= max_width:
        return value
    suffix = "…"
    while value and draw.textbbox((0, 0), value + suffix, font=f)[2] > max_width:
        value = value[:-1]
    return value.rstrip() + suffix


def dither(draw, box, step=5, shade="#c8c8c3"):
    x0, y0, x1, y1 = box
    for y in range(y0, y1, step):
        for x in range(x0 + (step if (y // step) % 2 else 0), x1, step):
            draw.rectangle((x, y, x + 1, y + 1), fill=shade)


def header(draw, title="", crumb="", main=False):
    if main:
        text(draw, (22, 17), "modern reader", 27, True)
        text(draw, (395, 25), "v1.0.1", 14, False)
        dither(draw, (20, 59, 452, 68), 5)
        draw.rectangle((20, 70, 452, 72), fill=INK)
    else:
        text(draw, (24, 17), crumb.upper(), 13, False)
        text(draw, (24, 39), fitted(draw, title, 400, 28, True), 28, True)
        draw.rectangle((20, 72, 452, 74), fill=INK)
        draw.rectangle((20, 70, 92, 77), fill=INK)


def page_card(draw, y, title, desc, tile, selected=False, value=None):
    x, w, h = 20, 432, 60
    fill = INK if selected else PAPER
    ink = PAPER if selected else INK
    if not selected:
        dither(draw, (x + 4, y + 4, x + w, y + h + 4), 5, "#deded9")
    draw.rounded_rectangle((x, y, x + w, y + h), radius=9, fill=fill, outline=INK, width=2)
    tile_fill = PAPER if selected else INK
    tile_ink = INK if selected else PAPER
    draw.rounded_rectangle((x + 10, y + 8, x + 54, y + 52), radius=8, fill=tile_fill)
    text(draw, (x + 23, y + 16), tile, 21, True, tile_ink)
    title_max = 230 if value else 310
    text(draw, (x + 66, y + 10), fitted(draw, title, title_max, 16, True), 16, True, ink)
    text(draw, (x + 66, y + 34), fitted(draw, desc, title_max, 13), 13, False, ink)
    if value:
        vb = draw.textbbox((0, 0), value, font=font(13, True))
        vw = vb[2] + 18
        vx = x + w - vw - 14
        draw.rounded_rectangle((vx, y + 17, vx + vw, y + 43), radius=8,
                               fill=PAPER if selected else PAPER,
                               outline=PAPER if selected else INK, width=1)
        text(draw, (vx + 9, y + 22), value, 13, True, INK)
    else:
        draw.line((x + w - 23, y + 24, x + w - 16, y + 30), fill=ink, width=2)
        draw.line((x + w - 16, y + 30, x + w - 23, y + 36), fill=ink, width=2)


def footer(draw, back=False, favorite=False):
    if back:
        draw.rounded_rectangle((20, 604, 101, 632), radius=13, outline=INK, width=1)
        text(draw, (32, 609), "‹ Back", 13, True)
    # Page dots, centered in the footer area.
    for i in range(3):
        x = 222 + i * 18
        draw.ellipse((x, 614, x + 9, 623), fill=INK if i == 0 else PAPER, outline=INK, width=1)
    if favorite:
        draw.rounded_rectangle((368, 604, 452, 632), radius=13, fill=INK, outline=INK)
        text(draw, (382, 609), "♥  Fav", 13, True, PAPER)


def panel_header(draw, label, title):
    text(draw, (24, 17), label.upper(), 13)
    text(draw, (24, 39), fitted(draw, title, 395, 28, True), 28, True)
    draw.rectangle((20, 72, 452, 74), fill=INK)
    draw.rectangle((20, 70, 92, 77), fill=INK)


def panel_footer(draw):
    dither(draw, (20, 582, 452, 590), 5)
    draw.rectangle((20, 592, 452, 594), fill=INK)
    draw.rounded_rectangle((24, 600, 370, 630), radius=14, outline=INK, width=1)
    text(draw, (36, 607), "Double-tap Up / Down: scroll a page", 13, True)
    text(draw, (405, 610), "84%", 12, True)


def stat_tile(draw, x, value, caption):
    w, y, h = 133, 96, 88
    dither(draw, (x + 3, y + 3, x + w, y + h + 3), 5)
    draw.rounded_rectangle((x, y, x + w, y + h), radius=9, fill=PAPER, outline=INK, width=2)
    label = str(value)
    bb = draw.textbbox((0, 0), label, font=font(31, True))
    text(draw, (x + (w - bb[2]) // 2, y + 11), label, 31, True)
    bb = draw.textbbox((0, 0), caption, font=font(12, True))
    text(draw, (x + (w - bb[2]) // 2, y + 60), caption, 12, True)


def make_spread(name, draw_left, draw_right):
    canvas = Image.new("RGB", (1040, 744), "#eeede6")
    outer = ImageDraw.Draw(canvas)
    outer.rounded_rectangle((18, 18, 1022, 700), radius=22, fill="#292927")
    outer.rounded_rectangle((29, 29, 1011, 683), radius=8, fill="#111111")
    left_page = Image.new("RGB", (PAGE_W, PAGE_H), PAPER)
    right_page = Image.new("RGB", (PAGE_W, PAGE_H), PAPER)
    draw_left(ImageDraw.Draw(left_page))
    draw_right(ImageDraw.Draw(right_page))
    canvas.paste(left_page, (32, 32))
    canvas.paste(right_page, (518, 32))
    outer = ImageDraw.Draw(canvas)
    # A small external caption makes it impossible to mistake these for captures.
    outer.text((32, 710), "SOURCE-DERIVED UI MOCKUP  ·  NOT A DEVICE CAPTURE",
               font=font(13, True), fill="#393935")
    outer.text((865, 710), "960 × 648 spread", font=font(12), fill="#5d5d57")
    canvas.save(OUT / name, optimize=True)


def home_left(draw):
    header(draw, main=True)
    # The hero card mirrors the firmware's large Now Reading card.
    x, y, w, h = 20, 82, 432, 80
    dither(draw, (x + 4, y + 4, x + w, y + h + 4), 5)
    draw.rounded_rectangle((x, y, x + w, y + h), radius=9, fill=PAPER, outline=INK, width=2)
    draw.rounded_rectangle((32, 91, 76, 153), radius=6, fill=INK)
    draw.line((41, 98, 41, 145), fill=PAPER, width=2)
    text(draw, (50, 108), "P", 23, True, PAPER)
    text(draw, (88, 91), "NOW READING", 12)
    draw.rectangle((88, 108, 180, 109), fill=INK)
    text(draw, (88, 115), "Project Hail Mary", 16, True)
    draw.rounded_rectangle((88, 139, 389, 151), radius=5, outline=INK, width=1)
    draw.rounded_rectangle((89, 140, 237, 150), radius=4, fill=INK)
    text(draw, (403, 137), "49%", 12, True)
    for i, row in enumerate([
        ("Library", "Open & read books", "B", True),
        ("Settings", "Edit device and reading settings", "S", False),
        ("Transfer files", "Connect over USB mass storage", "U", False),
    ]):
        page_card(draw, 176 + i * 80, row[0], row[1], row[2], row[3])
    footer(draw)


def home_right(draw):
    panel_header(draw, "Overview", "Your library")
    stat_tile(draw, 20, 42, "Books")
    stat_tile(draw, 169, 6, "Favorites")
    stat_tile(draw, 318, 14, "Groups")
    text(draw, (24, 207), "FAVORITES", 13, True)
    draw.line((24, 226, 107, 226), fill=INK, width=1)
    for i, title in enumerate([
        "Project Hail Mary",
        "The Left Hand of Darkness",
        "A Psalm for the Wild-Built",
        "The Dispossessed",
        "Kindred",
    ]):
        yy = 243 + 38 * i
        draw.ellipse((25, yy + 5, 34, yy + 14), fill=INK)
        text(draw, (44, yy), fitted(draw, title, 385, 14), 14)
    panel_footer(draw)


def series_left(draw):
    header(draw, "Library", "Modern Reader")
    rows = [
        ("Favorites", "Books: 6", "♥", False),
        ("The Expanse", "Books: 9", "E", True),
        ("Earthsea Cycle", "Books: 6", "E", False),
        ("Murderbot Diaries", "Books: 7", "M", False),
        ("Unsorted", "Books: 4", "U", False),
    ]
    for i, row in enumerate(rows):
        page_card(draw, 82 + i * 80, row[0], row[1], row[2], row[3])
    footer(draw, back=True)


def series_right(draw):
    panel_header(draw, "Group", "The Expanse")
    text(draw, (24, 91), "9 books", 14, True)
    for i, title in enumerate([
        "Leviathan Wakes",
        "Caliban's War",
        "Abaddon's Gate",
        "Cibola Burn",
        "Nemesis Games",
        "Babylon's Ashes",
        "Persepolis Rising",
    ]):
        yy = 128 + i * 47
        draw.rounded_rectangle((26, yy + 2, 40, yy + 20), radius=3, fill=INK)
        draw.rectangle((30, yy + 6, 32, yy + 17), fill=PAPER)
        text(draw, (54, yy), fitted(draw, title, 380, 14), 14)
    panel_footer(draw)


def settings_left(draw):
    header(draw, "Device settings", "Settings")
    rows = [
        ("Library grouping", "Group books by author, title/series, or folder", "L", True, "Title/series"),
        ("Battery indicator", "Show or hide the battery indicator", "B", False, "Shown"),
        ("Haptic buzzer", "Enable or disable buzzer", "H", False, "Enabled"),
        ("Dark mode", "Invert all colors", "D", False, "Disabled"),
        ("Sunlight mode", "Reduce artefacts in sunlight", "S", False, "Disabled"),
        ("Data storage location", "Store book data on internal memory or SD", "SD", False, "SD"),
    ]
    for i, row in enumerate(rows):
        page_card(draw, 82 + i * 80, row[0], row[1], row[2], row[3], row[4])
    footer(draw, back=True)


def settings_right(draw):
    panel_header(draw, "About / Help", "Library grouping")
    text(draw, (24, 106), "Choose how books are organized", 16, True)
    text(draw, (24, 144), "in the library:", 16, True)
    points = [
        "•  Title / series (default)",
        "•  Author",
        "•  Folder",
    ]
    for i, line in enumerate(points):
        text(draw, (30, 198 + i * 38), line, 15)
    draw.line((24, 337, 430, 337), fill=LIGHT, width=2)
    text(draw, (24, 365), "Series and volume details are read", 14)
    text(draw, (24, 389), "from EPUB metadata. Folder mode", 14)
    text(draw, (24, 413), "also scans subfolders under Books/.", 14)
    panel_footer(draw)


def chapter_left(draw):
    text(draw, (24, 18), "CONTENTS", 14, True)
    text(draw, (24, 43), "Chapters", 28, True)
    draw.rounded_rectangle((380, 43, 460, 70), radius=13, outline=INK, width=1)
    text(draw, (395, 49), "3 / 15", 13, True)
    draw.rectangle((20, 76, 452, 78), fill=INK)
    titles = [
        "Chapter 1  ·  Awake",
        "Chapter 2  ·  First steps",
        "Chapter 3  ·  The signal",
        "Chapter 4  ·  A new plan",
        "Chapter 5  ·  The crossing",
        "Chapter 6  ·  New discoveries",
        "Chapter 7  ·  A visitor",
        "Chapter 8  ·  The long way",
        "Chapter 9  ·  An answer",
        "Chapter 10  ·  Home",
    ]
    for i, title in enumerate(titles):
        y = 92 + i * 45
        if i == 2:
            draw.rounded_rectangle((20, y - 4, 452, y + 32), radius=6, fill=INK)
            text(draw, (34, y + 3), title, 15, True, PAPER)
            draw.rectangle((26, y + 10, 32, y + 16), fill=PAPER)
        else:
            text(draw, (34, y + 3), fitted(draw, title, 395, 15), 15)
            draw.line((34, y + 33, 440, y + 33), fill="#deded9", width=1)
    draw.rounded_rectangle((20, 604, 258, 632), radius=14, outline=INK, width=1)
    text(draw, (32, 611), "Middle: open   Left: back", 12, True)


def chapter_right(draw):
    text(draw, (24, 18), "CONTENTS · CONTINUED", 14, True)
    text(draw, (24, 43), "Chapters", 28, True)
    draw.rectangle((20, 76, 452, 78), fill=INK)
    titles = [
        "Chapter 11  ·  The final test",
        "Chapter 12  ·  A decision",
        "Chapter 13  ·  Home again",
        "Chapter 14  ·  Epilogue",
    ]
    for i, title in enumerate(titles):
        y = 92 + i * 45
        text(draw, (34, y + 3), fitted(draw, title, 395, 15), 15)
        draw.line((34, y + 33, 440, y + 33), fill="#deded9", width=1)
    draw.rounded_rectangle((20, 604, 270, 632), radius=14, outline=INK, width=1)
    text(draw, (32, 611), "Double-tap Up / Down: page", 12, True)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    make_spread("home-ui-mockup.png", home_left, home_right)
    make_spread("series-library-mockup.png", series_left, series_right)
    make_spread("settings-ui-mockup.png", settings_left, settings_right)
    make_spread("chapter-list-ui-mockup.png", chapter_left, chapter_right)
    print(f"Generated UI mockups in {OUT}")


if __name__ == "__main__":
    main()
