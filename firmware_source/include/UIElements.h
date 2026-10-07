#pragma once
#include <string>
#include <list>
#include <vector>
#include <string.h>
#include "htmlParser.h"
#include <Epub.h>
#include <dirent.h>
#include "esp_log.h"
#include "renderer.h"
#include <map>
#include <memory>
#include <functional>
#include "bookHandler.h"

#define MaxElementsPerPage 6

// menu layout (logical coordinates: x left->right, y bottom->top)
#define CARD_X 20        // left edge of the cards
#define CARD_W 432       // card width
#define CARD_H 60        // card height
#define CARD_PAD 16      // inner padding of a card
#define CARD_PITCH 80    // distance between two cards
#define CARD_TOP_Y 506   // y of the lowest edge of the first card
#define MAIN_CARD_TOP_Y 412 // same for the main menu, which has a bigger header
#define MENU_RULE_Y 574  // y of the line below the title
#define TILE_SIZE 44     // icon / monogram tile on the left of a card
#define TILE_TEXT_X 66   // x offset (from the card edge) of the text when there is a tile
#define SCROLLBAR_X 462  // x of the scrollbar

// icons for the tiles (see Renderer::drawIcon)
enum UIIcon { IconNone = 0, IconBook = 1, IconHelp = 2, IconGear = 3, IconTransfer = 4, IconInfo = 5, IconHeart = 6, IconSliders = 7, IconDisplay = 8, IconText = 9 };

// first letter of a title, for the monogram tiles
inline std::string monogramFor(const std::string &name)
{
    std::string n = name;
    size_t start = 0;
    while (start < n.size() && !isalnum((unsigned char)n[start]) && (unsigned char)n[start] < 0x80) start++;
    n = n.substr(start);
    auto lower = [](std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; };
    for (const char *article : {"the ", "a ", "an "}) {
        std::string a(article);
        if (n.size() > a.size() && lower(n.substr(0, a.size())) == a) { n = n.substr(a.size()); break; }
    }
    if (!n.empty() && isalpha((unsigned char)n[0])) return std::string(1, (char)toupper((unsigned char)n[0]));
    if (!n.empty() && isdigit((unsigned char)n[0])) return std::string(1, n[0]);
    return "#";
}

enum class UIElementType {
    Menu,
    Value,
    Book,
    Author,
    Action,
    Unknown
};

class UIElement : public std::enable_shared_from_this<UIElement> {
public:
    // UIElement(Renderer* renderer,std::string name = "", std::string desc = "")
    //     : renderer(renderer), elementName(std::move(name)), elementDescription(std::move(desc)) {}

    UIElement(Renderer* renderer,std::string name = "", std::string desc = "",std::string extraDesc="")
: renderer(renderer), elementName(std::move(name)), elementDescription(std::move(desc)), elementExtraDescription(std::move(extraDesc)) {}

    void setParent(std::shared_ptr<UIElement> p) { parent = p; }
    std::shared_ptr<UIElement> getParent() const { return parent.lock(); }

    Renderer *renderer;

    //virtual void enterElement() = 0;

    //virtual void renderElement() = 0;
    virtual UIElementType getType() const = 0;
    void renderElement();
    virtual void renderIcon(int y,bool Highlight);
    virtual bool isHeroCard() const { return false; } // big card drawn in the main menu header area

    // draws the card background, the tile, the title and the description line. Returns the x where text starts.
    int drawCardBase(int y, bool highlight, int titleReserve = 0, bool showDescription = true);
    void drawTile(int y, bool highlight);
    bool hasTile() const { return iconId != IconNone || !tileLetter.empty(); }

    int iconId = IconNone;       // icon shown on the tile
    std::string tileLetter;      // or a letter (monogram)
    bool coverTile = false;      // draw the tile like a book cover

    std::string elementName;
    std::string elementDescription;
    std::string elementExtraDescription;

protected:
    std::weak_ptr<UIElement> parent; // weak to avoid cyclic ownership
};


class MenuElement : public UIElement {

