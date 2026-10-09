#include "bookMarkMenuHandler.h"
#include "esp_log.h"
#include "device.h"

BookMarkMenuHandler::BookMarkMenuHandler(Book *book, Renderer *renderer,Epub *epub,unsigned char* leftPageFrameBuffer,unsigned char* rightPageFrameBuffer)
{
    this->renderer = renderer;
    this->book = book;
    this->epub = epub;
    this->leftPageFrameBuffer = leftPageFrameBuffer;
    this->rightPageFrameBuffer = rightPageFrameBuffer;
    this->preservedFramebuffer = (unsigned char*)calloc(EPD_WIDTH * EPD_HEIGHT / 8,sizeof(unsigned char));
    memcpy(preservedFramebuffer, leftPageFrameBuffer, EPD_WIDTH * EPD_HEIGHT / 8);
}

BookMarkMenuHandler::~BookMarkMenuHandler()
{
    free(preservedFramebuffer);
    if (preservedRightFramebuffer) free(preservedRightFramebuffer);
}

std::string BookMarkMenuHandler::toggleLabel(int row)
{
    Device &dev = Device::getInstance();
    if (row == ToggleDarkMode) return std::string("Dark mode: ") + (dev.deviceSettings.nightMode ? "On" : "Off");
    if (row == ToggleSunlight) return std::string("Sunlight mode: ") + (dev.deviceSettings.sunlightMode ? "On" : "Off");
    return std::string("Progress: ") + (dev.deviceSettings.showPagePercentage ? "Percent" : "Pages");
}

