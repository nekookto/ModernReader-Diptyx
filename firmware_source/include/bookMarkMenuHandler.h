#pragma once
#include "doubleTap.h"
#include <string>
#include <list>
#include <vector>
#include <string.h>
#include <Epub.h>
#include <dirent.h>
#include "esp_log.h"
#include "renderer.h"
#include <map>
#include "bookHandler.h"
#include <memory>
#include <Epub.h>

class BookMarkMenuHandler
{
    public:
        BookMarkMenuHandler(Book *book, Renderer *renderer,Epub *epub,unsigned char* leftPageFrameBuffer,unsigned char* rightPageFrameBuffer);
        ~BookMarkMenuHandler();


        enum class MenuElementID {
            Return,
            ChapterList,
            GotoMark,
            Toggles,
            Library
        };

        struct MenuElement 
        {
            MenuElementID ID;
            std::string text;
        };

        std::vector<MenuElement> menuElements = {
            MenuElement(MenuElementID::Return,"Return to book"),
            MenuElement(MenuElementID::ChapterList,"Chapter List"),
            MenuElement(MenuElementID::GotoMark,"Go to bookmark"),
            MenuElement(MenuElementID::Toggles,"Toggles"),
            MenuElement(MenuElementID::Library,"Library"),
        };

        // rows of the "Toggles" popup, first row is drawn closest to the bar
        static const int TOGGLE_COUNT = 3;
        enum ToggleRow { ToggleDarkMode = 0, ToggleSunlight = 1, ToggleProgress = 2 };
        std::string toggleLabel(int row);

        int currentMenuElementIndex = 0;
        int currentVerticalElementIndex = 0;
        int maxVerticalElements = 0;
        bool bookMarkOnPage = false;
        bool forceFullDraw = false; // redraw both screens completely on the next drawMenu()

        // ---- full screen chapter list ----
        struct ChapterEntry
        {
            std::string title;
            int spine; // spine index the entry jumps to
        };
        std::vector<ChapterEntry> chapterEntries;
        std::vector<uint8_t> chapterLines; // number of text lines each entry needs (1 or 2)
        bool chapterListOpen = false;
        int chapterSel = 0;   // selected entry
        int openedAt = -1;    // entry of the chapter the reader was in when the list opened
        int chapterTop = 0;   // first entry shown (top of the left screen)
        int chapterVisible = 1; // entries shown on both screens
        DoubleTapTracker chapterUpTap, chapterDownTap;

        void openChapterList(int currentSpine);   // build the list and show it, highlighting the current chapter
        void closeChapterList();                  // back to the mini menu bar
        void scrollChapters(int dir);             // dir -1 / +1; a double tap moves a full screen
        void drawChapterList(bool fullRefresh);
        int chapterWindowCount(int start, int avail);
        int chapterSpine() const { return chapterEntries.empty() ? 0 : chapterEntries[chapterSel].spine; }

        void drawMenu();

        Renderer *renderer;
        Book *book;
        Epub *epub;
        int selectedBookMarkIndex = -1;

        unsigned char* preservedFramebuffer;
        unsigned char* preservedRightFramebuffer = nullptr; // right page, kept while the chapter list covers it
        unsigned char* leftPageFrameBuffer;
        unsigned char* rightPageFrameBuffer;
    private:

};