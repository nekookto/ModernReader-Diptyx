#include "menuHandler.h"
#include "htmlParser.h"
#include "pageShowcase.h"
#include "device.h"
#include "esp_log.h"
#include "usbMassStorage.h"
#include "brand.h"
#include "driver/adc.h"
#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include "tinyusb_console.h"
#include "tinyusb_default_config.h"

PageShowcase pageShowcase;

MenuHandler::MenuHandler(Renderer *renderer)
{
    this->renderer = renderer;
    this->leftPageFrameBuffer = (unsigned char*)calloc(EPD_WIDTH * EPD_HEIGHT / 8,sizeof(unsigned char));
    this->rightPageFrameBuffer = (unsigned char*)calloc(EPD_WIDTH * EPD_HEIGHT / 8,sizeof(unsigned char));
    mainMenu = std::make_shared<MenuElement>(renderer, std::string("Modern Reader"),std::string(""));
    authorMenu = std::make_shared<MenuElement>(renderer, std::string("Library"),std::string("Open & read books"));
    recentMenu = std::make_shared<RecentMenuElement>(renderer, std::string("Recent books"),std::string("Most recently opened first"));
    settingsMenu = std::make_shared<MenuElement>(renderer, std::string("Settings"),std::string("Edit main settings, render settings, etc."));
    deviceSettingsMenu = std::make_shared<MenuElement>(renderer, std::string("Device settings"),std::string("Edit device behaviour"));
    readSettingsMenu = std::make_shared<MenuElement>(renderer, std::string("Book settings"),std::string("Edit the reading experience"));
    einkSettingsMenu = std::make_shared<MenuElement>(renderer, std::string("E-ink settings"),std::string("Edit E-ink display behaviour"));


    auto horizontalMarginBox = std::make_shared<ValueElement>(renderer, std::string("Horizontal margin"),std::string("Distance between text & display edge"),
    std::vector<int> {0,1,2,3,4,5}, std::vector<std::string> {std::string("0em"),std::string("1em"),std::string("2em"),std::string("3em"),std::string("4em"),std::string("5em")},&(Device::getInstance().renderSettings.marginsHorizontal) );

    auto verticalMarginBox = std::make_shared<ValueElement>(renderer, std::string("Vertical margin"),std::string("Distance between text & display edge"),
    std::vector<int> {0,1,2,3,4,5}, std::vector<std::string> {std::string("0em"),std::string("1em"),std::string("2em"),std::string("3em"),std::string("4em"),std::string("5em")},&(Device::getInstance().renderSettings.marginsVertical));

    auto batteryVoltageBox = std::make_shared<ValueElement>(renderer, std::string("Battery indicator"),std::string("Show or hide battery indicator"),
    std::vector<int> {0,1}, std::vector<std::string> {std::string("Hidden"),std::string("Shown")},&(Device::getInstance().deviceSettings.displayBattery));

    auto lineSpaceBox = std::make_shared<ValueElement>(renderer, std::string("Line spacing"),std::string("additional spacing between lines"),
    std::vector<int> {0,1,2,3,4}, std::vector<std::string> {std::string("0Px"),std::string("1Px"),std::string("2Px"),std::string("3Px"),std::string("4Px")},&(Device::getInstance().renderSettings.lineSpacing));

    auto textBoldBox = std::make_shared<ValueElement>(renderer, std::string("Default font weight"),std::string("Default boldness of text in books"),
    std::vector<int> {0,1}, std::vector<std::string> {std::string("Auto"),std::string("Bold")},&(Device::getInstance().renderSettings.fontBold));

    auto showPagePercentageBox = std::make_shared<ValueElement>(renderer, std::string("Reading progress percentage/pages"),std::string("Show current page as a number or percentage"),
    std::vector<int> {1,0}, std::vector<std::string> {std::string("Percentage"),std::string("Number")},&(Device::getInstance().deviceSettings.showPagePercentage));

    auto displayRefreshBox = std::make_shared<ValueElement>(renderer, std::string("Full refresh interval"),std::string("Fully refresh displays after x pages"),
    std::vector<int> {0,1,2,3,4,5}, std::vector<std::string> {std::string("0"),std::string("1"),std::string("2"),std::string("3"),std::string("4"),std::string("5")},&(Device::getInstance().deviceSettings.displayRefresh));

    auto buzzerBox = std::make_shared<ValueElement>(renderer, std::string("Haptic buzzer"),std::string("Enable or disable buzzer"),
    std::vector<int> {1,0}, std::vector<std::string> {std::string("Enable"),std::string("Disable")},&(Device::getInstance().deviceSettings.buzzerEnabled));

    auto buzzerIntensityBox = std::make_shared<ValueElement>(renderer, std::string("Buzzer intensity"),std::string("Strength of buzzes"),
    std::vector<int> {1,2,3,4,5,6,7,8,9}, std::vector<std::string> {std::string("1"),std::string("2"),std::string("3"),std::string("4"),std::string("5"),std::string("6"),std::string("7"),std::string("8"),std::string("9")},&(Device::getInstance().deviceSettings.buzzerIntensity));

    auto nightModeBox = std::make_shared<ValueElement>(renderer, std::string("Dark mode"),std::string("Invert all colors"),
    std::vector<int> {1,0}, std::vector<std::string> {std::string("Enable"),std::string("Disable")},&(Device::getInstance().deviceSettings.nightMode));

    auto libraryGroupingBox = std::make_shared<ValueElement>(renderer, std::string("Library grouping"),std::string("Group by author, title/series or folder"),
    std::vector<int> {0,1,2}, std::vector<std::string> {std::string("Author"),std::string("Title/series"),std::string("Folder")},&(Device::getInstance().deviceSettings.libraryGrouping));

    auto sunlightModeBox = std::make_shared<ValueElement>(renderer, std::string("Sunlight mode"),std::string("Reduce artefacts in sunlight, increases latency"),
    std::vector<int> {1,0}, std::vector<std::string> {std::string("Enable"),std::string("Disable")},&(Device::getInstance().deviceSettings.sunlightMode));

    auto standbyTimeoutBox = std::make_shared<ValueElement>(renderer, std::string("Standby timeout"),std::string("Time in minutes before entering deep sleep"),
    std::vector<int> {2,5,10,20}, std::vector<std::string> {std::string("2"),std::string("5"),std::string("10"),std::string("20")},&(Device::getInstance().deviceSettings.standbyTimeout));

    auto standbyScreenBox = std::make_shared<ValueElement>(renderer, std::string("Standby screen"),std::string("What to display during deep sleep"),
    std::vector<int> {0,1,2}, std::vector<std::string> {std::string("None"),std::string("Current book"),std::string("Custom")},&(Device::getInstance().deviceSettings.standbyScreen));

    auto storeDataOnSDBox = std::make_shared<ValueElement>(renderer, std::string("Data storage location"),std::string("Location to store book data files"),
    std::vector<int> {0,1}, std::vector<std::string> {std::string("Internal"),std::string("SD")},&(Device::getInstance().deviceSettings.storeDataOnSD));

    auto smartImageDetectBox = std::make_shared<ValueElement>(renderer, std::string("Smart image updates"),std::string("Limit full screen updates to large images"),
    std::vector<int> {0,1}, std::vector<std::string> {std::string("disabled"),std::string("enabled")},&(Device::getInstance().deviceSettings.smartImageDetect));

     auto sunlightFullRefreshBox = std::make_shared<ValueElement>(renderer, std::string("Sunlight mode full refresh"),std::string("Force full refresh in sunlight mode"),
    std::vector<int> {0,1}, std::vector<std::string> {std::string("disabled"),std::string("enabled")},&(Device::getInstance().deviceSettings.sunlightFullRefresh));

    auto standbyShutdownBox = std::make_shared<ValueElement>(renderer, std::string("Shutdown timer"),std::string("Shutdown after long standby"),
    std::vector<int> {0,1,2,3,7,14,21,28}, std::vector<std::string> {std::string("disabled"),std::string("1 day"),std::string("2 days"),std::string("3 days"),std::string("1 week"),std::string("2 weeks"),std::string("3 weeks"),std::string("4 weeks")},&(Device::getInstance().deviceSettings.standbyShutdown));

    
    //generate a list for the vcom voltages
    std::vector<int> vcomValues = {}; 
    std::vector<std::string> vcomNames = {};
    for(int i=0;i<80;i++)
    {
        vcomValues.push_back(i);
        std::string valueName = std::to_string(10000 + i*5 + 10) + "V";
        valueName[0] = 45;
        valueName[1] = valueName[2];
        valueName[2] = 46;
        vcomNames.push_back(valueName);
    }

    auto vcomLeftBox = std::make_shared<ValueElement>(renderer, std::string("Vcom voltage left"),std::string("Don't edit, refer to the web-docs"),
    vcomValues, vcomNames,&(Device::getInstance().deviceSettings.vcomLeft));

    auto vcomRightBox = std::make_shared<ValueElement>(renderer, std::string("Vcom voltage right"),std::string("Don't edit, refer to the web-docs"),
    vcomValues, vcomNames,&(Device::getInstance().deviceSettings.vcomRight));

    fontSizeBox = std::make_shared<ValueElement>(renderer, std::string("Font size"),std::string("Size of the text in books"),
    std::vector<int> {1}, std::vector<std::string> {"placeholder"},&(Device::getInstance().renderSettings.fontPoints));

    auto manualButton = std::make_shared<ActionElement>(
    renderer,
    "Manual",
    "Read the Modern Reader manual",
    []() {
        Device::getInstance().state = Device::State::simpleReader;
        Device::getInstance().activeBookPath = "userManual.epub";
        Device::getInstance().simpleReader->init("userManual.epub",Device::getInstance().renderer);
        Device::getInstance().saveAppState();
    }
);

    auto versionButton = std::make_shared<ActionElement>(
    renderer,
    std::string("Firmware info"),
    std::string("Modern Reader ") + MR_VERSION + " - credits & changes",
    []() {
        // opens the firmware info (fork notice, credits and the change log) in the built-in reader
        Device::getInstance().state = Device::State::simpleReader;
        Device::getInstance().activeBookPath = "firmwareVersion.epub";
        Device::getInstance().simpleReader->init("firmwareVersion.epub",Device::getInstance().renderer);
        Device::getInstance().saveAppState();
    }
);

    auto restoreBookSettingsButton = std::make_shared<ActionElement>(
    renderer,
    "Restore book settings",
    "Restore the default book settings",
    []() {
        Device::getInstance().renderSettings.fontBold = false;
        Device::getInstance().renderSettings.lineSpacing = 0;
        Device::getInstance().renderSettings.marginsVertical = 0;
        Device::getInstance().renderSettings.marginsHorizontal = 1;
        Device::getInstance().renderSettings.fontPoints = 16;
        std::string fontName = "Espy Sans";
        Device::getInstance().renderSettings.fontFamily  = 0;
        for(int i=0;i<Device::getInstance().renderer->fontHandler.families.size();i++)
        {
            if(fontName==Device::getInstance().renderer->fontHandler.families[i].name) Device::getInstance().renderSettings.fontFamily = i;

        }
        Device::getInstance().saveSettings();
        
        for(int i=0;i<Device::getInstance().menuHandler->readSettingsMenu->children.size();i++) //and update the values in the settings
        {
        if(Device::getInstance().menuHandler->readSettingsMenu->children[i]->getType()==UIElementType::Value)
        {
            std::static_pointer_cast<ValueElement>(Device::getInstance().menuHandler->readSettingsMenu->children[i])->ReadValue();
        }
    }
        Device::getInstance().menuHandler->drawMenu();
    }
);

auto fileTransferButton = std::make_shared<ActionElement>(
    renderer,
    "Transfer files",
    "Enter USB mass storage mode",
    []() {
        //stop usb cdc and unmount sd card
        usb_console_teardown();
        usb_msc_set_stage("sd unmount");
        delete(Device::getInstance().sd);
        vTaskDelay(10);
        

        if (usb_msc_sdmmc_start(GPIO_NUM_41, GPIO_NUM_40, GPIO_NUM_39, 1) == ESP_OK) {
            Device::getInstance().notificationHandler->drawManualFileTransfer(false);
            int timeOutTimer = 0;
            // Wait until USB cable disconnected (GPIO low)
            while(gpio_get_level(PAGE_RIGHT_BUTTON)) {

                if(!gpio_get_level(GPIO_NUM_16))
                {
                    timeOutTimer += 100;
                }
                if(timeOutTimer > 1000 * 60 * 10) //if the device is in filetransfer for longer than 10 minutes without a USB connection, we restart
                {
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(100));
                
            }
            Device::getInstance().notificationHandler->drawManualFileTransfer(true);
            usb_msc_stop();  // stop MSC

            
        } else {
            char msg[48];
            snprintf(msg, sizeof(msg), "USB storage failed: %s 0x%x", usb_msc_last_failure_stage(), (unsigned)usb_msc_last_failure_code());
            Device::getInstance().notificationHandler->drawNotification(msg);
            vTaskDelay(pdMS_TO_TICKS(4000));
        }

        vTaskDelay(10);
        esp_restart(); //restart the device to init everything properly
    }
);


    // icons for the tiles
    authorMenu->iconId = IconBook;
    manualButton->iconId = IconHelp;
    versionButton->iconId = IconInfo;
    settingsMenu->iconId = IconGear;
    fileTransferButton->iconId = IconTransfer;
    readSettingsMenu->iconId = IconText;
    deviceSettingsMenu->iconId = IconSliders;
    einkSettingsMenu->iconId = IconDisplay;

    currentElement = authorMenu;
    mainMenu->addChild(recentMenu); // the "now reading" card, shown above the other entries
    mainMenu->addChild(authorMenu);
    mainMenu->addChild(settingsMenu);
    mainMenu->addChild(fileTransferButton);
    settingsMenu->addChild(readSettingsMenu);
    settingsMenu->addChild(deviceSettingsMenu);
    settingsMenu->addChild(einkSettingsMenu);
    settingsMenu->addChild(manualButton);
    settingsMenu->addChild(versionButton);
    layoutFontSelect();
    updateFontSize();
    readSettingsMenu->addChild(fontSizeBox);
    readSettingsMenu->addChild(lineSpaceBox);
    readSettingsMenu->addChild(horizontalMarginBox);
    readSettingsMenu->addChild(verticalMarginBox);
    deviceSettingsMenu->addChild(libraryGroupingBox);
    deviceSettingsMenu->addChild(batteryVoltageBox);
    readSettingsMenu->addChild(textBoldBox);
    readSettingsMenu->addChild(showPagePercentageBox);
    readSettingsMenu->addChild(restoreBookSettingsButton);
    deviceSettingsMenu->addChild(buzzerBox);
    deviceSettingsMenu->addChild(buzzerIntensityBox);
    deviceSettingsMenu->addChild(nightModeBox);
    deviceSettingsMenu->addChild(sunlightModeBox);
    deviceSettingsMenu->addChild(standbyTimeoutBox);
    deviceSettingsMenu->addChild(standbyScreenBox);
    deviceSettingsMenu->addChild(standbyShutdownBox);
    deviceSettingsMenu->addChild(storeDataOnSDBox);

    einkSettingsMenu->addChild(displayRefreshBox);
    einkSettingsMenu->addChild(smartImageDetectBox);
    einkSettingsMenu->addChild(sunlightFullRefreshBox);
    einkSettingsMenu->addChild(vcomLeftBox);
    einkSettingsMenu->addChild(vcomRightBox);
    //drawMenu();
}

