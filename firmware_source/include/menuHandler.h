#pragma once
#include "doubleTap.h"
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
#include "UIElements.h"
#include "menuPanel.h"

class MenuHandler
{
    public:
        MenuHandler(Renderer *renderer);

        void middleButtonAction();
        void rightButtonAction();
        void leftButtonAction();
        void upButtonAction();
        void downButtonAction();
        void rightPageAction();
        void leftPageAction();

        void scrollSelection(int dir); // dir: -1 = up, +1 = down. A double tap scrolls a full page
        void openBook(Book *book, const std::string &groupName); // start reading a book from a menu
        void toggleFavorite(Book *book);
        void refreshRecentMenu(); // rebuild the "Recent books" menu + the now reading card

        void layoutReadMenu(std::vector<Author>& authorList);
        void layoutFontSelect();
        void updateFontSize();
        void updateFontSelect();
        void updateFont();
        
        PanelInfo buildPanelInfo(); // what to show on the right page
        void drawMenu();
        void drawValuePartial();
        bool buzzDisabled = false;
        std::function<void()> displayIdleCallbackReturn;

        Renderer *renderer;
        std::shared_ptr<UIElement> currentElement;
        std::shared_ptr<MenuElement> mainMenu;
        std::shared_ptr<MenuElement> settingsMenu;
        std::shared_ptr<MenuElement> deviceSettingsMenu;
        std::shared_ptr<MenuElement> readSettingsMenu;
        std::shared_ptr<MenuElement> einkSettingsMenu;
        std::shared_ptr<MenuElement> authorMenu; 
        std::shared_ptr<RecentMenuElement> recentMenu; // the "now reading" card / recent books list
        DoubleTapTracker upTap, downTap;
        std::shared_ptr<ValueElement> fontSelectBox; 
        std::shared_ptr<ValueElement> fontSizeBox; 

        unsigned char* leftPageFrameBuffer;
        unsigned char* rightPageFrameBuffer;
    private:

};