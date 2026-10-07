#pragma once
#include <string>
#include <vector>
#include "renderer.h"

// Content of the info panel that is drawn on the right page while a menu is open on the left page
struct PanelInfo {
    enum class Kind { Home, Group, BookDetail, Help };
    Kind kind = Kind::Help;

    std::string title;        // heading (book title, group name, setting name)
    std::string subtitle;     // author / folder / small label
    std::string body;         // description text
    std::string series;       // book: series name (with volume number)
    std::string pages;        // book: "208 / 430"
    int percent = -1;         // reading progress 0..100, -1 when unknown
    bool favorite = false;
    std::vector<std::string> list;   // group: titles of the books
    int moreCount = 0;               // group: books that did not fit in the list

    // home panel
    bool hasCurrentBook = false;
    int bookCount = 0;
    int favoriteCount = 0;
    int groupCount = 0;
    // (the favorites shown on the home panel use `list` / `moreCount`)
};

void drawMenuPanel(Renderer &r, const PanelInfo &info);