void MenuHandler::upButtonAction()
{
    scrollSelection(-1);
}

void MenuHandler::downButtonAction()
{
    scrollSelection(1);
}

// Move the selection one entry (single press / held button) or one full page (double tap)
void MenuHandler::scrollSelection(int dir)
{
    auto menu = std::static_pointer_cast<MenuElement>(currentElement);
    if (!menu || menu->children.empty()) return;

    Device &dev = Device::getInstance();
    DoubleTapTracker &tap = (dir < 0) ? upTap : downTap;
    const gpio_num_t button = (dir < 0) ? ARROW_UP_BUTTON : ARROW_DOWN_BUTTON;
    const int count = (int)menu->children.size();
    const int current = menu->selectedChildIndex;

    Device::TapKind kind = dev.classifyTap(button, tap, dir);
    int target;
    if (kind == Device::TapKind::Double)
    {
        // the first tap of the double tap already moved one entry: jump from where we started instead
        target = std::max(0, std::min(count - 1, tap.origin + dir * MaxElementsPerPage));
    }
    else
    {
        if (kind == Device::TapKind::Single) tap.origin = current;
        target = current + dir;
    }
    if (target < 0 || target >= count || target == current) return;

    if (menu->children[current]->getType() == UIElementType::Value)
    {
        std::static_pointer_cast<ValueElement>(menu->children[current])->selected = false;
        std::static_pointer_cast<ValueElement>(menu->children[current])->ReadValue();
    }
    if ((target / MaxElementsPerPage) != (current / MaxElementsPerPage)) renderer->epd.forceRefresh();
    else renderer->epd.partialUpdatesRemaining[1] = 8;
    menu->selectedChildIndex = target;
    drawMenu();
}