    public:
    using UIElement::UIElement;
    void addChild(std::shared_ptr<UIElement> child) {
        child->setParent(shared_from_this());
        children.push_back(child);
    }

    void renderElement();
    void renderIcon(int y,bool Highlight) override;
    void enterElement();
    UIElementType getType() const override { return UIElementType::Menu; }

    std::vector<std::shared_ptr<UIElement>> children;
    int selectedChildIndex = 0;
    private:

};

// The "Now reading" card on the main menu. Selecting it opens a menu with the recently opened books.
class RecentMenuElement : public MenuElement {
    public:
    using MenuElement::MenuElement;
    void renderIcon(int y, bool highlight) override;
    bool isHeroCard() const override { return true; }
    Book *currentBook = nullptr; // most recently opened book (nullptr when nothing was opened yet)
};

class ValueElement : public UIElement {

    public:
    ValueElement(Renderer* renderer,std::string name, std::string desc, std::vector<int> vals,std::vector<std::string> valDescriptions, int *valueAdress)
        : UIElement(renderer,std::move(name), std::move(desc)), values(std::move(vals)),valueDescriptions(std::move(valDescriptions)),valueAdress(valueAdress) {}
    UIElementType getType() const override { return UIElementType::Value; }

    bool selected = false;
    void renderIcon(int y,bool Highlight) override;
    void writeValue();
    void ReadValue();
    int selectedValueIndex = 0;
    std::vector<int> values;
    std::vector<std::string> valueDescriptions;
    int *valueAdress;
    bool infinitescrolling = false;
    int maxStringWidth = std::max_element(valueDescriptions.begin(), valueDescriptions.end(),
                     [](const std::string& a, const std::string& b) {
                         return a.size() < b.size();
                     })->size() + 4; //width of the longest string +4
    int drawValueXPos = 0;
    int drawValueYPos = 0;
    int drawValueWidth = 0;   // size of the area that changes while editing the value
    int drawValueHeight = 0; //these are determined dynamically, but we need these for drawing the values with partial updates..

    private:

};

class BookElement : public UIElement {

    public:
    BookElement(Renderer* renderer, Book* book)
        : UIElement(renderer,
                    book->title,
                    std::to_string(book->currentPage+1) + "/" +
                    (book->totalPageCount > 0 ? std::to_string(book->totalPageCount) : "?"), (book->favorite?std::string("❤"):std::string(""))),
        book(book) { tileLetter = monogramFor(book->title); coverTile = true; }

    Book *book;
    UIElementType getType() const override { return UIElementType::Book; }
    void renderIcon(int y,bool Highlight) override;
    //std::string elementName = book.title;
    //std::string elementDescription = book.author;
    //std::string elementDescription = std::to_string(book.readPageCount) + "/" + std::to_string(book.totalPageCount);

    private:

};

class AuthorElement : public MenuElement {
public:
    AuthorElement(Renderer* renderer, Author* author)
        : MenuElement(renderer,
                      author->name,
                      "Books: " + std::to_string(author->bookList.size())),
          author(author)
    {
        if (author->name == "Favorite books") iconId = IconHeart;
        else tileLetter = monogramFor(author->name);
    }

    void initChildren() {
        children.clear();
        for (Book* book : author->bookList) {
            auto bookElement = std::make_shared<BookElement>(renderer, book);
            addChild(bookElement);
        }
    }

    UIElementType getType() const override { return UIElementType::Author; }
    Author* author;
};


class ActionElement : public UIElement {

public:
    using ActionCallback = std::function<void()>;

    ActionElement(Renderer* renderer,
                  std::string name,
                  std::string desc,
                  ActionCallback callback,
                  std::string extraDesc = "")
        : UIElement(renderer,
                    std::move(name),
                    std::move(desc),
                    std::move(extraDesc)),
          action(std::move(callback))
    {}

    UIElementType getType() const override {
        return UIElementType::Action;
    }


    void trigger() {
        ESP_LOGI("UIElements", "Action called");
        if (action) {
            action();
        }
    }

private:
    ActionCallback action;
};