// The info panel on the right page of the menu: overview, book details, group contents, setting help.
#include "menuPanel.h"
#include <algorithm>
#include <cctype>

#define PANEL_X 20
#define PANEL_W 432

static std::string upperCopy(std::string s)
{
    for (auto &c : s) c = (char)toupper((unsigned char)c);
    return s;
}

// Same header style as the menu on the left page: small label above a big title, with an accent rule
static void drawPanelHeader(Renderer &r, const std::string &label, const std::string &title)
{
    r.drawString(PANEL_X + 4, 626, r.fitText(upperCopy(label), PANEL_W - 8, false), 1, false, false, true);
    r.drawString(PANEL_X + 4, 586, r.fitText(title, (PANEL_W - 8) / 2, true), 2, true, false, true);
    r.drawSquare(PANEL_X, 575, PANEL_W, 2, false);
    r.drawSquare(PANEL_X, 572, 72, 6, false);
}

// dithered band at the bottom, matches the band of the main menu
static void drawPanelFooter(Renderer &r)
{
    r.fillDither(PANEL_X, 44, PANEL_W, 8, true);
    r.drawSquare(PANEL_X, 52, PANEL_W, 2, false);
}

// tooltip about the double tap shortcut, shown at the bottom of every panel
static void drawScrollHint(Renderer &r)
{
    const std::string hint = "Double-tap Up / Down: scroll a page";
    int pillW = r.measureText(hint, true) + 32;
    r.drawPill(PANEL_X + 4, 58, pillW, 26, false);
    r.drawString(PANEL_X + 4 + 16, 63, hint, 1, true, false, true);
}

static void drawLabel(Renderer &r, int x, int y, const std::string &text)
{
    r.drawString(x, y, text, 1, false, false, true);
    r.drawSquare(x, y - 4, r.measureText(text, false), 1, false);
}

// a framed block with a big number and a caption
static void drawStatTile(Renderer &r, int x, int y, int w, int h, int value, const std::string &caption)
{
    r.fillRoundedDither(x + 4, y - 4, w, h, 8, true); // shadow in "grey"
    r.drawCard(x, y, w, h, false);
    std::string number = std::to_string(value);
    int numberWidth = 3 * r.measureText(number, true);
    r.drawString(x + (w - numberWidth) / 2, y + h - 56, number, 3, true, false, true);
    int captionWidth = r.measureText(caption, true);
    r.drawString(x + (w - captionWidth) / 2, y + 12, caption, 1, true, false, true);
}

// a book "cover": black block with a spine, the initial letter and a few lines
static void drawCover(Renderer &r, int x, int y, int w, int h, const std::string &title)
{
    r.fillRoundedDither(x + 4, y - 4, w, h, 8, true);        // shadow in "grey"
    r.fillRounded(x, y, w, h, 8, true);
    r.drawSquare(x + 14, y + 8, 2, h - 16, true);            // spine line (white)
    int cp = '#';
    std::string stripped = title;
    for (int c : utf8ToCodePoints(stripped)) {
        if (isalnum(c) || c >= 128) { cp = (c < 128) ? toupper(c) : c; break; }
    }
    int scale = std::max(2, std::min((h - 70) / 16, (w - 40) / 8));
    int glyphWidth = (8 + 8 * (r.fontHandler.getCharWidth(cp) - 1)) * scale;
    r.drawCharacter(x + 22 + (w - 22 - glyphWidth) / 2, y + h - 24 - 16 * scale, cp, true, false, scale, false);
    for (int i = 0; i < 3; i++) {
        r.drawSquare(x + 30, y + 14 + i * 7, (w - 46) - i * 16, 2, true);
    }
}

// draw wrapped text from topY downwards, returns the y below the last line
static int drawWrapped(Renderer &r, int x, int topY, int width, const std::string &text, bool bold, int scale, int maxLines, int lineHeight)
{
    std::vector<std::string> lines = r.wrapText(text, width, bold, scale, maxLines);
    int y = topY;
    for (const std::string &line : lines) {
        r.drawString(x, y - 16 * scale, line, scale, bold, false, true);
        y -= lineHeight;
    }
    return y;
}

static void drawProgress(Renderer &r, int x, int y, int w, int percent)
{
    r.drawMeter(x, y, w - 90, 16, std::max(0, percent), true);
    std::string label = percent >= 0 ? std::to_string(percent) + "% read" : std::string("new");
    r.drawString(x + w - r.measureText(label, true), y, label, 1, true, false, true);
}