void MenuHandler::toggleFavorite(Book *book)
{
    book->favorite = !book->favorite;
    Device::getInstance().bookHandler->saveBook(book); //store the updated book
    if(!authorMenu->selectedChildIndex==0) Device::getInstance().bookHandler->refreshFavorites(); //update the favorites vector
    auto favoriteListElement = std::static_pointer_cast<AuthorElement>(authorMenu->children[0]);
    favoriteListElement->initChildren(); //and update the UI element
    favoriteListElement->elementDescription = "Books: " + std::to_string(favoriteListElement->author->bookList.size());
    if(favoriteListElement->selectedChildIndex>=favoriteListElement->children.size()) favoriteListElement->selectedChildIndex = favoriteListElement->children.size()-1;
    drawMenu();
}

void MenuHandler::openBook(Book *book, const std::string &groupName)
{
    if(book->badParse)
    {
        Device::getInstance().notificationHandler->drawErrorNotification(book->title);
        vTaskDelay(500);
        drawMenu();
        return;
    }
    if(!book->matchesRenderSettings(book->getCurrentRenderSettings()))
    {
        Device::getInstance().notificationHandler->drawIndexingNotification(book->title,0);
        Device::getInstance().bookHandler->reindexBook(book);
    }
    Device::getInstance().notificationHandler->drawBookOpeningNotification(book->title);
    Reader *reader = Device::getInstance().reader;
    reader->init(book,renderer);

    Device::getInstance().bookHandler->markOpened(book); // moves it to the top of the recent books
    refreshRecentMenu();

    renderer->epd.forceRefresh();
    Device::getInstance().state=Device::State::Reading;
    Device::getInstance().activeBookPath = book->path;
    Device::getInstance().activeAuthorName = groupName;
    Device::getInstance().saveAppState();
    reader->openPage();
}

