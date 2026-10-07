// Drawing primitives used by the menu UI (cards, chevrons, meters, scrollbar).
// Coordinates follow the rest of the renderer: x runs left->right (0..EPD_HEIGHT-1),
// y runs bottom->top (0..EPD_WIDTH-1). "ink" = true draws black, false draws white.
#include "renderer.h"
#include <cmath>
#include <algorithm>
#include <vector>

// Filled rectangle with rounded corners
void Renderer::fillRounded(int x, int y, int w, int h, int radius, bool ink)
{
    if (w <= 0 || h <= 0) return;
    radius = std::max(0, std::min(radius, std::min(w, h) / 2));
    for (int row = 0; row < h; row++) {
        int inset = 0;
        int fromEdge = std::min(row, h - 1 - row); // distance from the nearest horizontal edge
        if (fromEdge < radius) {
            float dy = radius - fromEdge - 0.5f;
            float dx = std::sqrt(std::max(0.0f, (float)(radius * radius) - dy * dy));
            inset = radius - (int)dx;
        }
        drawSquare(x + inset, y + row, w - 2 * inset, 1, !ink);
    }
}

// A menu card: outlined with a small offset shadow, or solid black when selected
void Renderer::drawCard(int x, int y, int w, int h, bool selected)
{
    const int r = 8;
    if (selected) {
        fillRounded(x, y, w, h, r, true);
        return;
    }
    fillRoundedDither(x + 4, y - 4, w, h, r, true);    // soft (dithered) shadow
    fillRounded(x, y, w, h, r, true);                   // border
    fillRounded(x + 2, y + 2, w - 4, h - 4, r - 2, false); // paper
}

// Small ">" arrow, 10px wide and 16px tall, (x,y) is the bottom left corner
void Renderer::drawChevron(int x, int y, bool ink)
{
    for (int i = 0; i < 8; i++) {
        drawSquare(x + i, y + 14 - i, 2, 2, !ink); // upper arm
        drawSquare(x + i, y + i, 2, 2, !ink);      // lower arm
    }
}

// Horizontal meter (reading progress): rounded outline with a filled part
void Renderer::drawMeter(int x, int y, int w, int h, int percent, bool ink)
{
    percent = std::max(0, std::min(100, percent));
    fillRounded(x, y, w, h, h / 2, ink);
    fillRounded(x + 2, y + 2, w - 4, h - 4, std::max(0, h / 2 - 2), !ink);
    int fill = ((w - 8) * percent) / 100;
    if (fill > 0) fillRounded(x + 4, y + 4, std::max(fill, h - 8), h - 8, std::max(0, (h - 8) / 2), ink);
}

// Thin vertical scrollbar. top/bottom are the y limits (top > bottom), x is the left edge
void Renderer::drawScrollbar(int x, int top, int bottom, int index, int count, int perPage)
{
    if (count <= perPage) return;
    int trackH = top - bottom;
    drawSquare(x + 1, bottom, 2, trackH, false); // track
    int thumbH = std::max(24, trackH * perPage / count);
    int travel = trackH - thumbH;
    int thumbTop = top - (count > 1 ? (travel * index) / (count - 1) : 0);
    fillRounded(x, thumbTop - thumbH, 4, thumbH, 2, true);
}

int Renderer::measureText(const std::string &text, bool bold)
{
    int width = 0;
    for (int cp : utf8ToCodePoints(text)) {
        width += 8 + 8 * (fontHandler.getCharWidth(cp) - 1) + (bold ? 1 : 0);
    }
    return width;
}