void BookMarkMenuHandler::drawMenu()
{
    memcpy(leftPageFrameBuffer, preservedFramebuffer, EPD_WIDTH * EPD_HEIGHT / 8);

    renderer->framebuffer = leftPageFrameBuffer;
    int menuElementWidth = 150;
    int menuElementHeight = 50;
    renderer->drawSquare(0,0,EPD_HEIGHT,menuElementHeight+2,true);
    for(int i=0;i<menuElements.size();i++)
    {
        int pos = i - currentMenuElementIndex;

        int n = static_cast<int>(menuElements.size());
        int half = n / 2;

        if (pos > half) pos -= n;
        else if (pos < -half) pos += n;

        bool selected = (i==currentMenuElementIndex);// && (currentVerticalElementIndex==0);
        renderer->drawPaddedBox(EPD_HEIGHT/2-menuElementWidth/2+pos*menuElementWidth-2,0,menuElementWidth+4,menuElementHeight,4,!selected);
        std::string elementText = menuElements[i].text;
        int stringLength = elementText.length();
        renderer->drawString(EPD_HEIGHT/2-GLYPH_WIDTH*stringLength/4 + pos*menuElementWidth,menuElementHeight/2-GLYPH_HEIGHT/2,elementText,1,true,false,!selected);
    }

    int subElementHeight = 0;
    int scrollOffset = 0;
    if(menuElements[currentMenuElementIndex].ID==MenuElementID::Toggles || menuElements[currentMenuElementIndex].ID==MenuElementID::GotoMark)
    {
        const int MAX_VISIBLE_ITEMS = 20;

        if(menuElements[currentMenuElementIndex].ID==MenuElementID::Toggles) maxVerticalElements = TOGGLE_COUNT;
        if(menuElements[currentMenuElementIndex].ID==MenuElementID::GotoMark) maxVerticalElements = book->bookMarks.size();

        // Calculate scroll offset based on current selection
        scrollOffset = 0;
        if (maxVerticalElements > MAX_VISIBLE_ITEMS) {
            // ensure the selected item is visible
            if (currentVerticalElementIndex > MAX_VISIBLE_ITEMS-1)
                scrollOffset = currentVerticalElementIndex - MAX_VISIBLE_ITEMS+1;
        }
        if(currentVerticalElementIndex==maxVerticalElements && scrollOffset!=0) scrollOffset -= 1;

        // Determine how many items to display in this window
        int visibleCount = std::min(maxVerticalElements - scrollOffset, MAX_VISIBLE_ITEMS);

        // Calculate total height
    subElementHeight = (visibleCount + 1) * GLYPH_HEIGHT;

        renderer->drawSquare(
            EPD_HEIGHT / 2 - menuElementWidth / 2 - 4,
            menuElementHeight,
            menuElementWidth + 8,
            subElementHeight - 2,
            true
        );
        renderer->drawPaddedBox(
            EPD_HEIGHT / 2 - menuElementWidth / 2 - 2,
            menuElementHeight - 4,
            menuElementWidth + 4,
            subElementHeight,
            4,
            true
        );

        // Draw "ARROW UP" if not at top
        if (scrollOffset+MAX_VISIBLE_ITEMS < maxVerticalElements) {
             renderer->drawCharacter(
                EPD_HEIGHT / 2- GLYPH_WIDTH/4,
                menuElementHeight + (MAX_VISIBLE_ITEMS-1) * GLYPH_HEIGHT + 6,
                8593,
                false,
                false,
                1,
                true
            );
        }

        // Draw visible chapters
        for (int i = 0; i < visibleCount - (scrollOffset+MAX_VISIBLE_ITEMS < maxVerticalElements); i++) {
            int chapterIndex = i + scrollOffset;
            if(scrollOffset!=0 && i==0) continue;
            //if(scrollOffset+MAX_VISIBLE_ITEMS < maxVerticalElements && i==visibleCount-1) continue;
            bool selected = (chapterIndex == currentVerticalElementIndex - 1);

            std::string itemString;
            if(menuElements[currentMenuElementIndex].ID==MenuElementID::Toggles) itemString = toggleLabel(chapterIndex);
            if(menuElements[currentMenuElementIndex].ID==MenuElementID::GotoMark) itemString = "Page: " + std::to_string(book->bookMarks[chapterIndex].pageIndex+1) + "-" + std::to_string(book->bookMarks[chapterIndex].pageIndex+2);
            //ESP_LOGI("QuickMenu", "toc name: %s", itemString.c_str());

            int y = menuElementHeight + (i) * GLYPH_HEIGHT + 6;
            if (selected)
                renderer->drawSquare(
                    EPD_HEIGHT / 2 - itemString.length() * GLYPH_WIDTH / 4,
                    y,
                    itemString.length() * GLYPH_WIDTH / 2 + 1,
                    GLYPH_HEIGHT,
                    !selected
                );

            renderer->drawString(
                EPD_HEIGHT / 2 - itemString.length() * GLYPH_WIDTH / 4,
                y,
                itemString,
                1,
                false,
                false,
                !selected
            );
        }

        // Draw "ARROW DOWN" if not at end
        if (scrollOffset!=0) {
            renderer->drawCharacter(
                EPD_HEIGHT / 2- GLYPH_WIDTH/4,
                menuElementHeight + 6,
                8595,
                false,
                false,
                1,
                true
            );
        }
    }



    if(currentVerticalElementIndex==0 || forceFullDraw)
    {
        forceFullDraw = false;
        renderer->epd.DisplayPictureBoth(leftPageFrameBuffer,rightPageFrameBuffer);
    }
    else 
    {
        if(scrollOffset==0) renderer->epd.partialUpdatesRemaining[true] = 2;
        renderer->epd.DisplayPicturePartial(true, leftPageFrameBuffer,
                                EPD_HEIGHT / 2 - menuElementWidth / 2 - 4, menuElementHeight,
                                EPD_HEIGHT / 2 + menuElementWidth / 2 + 4, menuElementHeight+subElementHeight - 2);
    }
}


// ---------------------------------------------------------------------------------------------
// Full screen chapter list. Compact rows span both screens, with a shared header style.
// ---------------------------------------------------------------------------------------------
#define CL_LINE_H 20          // height of one text line
#define CL_ROW_PAD 4          // padding of a row
#define CL_MARGIN 14
#define CL_LEFT_TOP 548       // first row below the two-level header
#define CL_RIGHT_TOP 548      // match the left page; the right header says "continued"
#define CL_BOTTOM 52          // rows stop above the footer hint
#define CL_TEXT_X (CL_MARGIN + 16)
#define CL_MAX_LINES 2