void MenuHandler::refreshRecentMenu()
{
    recentMenu->children.clear();
    recentMenu->currentBook = nullptr;
    recentMenu->selectedChildIndex = 0;
    if (!Device::getInstance().bookHandler) return;
    std::vector<Book*> recent = Device::getInstance().bookHandler->getRecentBooks(2 * MaxElementsPerPage); // two pages
    for (Book *b : recent)
    {
        recentMenu->addChild(std::make_shared<BookElement>(renderer, b));
        if (!recentMenu->currentBook && !b->badParse) recentMenu->currentBook = b;
    }
}

void MenuHandler::middleButtonAction()
{
    upTap.reset();
    downTap.reset();
    if(currentElement->getType()==UIElementType::Menu || currentElement->getType()==UIElementType::Author)
    {    
        auto menu = std::static_pointer_cast<MenuElement>(currentElement);
        if(menu->children.size() > 0)
        {
            if(menu->children[menu->selectedChildIndex]->getType()==UIElementType::Menu ||
                menu->children[menu->selectedChildIndex]->getType()==UIElementType::Author)
            {        
                if(menu->children[menu->selectedChildIndex] == recentMenu && recentMenu->children.empty())
                {
                    Device::getInstance().notificationHandler->drawNotification("No books opened yet");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    renderer->epd.forceRefresh();
                    drawMenu();
                    return;
                }
                this->currentElement = menu->children[menu->selectedChildIndex];
                renderer->epd.forceRefresh();
                drawMenu();
            }
            else if(menu->children[menu->selectedChildIndex]->getType()==UIElementType::Action)
            {        
                auto actionElement = std::static_pointer_cast<ActionElement>(menu->children[menu->selectedChildIndex]);
                renderer->epd.forceRefresh();
                actionElement->trigger();
            }
            else if(menu->children[menu->selectedChildIndex]->getType()==UIElementType::Book)
            {
                // middle button = enter: open the book
                auto bookElement = std::static_pointer_cast<BookElement>(menu->children[menu->selectedChildIndex]);
                std::string groupName = (menu == recentMenu) ? Device::getInstance().bookHandler->groupNameForBook(bookElement->book) : menu->elementName;
                openBook(bookElement->book, groupName);
            }
            else if(menu->children[menu->selectedChildIndex]->getType()==UIElementType::Value)
            {
                auto valueElement = std::static_pointer_cast<ValueElement>(menu->children[menu->selectedChildIndex]);
                if(valueElement->selected) 
                {
                    valueElement->writeValue();
                    if(valueElement->valueAdress==&(Device::getInstance().deviceSettings.storeDataOnSD))
                    {
                        if(Device::getInstance().deviceSettings.storeDataOnSD) Device::getInstance().bookHandler->transferDataToSD();
                        else Device::getInstance().bookHandler->transferDataToFlash();
                    }
                    if(valueElement->valueAdress==&(Device::getInstance().deviceSettings.libraryGrouping))
                    {
                        Device::getInstance().bookHandler->regroupLibrary(); //rebuild the library menu with the new grouping
                    }
                    updateFontSize();
                    updateFont();
                    Device::getInstance().saveSettings();
                    renderer->epd.forceRefresh();
                }
                valueElement->selected = !valueElement->selected;
                drawMenu();
            }
        }
    }
}