// Shorten text with "..." so it fits in maxWidth pixels
std::string Renderer::fitText(const std::string &text, int maxWidth, bool bold)
{
    if (measureText(text, bold) <= maxWidth) return text;
    std::vector<int> cps = utf8ToCodePoints(text);
    const int dots = 3 * (8 + (bold ? 1 : 0));
    std::string out;
    int width = 0;
    for (int cp : cps) {
        int cw = 8 + 8 * (fontHandler.getCharWidth(cp) - 1) + (bold ? 1 : 0);
        if (width + cw + dots > maxWidth) break;
        width += cw;
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out + "...";
}


// Rounded rectangle filled with a checkerboard pattern, looks like a light grey on the e-ink display
void Renderer::fillRoundedDither(int x, int y, int w, int h, int radius, bool ink)
{
    if (w <= 0 || h <= 0) return;
    radius = std::max(0, std::min(radius, std::min(w, h) / 2));
    for (int row = 0; row < h; row++) {
        int inset = 0;
        int fromEdge = std::min(row, h - 1 - row);
        if (fromEdge < radius) {
            float dy = radius - fromEdge - 0.5f;
            float dx = std::sqrt(std::max(0.0f, (float)(radius * radius) - dy * dy));
            inset = radius - (int)dx;
        }
        for (int col = inset; col < w - inset; col++) {
            if (((x + col) + (y + row)) & 1) drawPixel(x + col, y + row, !ink);
        }
    }
}

void Renderer::fillDither(int x, int y, int w, int h, bool ink)
{
    for (int row = 0; row < h; row++)
        for (int col = 0; col < w; col++)
            if (((x + col) + (y + row)) & 1) drawPixel(x + col, y + row, !ink);
}

void Renderer::fillDisc(int cx, int cy, int radius, bool ink)
{
    for (int dy = -radius; dy <= radius; dy++) {
        int half = (int)std::sqrt((float)(radius * radius - dy * dy) + 0.5f);
        drawSquare(cx - half, cy + dy, 2 * half + 1, 1, !ink);
    }
}

// Pill shaped button: outlined, or filled
void Renderer::drawPill(int x, int y, int w, int h, bool filled)
{
    fillRounded(x, y, w, h, h / 2, true);
    if (!filled) fillRounded(x + 2, y + 2, w - 4, h - 4, std::max(0, h / 2 - 2), false);
}

// Small icons for the menu, drawn from simple shapes. 28x28 pixels, (x,y) is the bottom left corner.
// "ink" is the color of the icon (true = black), the icon is drawn on a background of the opposite color.
//  1 book, 2 help, 3 gear, 4 transfer, 5 info, 6 heart, 7 sliders, 8 display, 9 text
void Renderer::drawIcon(int icon, int x, int y, bool ink)
{
    const int cx = x + 14, cy = y + 14;
    switch (icon) {
    case 1: // open book
        fillRounded(x + 1, y + 4, 12, 20, 2, ink);
        fillRounded(x + 15, y + 4, 12, 20, 2, ink);
        for (int i = 0; i < 3; i++) {
            drawSquare(x + 3, y + 9 + i * 5, 8, 2, !ink);
            drawSquare(x + 17, y + 9 + i * 5, 8, 2, !ink);
        }
        break;
    case 2: // question mark in a circle
        fillDisc(cx, cy, 13, ink);
        fillDisc(cx, cy, 10, !ink);
        drawCharacter(x + 10, y + 6, '?', true, false, 1, ink);
        break;
    case 3: // gear
        for (int k = 0; k < 8; k++) {
            float a = k * 3.14159265f / 4.0f;
            int tx = cx + (int)std::lround(11.0f * std::cos(a));
            int ty = cy + (int)std::lround(11.0f * std::sin(a));
            drawSquare(tx - 2, ty - 2, 5, 5, !ink);
        }
        fillDisc(cx, cy, 9, ink);
        fillDisc(cx, cy, 4, !ink);
        break;
    case 4: // transfer: arrow up + arrow down
        drawSquare(x + 6, y + 3, 4, 16, !ink);
        for (int r = 0; r < 7; r++) drawSquare(x + 8 - (r + 1), y + 25 - r, 2 * (r + 1), 1, !ink);
        drawSquare(x + 18, y + 9, 4, 16, !ink);
        for (int r = 0; r < 7; r++) drawSquare(x + 20 - (r + 1), y + 2 + r, 2 * (r + 1), 1, !ink);
        break;
    case 5: // info
        fillDisc(cx, cy, 13, ink);
        drawCharacter(x + 10, y + 6, 'i', true, false, 1, !ink);
        break;
    case 6: // heart
        fillDisc(x + 8, y + 19, 7, ink);
        fillDisc(x + 20, y + 19, 7, ink);
        for (int t = 0; t < 17; t++) {
            int half = std::min(13, t + 2);
            drawSquare(cx - half, y + 2 + t, 2 * half, 1, !ink);
        }
        break;
    case 7: // sliders
        for (int i = 0; i < 3; i++) drawSquare(x + 1, y + 5 + i * 9, 26, 2, !ink);
        {
            const int knobs[3] = {19, 9, 15};
            for (int i = 0; i < 3; i++) {
                fillDisc(x + knobs[i], y + 6 + i * 9, 4, !ink);
                fillDisc(x + knobs[i], y + 6 + i * 9, 2, ink);
            }
        }
        break;
    case 8: // display
        fillRounded(x + 1, y + 8, 26, 18, 3, ink);
        fillRounded(x + 3, y + 10, 22, 14, 2, !ink);
        drawSquare(x + 6, y + 19, 12, 2, ink);
        drawSquare(x + 6, y + 14, 16, 2, ink);
        drawSquare(x + 12, y + 4, 4, 4, ink);
        drawSquare(x + 7, y + 2, 14, 2, ink);
        break;
    case 9: // text size
        drawCharacter(x + 6, y + 6, 'A', true, false, 1, ink);
        drawCharacter(x + 15, y + 6, 'a', false, false, 1, ink);
        break;
    default:
        break;
    }
}


// Split text into lines of at most maxWidth pixels (at the given scale); the last line gets "..." when it does not fit
std::vector<std::string> Renderer::wrapText(const std::string &text, int maxWidth, bool bold, int scale, int maxLines)
{
    std::vector<std::string> lines;
    std::string current;
    const int limit = std::max(8, maxWidth / std::max(1, scale));
    size_t i = 0;
    auto flush = [&]() { if (!current.empty()) { lines.push_back(current); current.clear(); } };
    while (i < text.size() && (int)lines.size() < maxLines) {
        size_t sp = text.find(' ', i);
        std::string word = text.substr(i, sp == std::string::npos ? std::string::npos : sp - i);
        i = (sp == std::string::npos) ? text.size() : sp + 1;
        if (word.empty()) continue;
        std::string candidate = current.empty() ? word : current + " " + word;
        if (measureText(candidate, bold) <= limit) {
            current = candidate;
        } else {
            flush();
            if ((int)lines.size() >= maxLines) break;
            // a single word that is too long gets cut
            current = (measureText(word, bold) <= limit) ? word : fitText(word, limit, bold);
        }
    }
    if ((int)lines.size() < maxLines) flush();
    bool truncated = i < text.size() || (int)lines.size() > maxLines;
    if ((int)lines.size() > maxLines) lines.resize(maxLines);
    if (truncated && !lines.empty()) {
        std::string &last = lines.back();
        if (last.size() < 3 || last.compare(last.size() - 3, 3, "...") != 0) last = fitText(last + " ...", limit, bold);
    }
    return lines;
}


void Renderer::drawBitmap1bpp(int x, int y, int w, int h, const uint8_t *bits, bool ink)
{
    const int rowBytes = (w + 7) / 8;
    for (int row = 0; row < h; row++) {
        const int py = y + (h - 1 - row); // logical y runs from the bottom up
        const uint8_t *line = bits + row * rowBytes;
        int runStart = -1;
        for (int col = 0; col <= w; col++) {
            const bool on = (col < w) && (line[col >> 3] & (0x80 >> (col & 7)));
            if (on && runStart < 0) runStart = col;
            if (!on && runStart >= 0) {
                drawSquare(x + runStart, py, col - runStart, 1, !ink); // false = ink in drawSquare
                runStart = -1;
            }
        }
    }
}
