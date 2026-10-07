#include "UIElements.h"
#include "device.h"
#include "brand.h"
#include "brandLogo.h"

static std::string upperCopy(std::string s)
{
    for (auto &c : s) c = (char)toupper((unsigned char)c);
    return s;
}

// Header of the main menu: big wordmark, a dithered band and the book that is being read
static void drawMainHeader(Renderer *renderer)
{
    // "modern reader" wordmark with the version tucked in at the right
    renderer->drawBitmap1bpp(CARD_X + 2, 590, MR_LOGO_W, MR_LOGO_H, MR_LOGO_BITS, true);
    const std::string ver = std::string("v") + MR_VERSION;
    renderer->drawString(CARD_X + CARD_W - (int)ver.size() * 8 - 2, 590, ver, 1, false, false, true);
    renderer->fillDither(CARD_X, 580, CARD_W, 8, true);
    renderer->drawSquare(CARD_X, 578, CARD_W, 2, false);
}

// The "now reading" card: shows the most recently opened book and opens the recent books menu
void RecentMenuElement::renderIcon(int y, bool highlight)
{
    (void)y;
    const int cy = 488, h = 80;
    const bool ink = !highlight; // text colour: black on a white card, white on the selected card
    renderer->fillRoundedDither(CARD_X + 4, cy - 4, CARD_W, h, 8, true);
    renderer->drawCard(CARD_X, cy, CARD_W, h, highlight);

    Book *book = currentBook;

    const int tx = CARD_X + 12, ty = cy + 10, tw = 40, th = 60;
    renderer->fillRounded(tx, ty, tw, th, 5, ink);
    renderer->drawSquare(tx + 7, ty + 5, 2, th - 10, ink); // same flag convention as drawTile
    if (book) renderer->drawString(tx + 15, ty + 14, monogramFor(book->title), 2, true, false, highlight);
    else renderer->drawCharacter(tx + 17, ty + 22, '?', true, false, 1, highlight);

    const int textX = tx + tw + 14;
    const int avail = CARD_X + CARD_W - 14 - textX;
    renderer->drawString(textX, cy + 56, "NOW READING", 1, false, false, ink);
    renderer->drawSquare(textX, cy + 52, 8 * 11, 1, highlight);
    if (book) {
        renderer->drawString(textX, cy + 32, renderer->fitText(book->title, avail - 24, true), 1, true, false, ink);
        int total = book->totalPageCount;
        int percent = (total > 1) ? (100 * book->currentPage) / (total - 1) : 0;
        percent = std::max(0, std::min(100, percent));
        std::string label = (total > 0) ? std::to_string(percent) + "%" : "?%";
        int labelWidth = renderer->measureText(label, false);
        renderer->drawString(CARD_X + CARD_W - 14 - labelWidth, cy + 9, label, 1, false, false, ink);
        renderer->drawMeter(textX, cy + 10, avail - labelWidth - 10, 12, (total > 0) ? percent : 0, ink);
    } else {
        renderer->drawString(textX, cy + 32, "No book open yet", 1, true, false, ink);
        renderer->drawString(textX, cy + 10, "Pick one from the Library", 1, false, false, ink);
    }
    renderer->drawChevron(CARD_X + CARD_W - 24, cy + 34, ink);
}

// Header of the other menus: small parent name above a big title, with an accent rule
static void drawSubHeader(Renderer *renderer, const std::string &title, const std::string &parentName)
{
    std::string crumb = parentName.empty() ? std::string("MODERN READER") : upperCopy(parentName);
    renderer->drawString(CARD_X + 4, 626, renderer->fitText(crumb, CARD_W - 8, false), 1, false, false, true);
    renderer->drawString(CARD_X + 4, 586, renderer->fitText(title, (CARD_W - 8) / 2, true), 2, true, false, true);
    renderer->drawSquare(CARD_X, MENU_RULE_Y + 1, CARD_W, 2, false);
    renderer->drawSquare(CARD_X, MENU_RULE_Y - 2, 72, 6, false);
}