void MenuHandler::rightButtonAction()
{
    upTap.reset();
    downTap.reset();
    auto menu = std::static_pointer_cast<MenuElement>(currentElement);
    if(menu->selectedChildIndex>=menu->children.size()) return;
    if(menu->children[menu->selectedChildIndex]->getType()==UIElementType::Value &&
    std::static_pointer_cast<ValueElement>(menu->children[menu->selectedChildIndex])->selected)
    {
        auto valueElement = std::static_pointer_cast<ValueElement>(menu->children[menu->selectedChildIndex]);
        if(valueElement->selectedValueIndex<valueElement->values.size()-1 || valueElement->infinitescrolling==true) valueElement->selectedValueIndex++;
        if(valueElement->selectedValueIndex>valueElement->values.size()-1) valueElement->selectedValueIndex-= valueElement->values.size();
        drawValuePartial();
    }
    else if(menu->children[menu->selectedChildIndex]->getType()==UIElementType::Book)
    {
        // right button on a book = add / remove favorite (the middle button opens it)
        toggleFavorite(std::static_pointer_cast<BookElement>(menu->children[menu->selectedChildIndex])->book);
    }
    else middleButtonAction();
}

void MenuHandler::rightPageAction()
{
    rightButtonAction();
}

void MenuHandler::leftPageAction()
{
    leftButtonAction();
}

