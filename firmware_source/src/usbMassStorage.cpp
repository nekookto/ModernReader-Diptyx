#include "usbMassStorage.h"

#include <stdlib.h>
#include <string.h>
#include "esp_attr.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "tinyusb_msc.h"
#include "tinyusb_cdc_acm.h"
#include "vfs_tinyusb.h"
#include "tinyusb_default_config.h"
#include "sdmmc_cmd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "USB_MSC_SDMMC";

// -------------------------- Static state --------------------------
static sdmmc_card_t* s_card = nullptr;
static sdmmc_host_t s_host;
static bool s_host_inited = false;
static bool s_tinyusb_inited = false;
static bool s_msc_driver_installed = false;
static tinyusb_msc_storage_handle_t s_msc_handle = nullptr;
static const char* s_fail_stage = "";
static esp_err_t s_fail_code = ESP_OK;

// Progress marker kept in RTC memory so that after a crash the next boot can tell how far the transfer mode got
#define MSC_STAGE_MAGIC 0xD1B7C0DEu
static RTC_NOINIT_ATTR uint32_t s_stage_magic;
static RTC_NOINIT_ATTR char s_stage_text[24];
void usb_msc_set_stage(const char* stage)
{
    strlcpy(s_stage_text, stage, sizeof(s_stage_text));
    s_stage_magic = MSC_STAGE_MAGIC;
}
const char* usb_msc_crash_stage(void)
{
    if (s_stage_magic != MSC_STAGE_MAGIC) return "";
    s_stage_text[sizeof(s_stage_text) - 1] = 0;
    return s_stage_text;
}
void usb_msc_clear_stage(void) { s_stage_magic = 0; }

static void msc_event_cb(tinyusb_msc_storage_handle_t, tinyusb_msc_event_t *event, void *)
{
    // runs in the TinyUSB task: only note what happened
    switch (event->id) {
        case TINYUSB_MSC_EVENT_MOUNT_START: usb_msc_set_stage("msc mount start"); break;
        case TINYUSB_MSC_EVENT_MOUNT_COMPLETE: usb_msc_set_stage("msc mount done"); break;
        case TINYUSB_MSC_EVENT_MOUNT_FAILED: usb_msc_set_stage("msc mount failed"); break;
        case TINYUSB_MSC_EVENT_FORMAT_REQUIRED: usb_msc_set_stage("msc format req"); break;
        case TINYUSB_MSC_EVENT_FORMAT_FAILED: usb_msc_set_stage("msc format fail"); break;
        default: break;
    }
}

const char* usb_msc_last_failure_stage(void) { return s_fail_stage; }
esp_err_t usb_msc_last_failure_code(void) { return s_fail_code; }

// -------------------------- TinyUSB descriptors -------------------
enum {
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};

enum {
    EDPT_MSC_OUT  = 0x01,
    EDPT_MSC_IN   = 0x81,
};

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

// Device descriptor
static tusb_desc_device_t s_device_desc = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x303A,
    .idProduct          = 0x4002,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

// Configuration descriptor
static const uint8_t s_fs_cfg_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, 64),
};

// Strings
static const char* s_string_desc[] = {
    (const char[]){0x09, 0x04}, // 0: English (0x0409)
    "Modern Reader",         // 1: Manufacturer
    "Modern Reader storage",         // 2: Product
    "123456",                   // 3: Serial
    "SD Card",                  // 4: MSC
};

// -------------------------- Internal helpers ----------------------
static esp_err_t init_sdmmc_card(gpio_num_t clk, gpio_num_t cmd, gpio_num_t d0, int bus_width, int max_freq_khz)
{
    if (bus_width != 1 && bus_width != 4) return ESP_ERR_INVALID_ARG;

    esp_err_t ret;
    bool host_init_called = false;
    s_card = nullptr;

    s_host = SDMMC_HOST_DEFAULT();
    s_host.max_freq_khz = max_freq_khz;
    if (bus_width == 1) s_host.flags = SDMMC_HOST_FLAG_1BIT; // same setup as the normal SD card mount

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = (bus_width == 4) ? 4 : 1;
    slot_config.clk = clk;
    slot_config.cmd = cmd;
    slot_config.d0  = d0;
    slot_config.d1  = GPIO_NUM_NC;
    slot_config.d2  = GPIO_NUM_NC;
    slot_config.d3  = GPIO_NUM_NC;
    slot_config.gpio_cd = GPIO_NUM_NC;
    slot_config.gpio_wp = GPIO_NUM_NC;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    s_card = (sdmmc_card_t*)malloc(sizeof(sdmmc_card_t));
    if (!s_card) return ESP_ERR_NO_MEM;

    ret = (*s_host.init)();
    if (ret != ESP_OK) { free(s_card); s_card=nullptr; return ret; }
    host_init_called = true;
    s_host_inited = true;

    ret = sdmmc_host_init_slot(s_host.slot, &slot_config);
    if (ret != ESP_OK) goto fail;

    ret = sdmmc_card_init(&s_host, s_card);
    if (ret != ESP_OK) goto fail;

    sdmmc_card_print_info(stdout, s_card);
    return ESP_OK;

fail:
    if (host_init_called) { (*s_host.deinit)(); s_host_inited=false; }
    if (s_card) { free(s_card); s_card=nullptr; }
    return ret;
}

