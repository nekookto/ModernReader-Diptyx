#include "notificationHandler.h"
#include "menuHandler.h"
#include "device.h"


NotificationHandler::NotificationHandler(Renderer *renderer)
{
    this->renderer = renderer;
}

void NotificationHandler::drawUSBQuery()
{
    renderer->framebuffer = Device::getInstance().menuHandler->leftPageFrameBuffer;
    renderer->clearScreenBuffer();

    Device::usbState selectionState = Device::getInstance().usb_state;

    int originY = 400;
    int originX = 2*GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,false);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,true);

    std::string displayString = std::string("Charge device or transfer files?");
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 1.5*GLYPH_HEIGHT,displayString,1,true,false,true);
    
    rectHeight = 2 * GLYPH_HEIGHT;
    rectWidth = EPD_HEIGHT/4;
    originX = 2*GLYPH_WIDTH;
    originY = originY-3 * GLYPH_HEIGHT;
    bool chargingSelected = (selectionState!=Device::usbState::Charging);
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,!chargingSelected);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,chargingSelected);
    displayString = std::string("Charge");
    renderer->drawString(originX + rectWidth/2 - displayString.length()*GLYPH_WIDTH/4, originY + 0.5*GLYPH_HEIGHT,displayString,1,true,false,chargingSelected);


    originX = EPD_HEIGHT - 2*GLYPH_WIDTH - rectWidth;
    bool transferSelected = (selectionState!=Device::usbState::FileTransfer);
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,!transferSelected);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,transferSelected);
    displayString = std::string("Transfer");
    renderer->drawString(originX + rectWidth/2 - displayString.length()*GLYPH_WIDTH/4, originY + 0.5*GLYPH_HEIGHT,displayString,1,true,false,transferSelected);
    
    if(transferSelected && chargingSelected) renderer->epd.forceRefresh();
    renderer->epd.DisplayPicture(true,renderer->framebuffer);
    if(!transferSelected || !chargingSelected) renderer->epd.forceRefresh();
}

void NotificationHandler::drawUSBTransferStatus(const std::string &title, const std::string &detail, bool safeToUnplug)
{
    renderer->framebuffer = Device::getInstance().reader->leftPageFrameBuffer;
    renderer->clearScreenBuffer();

    const int originX = 2 * GLYPH_WIDTH;
    const int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX, 342, rectWidth, 156, false);
    renderer->drawSquare(originX + padding, 342 + padding, rectWidth - 2 * padding, 156 - 2 * padding, true);

    int titleX = EPD_HEIGHT / 2 - renderer->measureText(title, true) / 2;
    renderer->drawString(titleX, 454, title, 1, true, false, true);
    std::vector<std::string> lines = renderer->wrapText(detail, rectWidth - 36, false, 1, 3);
    for (int i = 0; i < (int)lines.size(); i++)
    {
        int lineX = EPD_HEIGHT / 2 - renderer->measureText(lines[i], false) / 2;
        renderer->drawString(lineX, 414 - i * GLYPH_HEIGHT, lines[i], 1, false, false, true);
    }

    const std::string status = safeToUnplug ? "SAFE TO UNPLUG" : "WAIT FOR SAFE EJECT";
    const int statusWidth = renderer->measureText(status, true) + 28;
    const int statusX = (EPD_HEIGHT - statusWidth) / 2;
    renderer->drawPill(statusX, 286, statusWidth, 32, safeToUnplug);
    renderer->drawString(statusX + 14, 294, status, 1, true, false, !safeToUnplug);

    renderer->epd.forceRefresh();
    renderer->epd.DisplayPicture(true,renderer->framebuffer);
}


void NotificationHandler::drawIndexingNotification(std::string bookTitle)
{
    renderer->framebuffer = Device::getInstance().menuHandler->leftPageFrameBuffer;
    renderer->clearScreenBuffer();
    int originY = 400;
    int originX = 2*GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,false);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,true);

    std::string displayString = std::string("Loading book: ") + bookTitle;
    if (displayString.length() > 50) {
        displayString = displayString.substr(0, 50);
    }
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 1.5*GLYPH_HEIGHT,displayString,1,true,false,true);
    renderer->epd.DisplayPictureBoth(renderer->framebuffer,Device::getInstance().menuHandler->rightPageFrameBuffer);
}