void MenuHandler::leftButtonAction()
{
    upTap.reset();
    downTap.reset();
    if (auto menu = std::static_pointer_cast<MenuElement>(currentElement)) {
        if(menu->children.size()>0 && menu->children[menu->selectedChildIndex]->getType()==UIElementType::Value &&
        std::static_pointer_cast<ValueElement>(menu->children[menu->selectedChildIndex])->selected)
        {
            //if(!this->buzzDisabled) Device::buzz();
            auto valueElement = std::static_pointer_cast<ValueElement>(menu->children[menu->selectedChildIndex]);
            if(valueElement->selectedValueIndex>0 || valueElement->infinitescrolling==true) valueElement->selectedValueIndex--;
            if(valueElement->selectedValueIndex<0) valueElement->selectedValueIndex += valueElement->values.size();
            drawValuePartial();
            //if(!this->buzzDisabled) Device::buzz();
        }
        else if (auto parent = menu->getParent()) {
            // parent is always a MenuElement
            auto parentMenu = std::static_pointer_cast<MenuElement>(parent);

            // Find this menu inside parent's children
            for (int i = 0; i < parentMenu->children.size(); i++) {
                if (parentMenu->children[i] == currentElement) {
                    parentMenu->selectedChildIndex = i;
                    break;
                }
            }

            //if(!this->buzzDisabled) Device::buzz();
            this->currentElement = parent;
            Device::getInstance().bookHandler->refreshFavorites();
            renderer->epd.forceRefresh();
            drawMenu();
            //if(!this->buzzDisabled) Device::buzz();
        }
    }
}

void MenuHandler::layoutReadMenu(std::vector<Author>& authorList)
{
    authorMenu->children.clear(); //clear all the books
    for(int i=0;i<authorList.size();i++) //fill in the authors and the books
    {
        auto authorElement = std::make_shared<AuthorElement>(renderer,&authorList[i]);
        authorElement->initChildren();
        this->authorMenu->addChild(authorElement);
    }
    for(int i=0;i<readSettingsMenu->children.size();i++) //and update the values in the settings
    {
        if(readSettingsMenu->children[i]->getType()==UIElementType::Value)
        {
            std::static_pointer_cast<ValueElement>(readSettingsMenu->children[i])->ReadValue();
        }
    }
    for(int i=0;i<deviceSettingsMenu->children.size();i++) //and update the values in the settings
    {
        std::static_pointer_cast<ValueElement>(deviceSettingsMenu->children[i])->ReadValue();
    }
    for(int i=0;i<einkSettingsMenu->children.size();i++) //and update the values in the settings
    {
        std::static_pointer_cast<ValueElement>(einkSettingsMenu->children[i])->ReadValue();
    }
    refreshRecentMenu();
}

void MenuHandler::layoutFontSelect()
{
    //vTaskDelay(1500);
    std::vector<std::string> fontNames;
    std::vector<int> fontIDs;
    for(int i=0;i<renderer->fontHandler.families.size();i++)
    {
         printf("fontFam: %s", renderer->fontHandler.families[i].name.c_str());
          printf("fontFam: %d", i);
        fontNames.push_back(renderer->fontHandler.families[i].name);
        fontIDs.push_back(i);
    }
    fontSelectBox = std::make_shared<ValueElement>(renderer, std::string("Active font"),std::string("Font used for text in books"),fontIDs ,fontNames,&(Device::getInstance().renderSettings.fontFamily));
    fontSelectBox->infinitescrolling = true;
    readSettingsMenu->addChild(fontSelectBox);
}

void MenuHandler::updateFontSelect()
{
    for(int i=0;i<fontSelectBox->valueDescriptions.size();i++)
    {
        if(fontSelectBox->valueDescriptions[i]==renderer->fontHandler.currentFont.family) fontSelectBox->selectedValueIndex=i;

    }
}