static void deinit_sdmmc_card(void)
{
    if (s_host_inited) { (*s_host.deinit)(); s_host_inited=false; }
    if (s_card) { free(s_card); s_card=nullptr; }
}

// -------------------------- Public API ----------------------------
void usb_console_teardown(void)
{
    // NOTE: tinyusb_console_deinit() must not be used here. It "restores" stdin/stdout/stderr by reopening
    // /dev/uart/<console number>, but this firmware has no console (number -1), so the reopen fails, the
    // std streams become NULL and the very next log line crashes. Just detach the CDC VFS instead: the
    // streams stay valid and further writes simply fail harmlessly.
    usb_msc_set_stage("console off");
    esp_vfs_tusb_cdc_unregister(NULL);
    usb_msc_set_stage("cdc off");
    tinyusb_cdcacm_deinit(TINYUSB_CDC_ACM_0);
    usb_msc_set_stage("usb off");
    tinyusb_driver_uninstall();
}

esp_err_t usb_msc_sdmmc_start(gpio_num_t clk, gpio_num_t cmd, gpio_num_t d0, int bus_width)
{
    if (s_msc_handle || s_tinyusb_inited) {
        ESP_LOGW(TAG, "Already started");
        return ESP_OK;
    }

    s_fail_stage = "";
    s_fail_code = ESP_OK;
    usb_msc_set_stage("sd init");

    // Big (SDXC) cards can be picky: if the card does not initialize at full speed, retry with a slower bus clock
    static const int freqs[] = {SDMMC_FREQ_DEFAULT, SDMMC_FREQ_PROBING * 25, SDMMC_FREQ_PROBING * 10};
    esp_err_t ret = ESP_FAIL;
    for (int attempt = 0; attempt < 3; attempt++) {
        ret = init_sdmmc_card(clk, cmd, d0, bus_width, freqs[attempt]);
        if (ret == ESP_OK) break;
        ESP_LOGW(TAG, "Card init attempt %d failed: %s", attempt + 1, esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    if (ret != ESP_OK) {
        s_fail_stage = "card";
        s_fail_code = ret;
        return ret;
    }

    usb_msc_set_stage("usb install");

    // TinyUSB descriptors
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.task.size = 8192; // the default 4 KB is tight for FAT / SD work
    tusb_cfg.descriptor.device              = &s_device_desc;
    tusb_cfg.descriptor.full_speed_config   = s_fs_cfg_desc;
    tusb_cfg.descriptor.string              = s_string_desc;
    tusb_cfg.descriptor.string_count        = sizeof(s_string_desc) / sizeof(s_string_desc[0]);

    ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        s_fail_stage = "usb";
        s_fail_code = ret;
        deinit_sdmmc_card();
        return ret;
    }
    s_tinyusb_inited = true;

    usb_msc_set_stage("msc install");

    // Install the MSC driver ourselves so the SD card is NOT auto-mounted as a FAT volume inside the
    // TinyUSB task whenever the USB host connects/disconnects (the host owns the card in this mode,
    // and a failed mount there could even try to reformat it)
    tinyusb_msc_driver_config_t msc_drv_cfg = {};
    msc_drv_cfg.user_flags.auto_mount_off = true;
    msc_drv_cfg.callback = msc_event_cb;
    msc_drv_cfg.callback_arg = nullptr;
    ret = tinyusb_msc_install_driver(&msc_drv_cfg);
    if (ret != ESP_OK) {
        s_fail_stage = "msc";
        s_fail_code = ret;
        tinyusb_driver_uninstall();
        s_tinyusb_inited = false;
        deinit_sdmmc_card();
        return ret;
    }
    s_msc_driver_installed = true;

    // Configure SDMMC storage for MSC
    tinyusb_msc_storage_config_t storage_cfg = {};
    storage_cfg.medium.card = s_card;
    storage_cfg.mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB;
    storage_cfg.fat_fs.do_not_format = true; // a safe-eject remount must never format the user's SD card

    ret = tinyusb_msc_new_storage_sdmmc(&storage_cfg, &s_msc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_msc_storage_init_sdmmc failed: %s", esp_err_to_name(ret));
        s_fail_stage = "msc";
        s_fail_code = ret;
        tinyusb_msc_uninstall_driver();
        s_msc_driver_installed = false;
        tinyusb_driver_uninstall();
        s_tinyusb_inited = false;
        deinit_sdmmc_card();
        return ret;
    }

    usb_msc_set_stage("msc running");
    ESP_LOGI(TAG, "USB MSC started (SDMMC, %d-bit)", bus_width);
    return ESP_OK;
}

void usb_msc_stop(void)
{
    if (s_msc_handle) {
        tinyusb_msc_delete_storage(s_msc_handle);
        s_msc_handle = nullptr;
    }
    if (s_msc_driver_installed) {
        tinyusb_msc_uninstall_driver();
        s_msc_driver_installed = false;
    }
    if (s_tinyusb_inited) {
        tinyusb_driver_uninstall();
        s_tinyusb_inited = false;
    }
    deinit_sdmmc_card();
    usb_msc_clear_stage(); // clean exit
    ESP_LOGI(TAG, "USB MSC stopped");
}

bool usb_msc_in_use_by_host(void)
{
    if (!s_msc_handle) return false;
    tinyusb_msc_mount_point_t mount;
    tinyusb_msc_get_storage_mount_point(s_msc_handle, &mount);
    return (mount == TINYUSB_MSC_STORAGE_MOUNT_USB);
}
