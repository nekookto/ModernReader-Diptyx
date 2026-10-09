#include "usbTransfer.h"

#include "device.h"
#include "usbMassStorage.h"
#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "USB_TRANSFER";

void runUSBFileTransfer()
{
    Device &device = Device::getInstance();
    if (device.reader && device.reader->book && device.bookHandler)
        device.bookHandler->saveBook(device.reader->book);

    usb_console_teardown();
    usb_msc_set_stage("sd unmount");
    delete device.sd;
    device.sd = nullptr;
    vTaskDelay(pdMS_TO_TICKS(10));

    esp_err_t err = usb_msc_sdmmc_start(GPIO_NUM_41, GPIO_NUM_40, GPIO_NUM_39, 1);
    if (err != ESP_OK)
    {
        char detail[72];
        snprintf(detail, sizeof(detail), "%s failed (0x%x)", usb_msc_last_failure_stage(), (unsigned)usb_msc_last_failure_code());
        device.notificationHandler->drawUSBTransferStatus("USB setup failed", detail, false);
        vTaskDelay(pdMS_TO_TICKS(4000));
        esp_restart();
        return;
    }

    bool cableWasConnected = gpio_get_level(GPIO_NUM_16);
    bool safeToUnplug = false;
    int waitingWithoutCableMs = 0;
    if (cableWasConnected)
    {
        device.notificationHandler->drawUSBTransferStatus(
            "USB connected", "Transfer files, then safely eject Modern Reader on your computer.", false);
    }
    else
    {
        device.notificationHandler->drawUSBTransferStatus(
            "Waiting for USB", "Connect the reader to your computer to transfer files.", false);
    }

    while (true)
    {
        const bool cableConnected = gpio_get_level(GPIO_NUM_16);
        if (cableConnected)
        {
            waitingWithoutCableMs = 0;
            if (!cableWasConnected)
            {
                cableWasConnected = true;
                device.notificationHandler->drawUSBTransferStatus(
                    "USB connected", "Transfer files, then safely eject Modern Reader on your computer.", false);
            }
            if (!safeToUnplug && !usb_msc_in_use_by_host())
            {
                safeToUnplug = true;
                device.notificationHandler->drawUSBTransferStatus(
                    "Safe eject received", "Your computer has finished with the card. You can unplug the cable.", true);
            }
        }
        else if (cableWasConnected)
        {
            // Check once more on disconnect in case eject and cable removal happened between polls.
            if (!safeToUnplug && !usb_msc_in_use_by_host()) safeToUnplug = true;
            break;
        }
        else
        {
            waitingWithoutCableMs += 100;
            if (waitingWithoutCableMs >= 10 * 60 * 1000) break;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (cableWasConnected)
    {
        if (safeToUnplug)
            device.notificationHandler->drawUSBTransferStatus(
                "USB unplugged safely", "The computer ejected the card before disconnecting.", true);
        else
            device.notificationHandler->drawUSBTransferStatus(
                "USB removed early", "Reconnect and safely eject the card to reduce the risk of file damage.", false);
    }
    else
    {
        device.notificationHandler->drawUSBTransferStatus(
            "No USB connection", "Transfer mode timed out and will now restart.", false);
    }
    vTaskDelay(pdMS_TO_TICKS(1800));

    ESP_LOGI(TAG, "Stopping USB mass storage");
    usb_msc_stop();
    esp_restart();
}