void MenuHandler::updateFontSize()
{
    std::string currentFontName = fontSelectBox->valueDescriptions[fontSelectBox->selectedValueIndex];
    int currentFontSize = Device::getInstance().renderSettings.fontPoints;
    fontSizeBox->selectedValueIndex = 0;
    std::vector<int> availableFontSizes;
    for(int i=0;i<renderer->fontHandler.families.size();i++)
    {
        if(renderer->fontHandler.families[i].name==currentFontName)
        {
            for(int j=0;j<renderer->fontHandler.families[i].fonts.size();j++)
            {
                availableFontSizes.push_back(renderer->fontHandler.families[i].fonts[j].pointSize);
                if(renderer->fontHandler.families[i].fonts[j].pointSize==currentFontSize) fontSizeBox->selectedValueIndex = j;
            }
        }
    }

    fontSizeBox->values.clear();
    fontSizeBox->valueDescriptions.clear();

    for(int i=0;i<availableFontSizes.size();i++)
    {
        fontSizeBox->values.push_back(availableFontSizes[i]);
        fontSizeBox->valueDescriptions.push_back(std::to_string(availableFontSizes[i]) + "Px");
    }

}

void MenuHandler::updateFont()
{
    std::string fontName = fontSelectBox->valueDescriptions[fontSelectBox->selectedValueIndex];
    int fontSize = fontSizeBox->values[fontSizeBox->selectedValueIndex];
    std::string fontFileName = "";

    for(int i=0;i<renderer->fontHandler.families.size();i++)
    {
        if(renderer->fontHandler.families[i].name==fontName)
        {
            for(int j=0;j<renderer->fontHandler.families[i].fonts.size();j++)
            {
                if(renderer->fontHandler.families[i].fonts[j].pointSize==fontSize) fontFileName = renderer->fontHandler.families[i].fonts[j].fileName;
            }
        }
    }
    if(fontFileName.length()>0) 
    {
        renderer->fontHandler.loadFont(fontFileName);
        Device::getInstance().renderSettings.fontPoints = fontSize;
    }
}

// Collects what the right page should show for the currently selected menu entry
PanelInfo MenuHandler::buildPanelInfo()
{
    PanelInfo info;
    Device &dev = Device::getInstance();
    auto menu = std::static_pointer_cast<MenuElement>(currentElement);
    std::shared_ptr<UIElement> selected;
    if (menu && menu->selectedChildIndex >= 0 && menu->selectedChildIndex < (int)menu->children.size())
        selected = menu->children[menu->selectedChildIndex];

    if (currentElement == mainMenu)
    {
        // overview of the library
        info.kind = PanelInfo::Kind::Home;
        if (dev.bookHandler)
        {
            info.bookCount = (int)dev.bookHandler->bookList.size();
            if (!dev.bookHandler->authorList.empty())
            {
                info.groupCount = (int)dev.bookHandler->authorList.size() - 1; // the first entry is the favorites
                const std::vector<Book*> &favorites = dev.bookHandler->authorList[0].bookList;
                info.favoriteCount = (int)favorites.size();
                for (size_t i = 0; i < favorites.size() && i < 8; i++) info.list.push_back(favorites[i]->title);
                info.moreCount = std::max(0, (int)favorites.size() - 8);
            }
        }
    }
    else if ((currentElement->getType() == UIElementType::Author || currentElement == recentMenu) && selected && selected->getType() == UIElementType::Book)
    {
        // details of the selected book
        info.kind = PanelInfo::Kind::BookDetail;
        Book *book = std::static_pointer_cast<BookElement>(selected)->book;
        info.title = book->title;
        if (!book->badParse && book->author != "unknown author") info.subtitle = book->author;
        if (!book->series.empty())
        {
            info.series = book->series;
            if (book->seriesIndex > 0)
            {
                char number[16];
                if (book->seriesIndex == (float)(int)book->seriesIndex) snprintf(number, sizeof(number), "%d", (int)book->seriesIndex);
                else snprintf(number, sizeof(number), "%.1f", book->seriesIndex);
                info.series += std::string(" #") + number;
            }
        }
        int total = book->totalPageCount;
        if (total > 0)
        {
            info.pages = std::to_string(book->currentPage + 1) + " / " + std::to_string(total);
            info.percent = (total > 1) ? std::max(0, std::min(100, (100 * book->currentPage) / (total - 1))) : 0;
        }
        info.favorite = book->favorite;
    }
    else if (currentElement == authorMenu && selected && selected->getType() == UIElementType::Author)
    {
        // contents of the selected group
        info.kind = PanelInfo::Kind::Group;
        Author *group = std::static_pointer_cast<AuthorElement>(selected)->author;
        info.title = group->name;
        int count = (int)group->bookList.size();
        info.subtitle = std::to_string(count) + (count == 1 ? " book" : " books");
        for (size_t i = 0; i < group->bookList.size() && i < 9; i++) info.list.push_back(group->bookList[i]->title);
        info.moreCount = std::max(0, count - 9);
    }
    else if (selected)
    {
        // description of the selected menu entry / setting
        info.kind = PanelInfo::Kind::Help;
        info.title = selected->elementName;
        info.body = selected->elementDescription;
    }
    return info;
}