// Bottom of every menu: back button, page dots and a select hint
// favState: -1 = no hint on the right (menus), 0 = a book is selected, 1 = a book is selected and it is a favorite
static void drawFooter(Renderer *renderer, bool showBack, int page, int pages, int favState)
{
    if (showBack) {
        renderer->drawPill(CARD_X, 12, 80, 28, false);
        renderer->drawString(CARD_X + 12, 18, "< Back", 1, true, false, true);
    }
    // the right button toggles favorite on a book; in menus it has no special meaning, so nothing is shown
    if (favState >= 0) {
        const bool fav = (favState == 1);
        const int pw = 76;
        const int px = CARD_X + CARD_W - pw;
        renderer->drawPill(px, 12, pw, 28, fav);
        renderer->drawCharacter(px + 12, 18, 10084, true, false, 1, !fav);
        renderer->drawString(px + 12 + 18, 18, "Fav", 1, true, false, !fav);
    }

    if (pages > 1) {
        const int spacing = 20;
        int startX = (EPD_HEIGHT - (pages - 1) * spacing) / 2;
        for (int i = 0; i < pages; i++) {
            if (i == page) renderer->fillDisc(startX + i * spacing, 26, 6, true);
            else {
                renderer->fillDisc(startX + i * spacing, 26, 5, true);
                renderer->fillDisc(startX + i * spacing, 26, 3, false);
            }
        }
    }
}

void MenuElement::renderElement()
{
    renderer->clearScreenBuffer();

    const bool isMain = (elementName == "Modern Reader");
    const int listTop = isMain ? MAIN_CARD_TOP_Y : CARD_TOP_Y;

    if (isMain) {
        drawMainHeader(renderer);
    } else {
        auto parentElement = getParent();
        drawSubHeader(renderer, elementName, parentElement ? parentElement->elementName : std::string());
    }

    const int maxVisible = MaxElementsPerPage; 
    int currentPage = selectedChildIndex / maxVisible;
    int startIndex = currentPage * maxVisible;
    int endIndex = std::min(startIndex + maxVisible, (int)children.size());

    // On the main menu the first child may be the big "now reading" card, drawn above the list
    int firstListChild = startIndex;
    if (isMain && currentPage == 0 && !children.empty() && children[0]->isHeroCard()) {
        children[0]->renderIcon(0, selectedChildIndex == 0);
        firstListChild = 1;
    }

    // Draw visible children as cards
    for (int i = firstListChild; i < endIndex; i++) {
        int localIndex = i - firstListChild; // position within page
        int y = listTop - localIndex * CARD_PITCH;
        children[i]->renderIcon(y, selectedChildIndex == i);
    }

    // Scrollbar on the right when the list is longer than a page
    int pages = ((int)children.size() + maxVisible - 1) / maxVisible;
    if ((int)children.size() > maxVisible) {
        renderer->drawScrollbar(SCROLLBAR_X, listTop + CARD_H, listTop - (maxVisible - 1) * CARD_PITCH,
                                selectedChildIndex, (int)children.size(), maxVisible);
    }
    int favState = -1;
    if (selectedChildIndex >= 0 && selectedChildIndex < (int)children.size() && children[selectedChildIndex]->getType() == UIElementType::Book) {
        auto bookElement = std::static_pointer_cast<BookElement>(children[selectedChildIndex]);
        favState = (bookElement->book && bookElement->book->favorite) ? 1 : 0;
    }
    drawFooter(renderer, !isMain, currentPage, pages, favState);
}

// Icon / monogram tile on the left side of a card
void UIElement::drawTile(int y, bool highlight)
{
    const int tx = CARD_X + 10, ty = y + (CARD_H - TILE_SIZE) / 2;
    const bool tileInk = !highlight;  // black tile on a white card, white tile on the selected (black) card
    renderer->fillRounded(tx, ty, TILE_SIZE, TILE_SIZE, 8, tileInk);
    if (iconId != IconNone) {
        renderer->drawIcon(iconId, tx + 8, ty + 8, !tileInk);
    } else if (!tileLetter.empty()) {
        int letterX = tx + 14;
        if (coverTile) {
            renderer->drawSquare(tx + 8, ty + 6, 2, TILE_SIZE - 12, tileInk); // spine line
            letterX += 4;
        }
        renderer->drawString(letterX, ty + 6, tileLetter, 2, true, false, highlight);
    }
}