void drawMenuPanel(Renderer &r, const PanelInfo &info)
{
    switch (info.kind) {
    case PanelInfo::Kind::Home: {
        drawPanelHeader(r, "Overview", "Your library");
        const int tileW = (PANEL_W - 2 * 16) / 3;
        drawStatTile(r, PANEL_X, 470, tileW, 90, info.bookCount, "Books");
        drawStatTile(r, PANEL_X + tileW + 16, 470, tileW, 90, info.favoriteCount, "Favorites");
        drawStatTile(r, PANEL_X + 2 * (tileW + 16), 470, tileW, 90, info.groupCount, "Groups");

        drawLabel(r, PANEL_X + 4, 430, "FAVORITES");
        int ly = 396;
        for (const std::string &item : info.list) {
            r.fillDisc(PANEL_X + 10, ly + 6, 5, true);
            r.drawString(PANEL_X + 28, ly, r.fitText(item, PANEL_W - 28, false), 1, false, false, true);
            ly -= 30;
            if (ly < 118) break;
        }
        if (info.list.empty()) {
            drawWrapped(r, PANEL_X + 4, 410, PANEL_W - 8, "Press Right on a book in the library to add it to your favorites.", false, 1, 3, 24);
        } else if (info.moreCount > 0 && ly >= 86) {
            r.drawString(PANEL_X + 28, ly, "+ " + std::to_string(info.moreCount) + " more", 1, true, false, true);
        }
        drawScrollHint(r);
        drawPanelFooter(r);
        break;
    }
    case PanelInfo::Kind::BookDetail: {
        drawPanelHeader(r, "Book", "Details");
        drawCover(r, PANEL_X + 4, 372, 150, 190, info.title);
        int tx = PANEL_X + 150 + 28;
        int tw = PANEL_W - 150 - 28;
        int y = 560;
        if (!info.subtitle.empty()) {
            drawLabel(r, tx, y - 14, "AUTHOR");
            y = drawWrapped(r, tx, y - 26, tw, info.subtitle, true, 1, 3, 20) - 12;
        }
        if (!info.series.empty()) {
            drawLabel(r, tx, y - 14, "SERIES");
            y = drawWrapped(r, tx, y - 26, tw, info.series, true, 1, 3, 20) - 12;
        }
        if (!info.pages.empty()) {
            drawLabel(r, tx, y - 14, "PAGE");
            y = drawWrapped(r, tx, y - 26, tw, info.pages, true, 1, 1, 20) - 12;
        }
        // full title below the cover
        r.drawSquare(PANEL_X, 350, PANEL_W, 1, false);
        drawWrapped(r, PANEL_X + 4, 336, PANEL_W - 8, info.title, true, 2, 4, 36);
        drawProgress(r, PANEL_X + 4, 160, PANEL_W - 8, info.percent);

        // favorite hint in a pill
        const std::string hint = info.favorite ? "In your favorites" : "Press Right to favorite";
        int pillW = r.measureText(hint, true) + 56;
        r.drawPill(PANEL_X + 4, 96, pillW, 32, info.favorite);
        r.drawString(PANEL_X + 4 + 40, 104, hint, 1, true, false, !info.favorite);
        r.drawCharacter(PANEL_X + 4 + 14, 104, 10084, true, false, 1, !info.favorite);
        drawScrollHint(r);
        drawPanelFooter(r);
        break;
    }
    case PanelInfo::Kind::Group: {
        drawPanelHeader(r, "Group", info.title);
        r.drawString(PANEL_X + 4, 540, r.fitText(info.subtitle, PANEL_W - 8, true), 1, true, false, true);
        int ly = 496;
        for (const std::string &item : info.list) {
            r.fillRounded(PANEL_X + 6, ly + 1, 12, 16, 2, true);
            r.drawSquare(PANEL_X + 9, ly + 4, 2, 10, false);
            r.drawString(PANEL_X + 32, ly, r.fitText(item, PANEL_W - 36, false), 1, false, false, true);
            ly -= 34;
            if (ly < 120) break;
        }
        if (info.moreCount > 0 && ly >= 86) {
            r.drawString(PANEL_X + 32, ly, "+ " + std::to_string(info.moreCount) + " more", 1, true, false, true);
        }
        if (info.list.empty()) {
            drawWrapped(r, PANEL_X + 4, 500, PANEL_W - 8, "No books in this group yet.", false, 1, 2, 24);
        }
        drawScrollHint(r);
        drawPanelFooter(r);
        break;
    }
    case PanelInfo::Kind::Help:
    default: {
        drawPanelHeader(r, "About", "Help");
        int y = drawWrapped(r, PANEL_X + 4, 548, PANEL_W - 8, info.title, true, 2, 3, 36);
        r.drawSquare(PANEL_X + 4, y - 4, 56, 4, false);
        drawWrapped(r, PANEL_X + 4, y - 30, PANEL_W - 8, info.body, false, 2, 9, 34);
        drawScrollHint(r);
        drawPanelFooter(r);
        break;
    }
    }
}