void MenuHandler::drawMenu()
{ESP_LOGI("MenuHandler", "Draw menu called");
    Device::getInstance().clearButtonLatches();
    Device::getInstance().setLatchTimeOut(200000);
    //this->displayIdleCallbackReturn = nullptr;
    renderer->framebuffer = this->leftPageFrameBuffer;
    std::static_pointer_cast<MenuElement>(currentElement)->renderElement();

    //renderer->epd.DisplayPicture(true, renderer->framebuffer);

    renderer->clearScreenBuffer(rightPageFrameBuffer);
    if(currentElement==readSettingsMenu)
    {
        const char *html = pageShowcase.getPage();
        HtmlParser *parser = nullptr;
        parser = new HtmlParser(html, pageShowcase.size(), "",this->renderer,0,nullptr,rightPageFrameBuffer,Device::getInstance().reader->leftPageFrameBuffer); //dump the right page into the readers framebuffer
        parser->parse();
        delete parser;
    }
    else
    {
        renderer->framebuffer = rightPageFrameBuffer; //info panel: library overview, book details, group contents, setting help
        drawMenuPanel(*renderer, buildPanelInfo());
    }
    this->renderer->drawBattery(rightPageFrameBuffer,Device::getInstance().getBatteryPercentage());
    renderer->epd.DisplayPictureBoth(leftPageFrameBuffer,rightPageFrameBuffer);

    if(Device::getInstance().buttonLatchedStates[MIDDLE_BUTTON]) {middleButtonAction(); return;}
    if(Device::getInstance().buttonLatchedStates[PAGE_RIGHT_BUTTON]) {rightPageAction(); return;}
    if(Device::getInstance().buttonLatchedStates[PAGE_LEFT_BUTTON]) {leftPageAction(); return;}
    if(Device::getInstance().buttonLatchedStates[ARROW_DOWN_BUTTON]) {downButtonAction(); return;}
    if(Device::getInstance().buttonLatchedStates[ARROW_UP_BUTTON]) {upButtonAction(); return;}
    // a quick second tap can land inside the debounce window and never get latched: pick it up here
    if(Device::getInstance().tapPending(ARROW_DOWN_BUTTON, downTap)) {downButtonAction(); return;}
    if(Device::getInstance().tapPending(ARROW_UP_BUTTON, upTap)) {upButtonAction(); return;}
}

void MenuHandler::drawValuePartial()
{ESP_LOGI("MenuHandler", "Draw value called");
    Device::getInstance().clearButtonLatches();
    Device::getInstance().setLatchTimeOut(200000);
  
    renderer->framebuffer = this->leftPageFrameBuffer;
    std::static_pointer_cast<MenuElement>(currentElement)->renderElement();

    auto menu = std::static_pointer_cast<MenuElement>(currentElement);
    auto activeValueElement = std::static_pointer_cast<ValueElement>(menu->children[menu->selectedChildIndex]);

    int x = activeValueElement->drawValueXPos;
    int y = activeValueElement->drawValueYPos;
    int width = activeValueElement->drawValueWidth;
    int height = activeValueElement->drawValueHeight;


    renderer->epd.DisplayPicturePartial(true, leftPageFrameBuffer,
                                x,y,x+width,y+height);


    if(Device::getInstance().buttonLatchedStates[MIDDLE_BUTTON]) {middleButtonAction(); return;}
    if(Device::getInstance().buttonLatchedStates[PAGE_RIGHT_BUTTON]) {rightPageAction(); return;}
    if(Device::getInstance().buttonLatchedStates[PAGE_LEFT_BUTTON]) {leftPageAction(); return;}
    if(Device::getInstance().buttonLatchedStates[ARROW_DOWN_BUTTON]) {downButtonAction(); return;}
    if(Device::getInstance().buttonLatchedStates[ARROW_UP_BUTTON]) {upButtonAction(); return;}
    // a quick second tap can land inside the debounce window and never get latched: pick it up here
    if(Device::getInstance().tapPending(ARROW_DOWN_BUTTON, downTap)) {downButtonAction(); return;}
    if(Device::getInstance().tapPending(ARROW_UP_BUTTON, upTap)) {upButtonAction(); return;}
}