// Card background + tile + title (+ description). titleReserve = pixels on the right of the title kept free.
int UIElement::drawCardBase(int y, bool highlight, int titleReserve, bool showDescription)
{
    const bool ink = !highlight; // text is black on a white card, white on a selected (black) card
    renderer->drawCard(CARD_X, y, CARD_W, CARD_H, highlight);
    int textX = CARD_X + CARD_PAD;
    if (hasTile()) {
        drawTile(y, highlight);
        textX = CARD_X + TILE_TEXT_X;
    }
    const int avail = CARD_X + CARD_W - CARD_PAD - textX;
    std::string title = renderer->fitText(elementName, avail - titleReserve, true);
    renderer->drawString(textX, y + 36, title, 1, true, false, ink);
    if (showDescription) {
        std::string desc = renderer->fitText(elementDescription, avail, false);
        renderer->drawString(textX, y + 10, desc, 1, false, false, ink);
    }
    return textX;
}

void UIElement::renderIcon(int y, bool highlight)
{
    drawCardBase(y, highlight, 28);
    renderer->drawChevron(CARD_X + CARD_W - CARD_PAD - 10, y + 22, !highlight);
}

void MenuElement::renderIcon(int y, bool highlight)
{
    drawCardBase(y, highlight, 28);
    renderer->drawChevron(CARD_X + CARD_W - CARD_PAD - 10, y + 22, !highlight);
}

void BookElement::renderIcon(int y, bool highlight)
{
    const bool ink = !highlight;
    int textX = drawCardBase(y, highlight, 28, false); // title only, the second line shows progress
    if (book->favorite) {
        renderer->drawCharacter(CARD_X + CARD_W - CARD_PAD - 16, y + 36, 10084, true, false, 1, ink);
    }

    // reading progress: a meter plus the percentage / page numbers
    int total = book->totalPageCount;
    int percent = (total > 1) ? (100 * book->currentPage) / (total - 1) : 0;
    percent = std::max(0, std::min(100, percent));
    std::string label;
    if (Device::getInstance().deviceSettings.showPagePercentage) label = (total > 0) ? std::to_string(percent) + "%" : "?%";
    else label = std::to_string(book->currentPage + 1) + "/" + (total > 0 ? std::to_string(total) : "?");
    this->elementDescription = label;

    int labelWidth = renderer->measureText(label, false);
    renderer->drawString(CARD_X + CARD_W - CARD_PAD - labelWidth, y + 10, label, 1, false, false, ink);
    int meterWidth = CARD_X + CARD_W - CARD_PAD - labelWidth - 12 - textX;
    renderer->drawMeter(textX, y + 12, meterWidth, 12, (total > 0) ? percent : 0, ink);
}




void MenuElement::enterElement()
{
    
}
void ValueElement::renderIcon(int y, bool highlight)
{
    const bool ink = !highlight;
    const int pillH = 24;
    const int pillRight = CARD_X + CARD_W - 12;
    const int maxPillWidth = maxStringWidth * 9 + 16; // widest value (incl. the < > arrows) + padding

    int textX = drawCardBase(y, highlight, maxPillWidth + 8);
    (void)textX;

    std::string valueString = this->valueDescriptions[selectedValueIndex];
    if(selected)
    {
        if(selectedValueIndex > 0 || infinitescrolling==true) valueString = "< " + valueString;
        if(selectedValueIndex<values.size()-1  || infinitescrolling==true) valueString = valueString + " >";
    }
    int textWidth = renderer->measureText(valueString, true);
    int pillWidth = textWidth + 16;
    int pillX = pillRight - pillWidth;
    int pillY = y + 32;

    if (selected) {
        // editing: white pill with black text on the selected card
        renderer->fillRounded(pillX, pillY, pillWidth, pillH, 8, false);
        renderer->drawString(pillX + 8, y + 36, valueString, 1, true, false, true);
    } else {
        // outlined pill
        renderer->fillRounded(pillX, pillY, pillWidth, pillH, 8, ink);
        renderer->fillRounded(pillX + 2, pillY + 2, pillWidth - 4, pillH - 4, 6, highlight);
        renderer->drawString(pillX + 8, y + 36, valueString, 1, true, false, ink);
    }

    // area that is refreshed with partial updates while a value is being edited
    drawValueXPos = pillRight - maxPillWidth;
    drawValueYPos = pillY;
    drawValueWidth = maxPillWidth;
    drawValueHeight = pillH;
}

void ValueElement::writeValue()
{
    if(!this->values.empty()) *(this->valueAdress) = this->values[this->selectedValueIndex];
}

void ValueElement::ReadValue()
{
    for(int i=0;i<values.size();i++)
    {
        if(*(this->valueAdress) == this->values[i]) this->selectedValueIndex = i;
    }
}