void NotificationHandler::drawIndexingNotification(std::string bookTitle, int percent)
{
    renderer->framebuffer = Device::getInstance().menuHandler->leftPageFrameBuffer;
    renderer->clearScreenBuffer();
    int originY = 400;
    int originX = 2*GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,false);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,true);

    std::string displayString = std::string("Indexing book: ") + bookTitle;
    if (displayString.length() > 50) {
        displayString = displayString.substr(0, 50);
    }
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 1.5*GLYPH_HEIGHT,displayString,1,true,false,true);
    renderer->drawProgressBar(originY-GLYPH_HEIGHT-4,51,percent);
    renderer->epd.DisplayPicture(true,renderer->framebuffer);
}

void NotificationHandler::drawBookOpeningNotification(std::string bookTitle)
{
    renderer->framebuffer = Device::getInstance().menuHandler->leftPageFrameBuffer;
    renderer->clearScreenBuffer();
    int originY = 400;
    int originX = 2*GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,false);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,true);

    std::string displayString = std::string("Opening book: ") + bookTitle;
    if (displayString.length() > 50) {
        displayString = displayString.substr(0, 50);
    }
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 1.5*GLYPH_HEIGHT,displayString,1,true,false,true);
    renderer->epd.DisplayPicture(true,renderer->framebuffer);
}


void NotificationHandler::drawErrorNotification(std::string bookTitle)
{
    renderer->framebuffer = Device::getInstance().menuHandler->leftPageFrameBuffer;
    renderer->clearScreenBuffer();
    int originY = 400;
    int originX = 2*GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,false);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,true);

    std::string displayString = std::string("Error rendering book: ") + bookTitle;
    if (displayString.length() > 50) {
        displayString = displayString.substr(0, 50);
    }
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 2.5*GLYPH_HEIGHT,displayString,1,true,false,true);
    displayString = std::string("Please report this problem");
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 1.5*GLYPH_HEIGHT,displayString,1,false,false,true);
    renderer->epd.DisplayPicture(true,renderer->framebuffer);
}

void NotificationHandler::drawNotification(std::string notificationText)
{
    renderer->framebuffer = Device::getInstance().reader->leftPageFrameBuffer;
    renderer->clearScreenBuffer();
    int originY = 400;
    int originX = 2*GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,false);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,true);

    std::string displayString = notificationText;
    if (displayString.length() > 50) {
        displayString = displayString.substr(0, 50);
    }
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 2.5*GLYPH_HEIGHT,displayString,1,true,false,true);
    renderer->epd.DisplayPicture(true,renderer->framebuffer);
}

void NotificationHandler::drawSDcardErrorNotification()
{
    renderer->framebuffer = Device::getInstance().reader->leftPageFrameBuffer;
    renderer->clearScreenBuffer();
    int originY = 400;
    int originX = 2*GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,false);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,true);

    std::string displayString = "Error mounting SD card, shutting down :( ";
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 2.5*GLYPH_HEIGHT,displayString,1,true,false,true);
    displayString = std::string("Please refer to the manual");
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 1.5*GLYPH_HEIGHT,displayString,1,false,false,true);
    renderer->epd.DisplayPicture(true,renderer->framebuffer);
}

void NotificationHandler::drawStorageAccessNotication()
{
    renderer->framebuffer = Device::getInstance().reader->leftPageFrameBuffer;
    renderer->clearScreenBuffer();
    int originY = 400;
    int originX = 2*GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,false);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,true);

    std::string displayString = "Debug storage access";
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 2.5*GLYPH_HEIGHT,displayString,1,true,false,true);
    displayString = std::string("Unplug USB cable to restart device");
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 1.5*GLYPH_HEIGHT,displayString,1,false,false,true);
    renderer->epd.DisplayPicture(true,renderer->framebuffer);
}