static std::string cleanTitle(const std::string &in)
{
    std::string out;
    bool lastSpace = true;
    for (unsigned char c : in)
    {
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t')
        {
            if (!lastSpace) out += ' ';
            lastSpace = true;
        }
        else { out += (char)c; lastSpace = false; }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

int BookMarkMenuHandler::chapterWindowCount(int start, int avail)
{
    int n = 0, used = 0;
    const int total = (int)chapterEntries.size();
    while (start + n < total)
    {
        int h = chapterLines[start + n] * CL_LINE_H + CL_ROW_PAD;
        if (used + h > avail) break;
        used += h;
        n++;
    }
    return n;
}

void BookMarkMenuHandler::openChapterList(int currentSpine)
{
    chapterEntries.clear();
    chapterLines.clear();

    int tocCount = epub->get_toc_items_count();
    if (tocCount > 0)
    {
        for (int i = 0; i < tocCount; i++)
        {
            ChapterEntry e;
            e.title = cleanTitle(epub->get_toc_item(i).title);
            if (e.title.empty()) e.title = "(untitled)";
            e.spine = epub->get_spine_index_for_toc_index(i);
            chapterEntries.push_back(e);
        }
    }
    else
    {
        // book without a table of contents: list its sections instead
        int spineCount = epub->get_spine_items_count();
        for (int i = 0; i < spineCount; i++)
        {
            ChapterEntry e;
            e.title = "Section " + std::to_string(i + 1);
            e.spine = i;
            chapterEntries.push_back(e);
        }
    }

    const int textWidth = EPD_HEIGHT - CL_TEXT_X - CL_MARGIN;
    for (auto &e : chapterEntries)
    {
        int lines = (int)renderer->wrapText(e.title, textWidth, false, 1, CL_MAX_LINES).size();
        chapterLines.push_back((uint8_t)std::max(1, std::min(CL_MAX_LINES, lines)));
    }

    // the chapter we are in: first entry of the current section, else the last entry before it
    int exact = -1, before = -1;
    for (int i = 0; i < (int)chapterEntries.size(); i++)
    {
        if (chapterEntries[i].spine == currentSpine && exact < 0) exact = i;
        if (chapterEntries[i].spine < currentSpine) before = i;
    }
    chapterSel = (exact >= 0) ? exact : (before >= 0 ? before : 0);
    openedAt = chapterSel;
    chapterTop = std::max(0, chapterSel - 4);
    chapterUpTap.reset();
    chapterDownTap.reset();

    // keep the pages underneath so cancelling can restore them
    if (!preservedRightFramebuffer)
    {
        preservedRightFramebuffer = (unsigned char*)calloc(EPD_WIDTH * EPD_HEIGHT / 8, sizeof(unsigned char));
    }
    if (preservedRightFramebuffer) memcpy(preservedRightFramebuffer, rightPageFrameBuffer, EPD_WIDTH * EPD_HEIGHT / 8);

    chapterListOpen = true;
    drawChapterList(true);
}

void BookMarkMenuHandler::closeChapterList()
{
    chapterListOpen = false;
    if (preservedRightFramebuffer) memcpy(rightPageFrameBuffer, preservedRightFramebuffer, EPD_WIDTH * EPD_HEIGHT / 8);
    chapterEntries.clear();
    chapterLines.clear();
    currentVerticalElementIndex = 0;
    renderer->epd.forceRefresh();
    drawMenu(); // draws the bar over the restored left page
}

void BookMarkMenuHandler::scrollChapters(int dir)
{
    const int total = (int)chapterEntries.size();
    if (total == 0) return;

    DoubleTapTracker &tap = (dir < 0) ? chapterUpTap : chapterDownTap;
    const gpio_num_t button = (dir < 0) ? ARROW_UP_BUTTON : ARROW_DOWN_BUTTON;
    Device::TapKind kind = Device::getInstance().classifyTap(button, tap, dir);

    const int oldTop = chapterTop;
    const int oldSel = chapterSel;
    int target;
    if (kind == Device::TapKind::Double)
    {
        target = std::max(0, std::min(total - 1, tap.origin + dir * chapterVisible));
        chapterTop = std::max(0, std::min(total - 1, oldTop + dir * chapterVisible));
    }
    else
    {
        if (kind == Device::TapKind::Single) tap.origin = oldSel;
        target = oldSel + dir;
    }
    if (target < 0 || target >= total) return;
    chapterSel = target;

    // make sure the selection is on screen; the list flips like pages when it leaves the screen
    const int availLeft = CL_LEFT_TOP - CL_BOTTOM;
    const int availRight = CL_RIGHT_TOP - CL_BOTTOM;
    auto visibleFrom = [&](int top) {
        int left = chapterWindowCount(top, availLeft);
        return left + chapterWindowCount(top + left, availRight);
    };
    if (chapterSel < chapterTop)
    {
        // smallest top that still shows the selection => it ends up at the bottom of the right column
        int t = std::max(0, chapterSel - 80);
        while (t < chapterSel && t + visibleFrom(t) <= chapterSel) t++;
        chapterTop = t;
    }
    else if (chapterSel >= chapterTop + std::max(1, visibleFrom(chapterTop)))
    {
        chapterTop = chapterSel;
    }

    if (chapterTop == oldTop && chapterSel == oldSel) return;
    drawChapterList(chapterTop != oldTop);
}

void BookMarkMenuHandler::drawChapterList(bool fullRefresh)
{
    Device &dev = Device::getInstance();
    dev.clearButtonLatches();
    dev.setLatchTimeOut(100000);

    const int PW = EPD_HEIGHT; // page width in the drawing coordinates
    const int total = (int)chapterEntries.size();
    const int textWidth = PW - CL_TEXT_X - CL_MARGIN;

    int leftCount = chapterWindowCount(chapterTop, CL_LEFT_TOP - CL_BOTTOM);
    int rightCount = chapterWindowCount(chapterTop + leftCount, CL_RIGHT_TOP - CL_BOTTOM);
    chapterVisible = std::max(1, leftCount + rightCount);

    auto drawColumn = [&](unsigned char *buffer, int first, int count, int topY) {
        renderer->framebuffer = buffer;
        int rowTop = topY;
        for (int i = first; i < first + count; i++)
        {
            const bool selected = (i == chapterSel);
            const int lineCount = chapterLines[i];
            const int h = lineCount * CL_LINE_H + CL_ROW_PAD;
            const int rowX = CL_MARGIN - 4;
            const int rowW = PW - 2 * rowX;
            const int rowY = rowTop - h;
            if (selected) {
                renderer->fillRounded(rowX, rowY, rowW, h, 6, true);
            } else {
                // Light dividers make long tables easier to scan without making every
                // entry a full card (important for books with hundreds of chapters).
                renderer->fillDither(CL_TEXT_X, rowY, PW - CL_TEXT_X - CL_MARGIN, 1, true);
            }
            std::vector<std::string> lines = renderer->wrapText(chapterEntries[i].title, textWidth, false, 1, CL_MAX_LINES);
            for (int k = 0; k < (int)lines.size() && k < lineCount; k++)
            {
                renderer->drawString(CL_TEXT_X, rowTop - CL_LINE_H - k * CL_LINE_H, lines[k], 1, false, false, !selected);
            }
            if (i == openedAt) renderer->fillRounded(CL_MARGIN, rowTop - h / 2 - 3, 6, 6, 1, !selected); // "you are here" marker
            rowTop -= h;
        }
    };

    // Left screen: section label, title, selected position, then the first column.
    renderer->framebuffer = leftPageFrameBuffer;
    renderer->clearScreenBuffer();
    renderer->drawString(CL_MARGIN + 4, 626, "CONTENTS", 1, false, false, true);
    renderer->drawString(CL_MARGIN + 4, 586, "Chapters", 2, true, false, true);
    std::string position = std::to_string(chapterSel + 1) + " / " + std::to_string(total);
    const int positionW = renderer->measureText(position, true) + 20;
    const int positionX = PW - CL_MARGIN - positionW;
    renderer->drawPill(positionX, 590, positionW, 28, false);
    renderer->drawString(positionX + 10, 597, position, 1, true, false, true);
    renderer->drawSquare(CL_MARGIN, 575, PW - 2 * CL_MARGIN, 2, false);
    renderer->drawSquare(CL_MARGIN, 572, 72, 6, false);
    drawColumn(leftPageFrameBuffer, chapterTop, leftCount, CL_LEFT_TOP);
    renderer->framebuffer = leftPageFrameBuffer;
    renderer->drawPill(CL_MARGIN, 12, 224, 28, false);
    renderer->drawString(CL_MARGIN + 12, 19, "Up/Down: move · Center: open", 1, true, false, true);

    // Right screen: repeat the header so the continuation reads as one designed spread.
    renderer->framebuffer = rightPageFrameBuffer;
    renderer->clearScreenBuffer();
    renderer->drawString(CL_MARGIN + 4, 626, "CONTENTS · CONTINUED", 1, false, false, true);
    renderer->drawString(CL_MARGIN + 4, 586, "Chapters", 2, true, false, true);
    renderer->drawSquare(CL_MARGIN, 575, PW - 2 * CL_MARGIN, 2, false);
    renderer->drawSquare(CL_MARGIN, 572, 72, 6, false);
    drawColumn(rightPageFrameBuffer, chapterTop + leftCount, rightCount, CL_RIGHT_TOP);
    renderer->framebuffer = rightPageFrameBuffer;
    renderer->drawPill(CL_MARGIN, 12, 250, 28, false);
    renderer->drawString(CL_MARGIN + 12, 19, "Left page: back · Right page: open · Double-tap: page", 1, true, false, true);

    if (fullRefresh) renderer->epd.forceRefresh();
    else renderer->epd.partialUpdatesRemaining[true] = 8;
    renderer->epd.DisplayPictureBoth(leftPageFrameBuffer, rightPageFrameBuffer);
}
