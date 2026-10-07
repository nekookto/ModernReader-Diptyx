#include "bookHandler.h"
#include "htmlParser.h"
#include "reader.h"
#include "cJSON.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include "device.h"
#include "esp_log.h"
static const char *TAG = "Bookhandler";

RTC_NOINIT_ATTR char rtc_currently_parsing_book[MAX_BOOK_NAME] = {0};

bool ends_with_epub(const char *filename) {
    if (!filename || filename[0] == '.') {
        return false;   // reject hidden files
    }

    const char *ext = strrchr(filename, '.');
    return ext && strcasecmp(ext, ".epub") == 0;
}


bool rtc_isCurrentlyParsing(const std::string& bookPath)
{
    if (rtc_currently_parsing_book[0] == '\0') {
        return false;
    }
    return bookPath == std::string(rtc_currently_parsing_book);
}

void rtc_setCurrentlyParsing(const std::string& bookPath)
{
    std::strncpy(rtc_currently_parsing_book,
                 bookPath.c_str(),
                 MAX_BOOK_NAME - 1);
    rtc_currently_parsing_book[MAX_BOOK_NAME - 1] = '\0';
}

void rtc_clearCurrentlyParsing()
{
    rtc_currently_parsing_book[0] = '\0';
}

std::string rtc_getCurrentlyParsing()
{
    return std::string(rtc_currently_parsing_book);
}

// simple suffix check
static bool ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return false;
    size_t l1 = strlen(str);
    size_t l2 = strlen(suffix);
    if (l2 > l1) return false;
    return strcmp(str + l1 - l2, suffix) == 0;
}

// the file name of an epub without its folders
static std::string baseName(const std::string &path) {
    size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

// book paths can contain folders; the metadata file lives in a flat folder, so the slashes are replaced
static std::string metaFileName(const std::string &bookPath) {
    std::string name = bookPath;
    for (auto &c : name) if (c == '/') c = '~';
    return name + ".json";
}

// ensure the /bookStorage/books directory exists
static void ensureBooksDir() {
    const char *dirpath = "/littlefs/books";
    struct stat st;
    if (stat(dirpath, &st) != 0) {
        // doesn't exist -> try to create (recursive mkdir not required here since /bookStorage should exist)
        if (mkdir(dirpath, 0777) != 0) {
            ESP_LOGE(TAG, "mkdir(%s) failed: %s", dirpath, strerror(errno));
        } else {
            ESP_LOGI(TAG, "Created directory: %s", dirpath);
        }
    } else {
        if (!S_ISDIR(st.st_mode)) {
            ESP_LOGW(TAG, "%s exists but is not a directory", dirpath);
        } // else directory exists and is fine
    }
}

Book::Book(std::string path, std::string title, std::string author, int totalPageCount)
{
    this->author = std::move(author);
    this->title = std::move(title);
    this->path = std::move(path);
    this->currentPage = 0;
    this->totalPageCount = totalPageCount;
    this->chapterCount = 0;
    this->chapterPageCounts.clear();
    // renderSettings default constructed
}

Book::Book(std::string path,
           std::string title,
           std::string author,
           int totalPageCount,
           int chapterCount,
           const std::vector<int>& chapterPageCounts,
           const RenderSettings& renderSettings)
{
    this->path = std::move(path);
    this->title = std::move(title);
    this->author = std::move(author);
    this->totalPageCount = totalPageCount;
    this->chapterCount = chapterCount;
    this->chapterPageCounts = chapterPageCounts;
    this->currentPage = 0;
    this->renderSettings = renderSettings;
}

Book::~Book()
{
    // nothing special; BookHandler will own/free Book pointers
}

Author::Author(std::string name)
{
    this->name = name;
}

void BookHandler::initFilesystem() {
    //mount the bookstorage filesystem. book metadata is stored here
    esp_vfs_littlefs_conf_t conf = {  
        .base_path = "/littlefs",
        .partition_label = "bookStorage",
        .format_if_mount_failed = true,
        .read_only = false,
    };
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE("FS", "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE("FS", "Failed to find LittleFS partition");
        } else {
            ESP_LOGE("FS", "Failed to init LittleFS (%s)", esp_err_to_name(ret));
        }
    } else {
        size_t total, used;
        esp_littlefs_info("bookStorage", &total, &used);
        ESP_LOGI("FS", "LittleFS mounted. Partition size: total: %d, used: %d", total, used);
    }
    ensureBooksDir();

    //mount the assets filesystem. System epubs are stored here
    esp_vfs_littlefs_conf_t conf_2 = {
        .base_path = "/assets",
        .partition_label = "assets",
        .format_if_mount_failed = true,
        .read_only = false,
    };
    
    esp_err_t ret_2 = esp_vfs_littlefs_register(&conf_2);

    if (ret_2 != ESP_OK) {
        ESP_LOGE("FS", "assets mount failed: %s", esp_err_to_name(ret_2));
    } else {
        ESP_LOGI("FS", "assets mounted");
    }
}

std::string Book::toJSON() const {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "path", path.c_str());
    cJSON_AddStringToObject(root, "title", title.c_str());
    cJSON_AddStringToObject(root, "author", author.c_str());
    cJSON_AddStringToObject(root, "series", series.c_str());
    cJSON_AddNumberToObject(root, "seriesIndex", seriesIndex);
    cJSON_AddNumberToObject(root, "seriesChecked", seriesChecked);
    cJSON_AddNumberToObject(root, "totalPageCount", totalPageCount);
    cJSON_AddNumberToObject(root, "chapterCount", chapterCount);
    cJSON_AddNumberToObject(root, "currentPageChapterIndex", currentPageChapterIndex);
    cJSON_AddNumberToObject(root, "currentPageElementIndex", currentPageElementIndex);
    cJSON_AddNumberToObject(root, "currentPage", currentPage);
    cJSON_AddNumberToObject(root, "badParse", badParse);
    cJSON_AddNumberToObject(root, "favorite", favorite);
    cJSON_AddNumberToObject(root, "lastOpened", (double)lastOpened);

    // chapters
    cJSON *chapters = cJSON_CreateArray();
    for (int v : chapterPageCounts) {
        cJSON_AddItemToArray(chapters, cJSON_CreateNumber(v));
    }
    cJSON_AddItemToObject(root, "chapterPageCounts", chapters);

    // bookmarks (as arrays [page, chapter, element])
    cJSON *bmArray = cJSON_CreateArray();
    for (const BookMark &bm : bookMarks) {
        cJSON *arr = cJSON_CreateArray();
        cJSON_AddItemToArray(arr, cJSON_CreateNumber(bm.pageIndex));
        cJSON_AddItemToArray(arr, cJSON_CreateNumber(bm.chapterIndex));
        cJSON_AddItemToArray(arr, cJSON_CreateNumber(bm.elementIndex));
        cJSON_AddItemToArray(bmArray, arr);
    }
    cJSON_AddItemToObject(root, "bookMarks", bmArray);

    // render settings
    cJSON *r = cJSON_CreateObject();
    cJSON_AddNumberToObject(r, "fontSize", renderSettings.fontSize);
    cJSON_AddNumberToObject(r, "fontBold", renderSettings.fontBold);
    cJSON_AddNumberToObject(r, "marginsHorizontal", renderSettings.marginsHorizontal);
    cJSON_AddNumberToObject(r, "marginsVertical", renderSettings.marginsVertical);
    cJSON_AddNumberToObject(r, "fontPoints", renderSettings.fontPoints);
    cJSON_AddNumberToObject(r, "lineSpacing", renderSettings.lineSpacing);
    cJSON_AddStringToObject(r, "fontName", renderSettings.fontFamily.c_str());
    cJSON_AddItemToObject(root, "renderSettings", r);

    // cached images
    cJSON *imgArray = cJSON_CreateArray();
    for (const cachedImage &img : cachedImages) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "filePath", img.filePath.c_str());
        cJSON_AddNumberToObject(obj, "width", img.width);
        cJSON_AddNumberToObject(obj, "height", img.height);
        cJSON_AddItemToArray(imgArray, obj);
    }
    cJSON_AddItemToObject(root, "cachedImages", imgArray);

    // finalize
    char *jsonStr = cJSON_PrintUnformatted(root);
    std::string result = jsonStr ? std::string(jsonStr) : "{}";
    if (jsonStr) cJSON_free(jsonStr);
    cJSON_Delete(root);
    return result;
}


Book *Book::fromJSON(const std::string &json) {
    cJSON *root = cJSON_Parse(json.c_str());
    if (!root) return nullptr;

    cJSON *jpath = cJSON_GetObjectItem(root, "path");
    cJSON *jtitle = cJSON_GetObjectItem(root, "title");
    cJSON *jauthor = cJSON_GetObjectItem(root, "author");
    cJSON *jpages = cJSON_GetObjectItem(root, "totalPageCount");
    

    if (!cJSON_IsString(jpath) || !cJSON_IsString(jtitle) || !cJSON_IsString(jauthor) || !cJSON_IsNumber(jpages)) {
        cJSON_Delete(root);
        return nullptr;
    }

    Book *book = new Book(jpath->valuestring, jtitle->valuestring, jauthor->valuestring, jpages->valueint);

    cJSON *jchapterCount = cJSON_GetObjectItem(root, "chapterCount");
    cJSON *jchapterPages = cJSON_GetObjectItem(root, "chapterPageCounts");
    cJSON *jread = cJSON_GetObjectItem(root, "currentPage");
    cJSON *jreadChapter = cJSON_GetObjectItem(root, "currentPageChapterIndex");
    cJSON *jreadElement = cJSON_GetObjectItem(root, "currentPageElementIndex");
    cJSON *jrender = cJSON_GetObjectItem(root, "renderSettings");
    cJSON *jbadParse = cJSON_GetObjectItem(root, "badParse");
    cJSON *jfavorite = cJSON_GetObjectItem(root, "favorite");
    cJSON *jlastOpened = cJSON_GetObjectItem(root, "lastOpened");
    cJSON *jbookMarks = cJSON_GetObjectItem(root, "bookMarks");
    cJSON *jcachedImages = cJSON_GetObjectItem(root, "cachedImages");

    cJSON *jseries = cJSON_GetObjectItem(root, "series");
    cJSON *jseriesIndex = cJSON_GetObjectItem(root, "seriesIndex");
    cJSON *jseriesChecked = cJSON_GetObjectItem(root, "seriesChecked");
    if (cJSON_IsString(jseries)) book->series = jseries->valuestring;
    book->seriesIndex = cJSON_IsNumber(jseriesIndex) ? (float)jseriesIndex->valuedouble : 0;
    book->seriesChecked = cJSON_IsNumber(jseriesChecked) ? jseriesChecked->valueint : false; // old files: not scanned yet

    book->currentPageChapterIndex = cJSON_IsNumber(jreadChapter) ? jreadChapter->valueint : 0;
    book->currentPageElementIndex = cJSON_IsNumber(jreadElement) ? jreadElement->valueint : 0;
    book->chapterCount = cJSON_IsNumber(jchapterCount) ? jchapterCount->valueint : 0;
    book->currentPage = cJSON_IsNumber(jread) ? jread->valueint : 0;
    book->badParse = cJSON_IsNumber(jbadParse) ? jbadParse->valueint : false;
    book->favorite = cJSON_IsNumber(jfavorite) ? jfavorite->valueint : false;
    book->lastOpened = cJSON_IsNumber(jlastOpened) ? (uint32_t)jlastOpened->valuedouble : 0; // old files: never opened

    // chapterPageCounts
    if (jchapterPages && cJSON_IsArray(jchapterPages)) {
        int chapterLen = cJSON_GetArraySize(jchapterPages);
        for (int i = 0; i < chapterLen; ++i) {
            cJSON *item = cJSON_GetArrayItem(jchapterPages, i);
            book->chapterPageCounts.push_back(cJSON_IsNumber(item) ? item->valueint : 0);
        }
    }

    // bookmarks (supports both new array and old int formats)
    if (jbookMarks && cJSON_IsArray(jbookMarks)) {
        int bmLen = cJSON_GetArraySize(jbookMarks);
        for (int i = 0; i < bmLen; ++i) {
            cJSON *item = cJSON_GetArrayItem(jbookMarks, i);
            BookMark bm{};
            if (cJSON_IsArray(item)) {
                // new format [page, chapter, element]
                bm.pageIndex = cJSON_IsNumber(cJSON_GetArrayItem(item, 0)) ? cJSON_GetArrayItem(item, 0)->valueint : 0;
                bm.chapterIndex = cJSON_IsNumber(cJSON_GetArrayItem(item, 1)) ? cJSON_GetArrayItem(item, 1)->valueint : 0;
                bm.elementIndex = cJSON_IsNumber(cJSON_GetArrayItem(item, 2)) ? cJSON_GetArrayItem(item, 2)->valueint : 0;
            } else if (cJSON_IsNumber(item)) {
                // legacy integer format
                bm.pageIndex = item->valueint;
                bm.chapterIndex = 0;
                bm.elementIndex = 0;
            }
            book->bookMarks.push_back(bm);
        }
    }

    // cachedImages
    if (jcachedImages && cJSON_IsArray(jcachedImages)) {
        int imgLen = cJSON_GetArraySize(jcachedImages);
        for (int i = 0; i < imgLen; ++i) {
            cJSON *item = cJSON_GetArrayItem(jcachedImages, i);
            if (!cJSON_IsObject(item)) continue;

            cachedImage img{};

            cJSON *jp = cJSON_GetObjectItem(item, "filePath");
            cJSON *jw = cJSON_GetObjectItem(item, "width");
            cJSON *jh = cJSON_GetObjectItem(item, "height");

            if (cJSON_IsString(jp)) img.filePath = jp->valuestring;
            if (cJSON_IsNumber(jw)) img.width = jw->valueint;
            if (cJSON_IsNumber(jh)) img.height = jh->valueint;

            book->cachedImages.push_back(img);
        }
    }

    // renderSettings
    if (jrender && cJSON_IsObject(jrender)) {
        cJSON *jf = cJSON_GetObjectItem(jrender, "fontSize");
        cJSON *jb = cJSON_GetObjectItem(jrender, "fontBold");
        cJSON *mh = cJSON_GetObjectItem(jrender, "marginsHorizontal");
        cJSON *mv = cJSON_GetObjectItem(jrender, "marginsVertical");
        cJSON *ls = cJSON_GetObjectItem(jrender, "lineSpacing");
        cJSON *fp = cJSON_GetObjectItem(jrender, "fontPoints");
        cJSON *fn = cJSON_GetObjectItem(jrender, "fontName");
        if (cJSON_IsNumber(jf)) book->renderSettings.fontSize = jf->valueint;
        if (cJSON_IsNumber(jb)) book->renderSettings.fontBold = jb->valueint;
        if (cJSON_IsNumber(mh)) book->renderSettings.marginsHorizontal = mh->valueint;
        if (cJSON_IsNumber(mv)) book->renderSettings.marginsVertical = mv->valueint;
        if (cJSON_IsNumber(ls)) book->renderSettings.lineSpacing = ls->valueint;
        if (cJSON_IsNumber(fp)) book->renderSettings.fontPoints = fp->valueint;
        if (cJSON_IsString(fn)) book->renderSettings.fontFamily = fn->valuestring;
    }

    cJSON_Delete(root);
    return book;
}



bool Book::matchesRenderSettings(const RenderSettings &current) const {
    return renderSettings.fontSize == current.fontSize &&
           renderSettings.fontBold == current.fontBold &&
           renderSettings.marginsHorizontal == current.marginsHorizontal &&
           renderSettings.marginsVertical == current.marginsVertical &&
           renderSettings.lineSpacing == current.lineSpacing &&
           renderSettings.fontPoints == current.fontPoints &&
           renderSettings.fontFamily == current.fontFamily;
}

Book::RenderSettings Book::getCurrentRenderSettings() {
    Device &dev = Device::getInstance();
    RenderSettings currentRS {
            dev.renderSettings.fontSize,
            dev.renderSettings.fontBold,
            dev.renderSettings.marginsHorizontal,
            dev.renderSettings.marginsVertical,
            dev.renderer->fontHandler.families[dev.renderSettings.fontFamily].name,
            dev.renderSettings.fontPoints,
            dev.renderSettings.lineSpacing,
        };
    return currentRS;
}

BookHandler::BookHandler()
{
    initFilesystem();
}

BookHandler::~BookHandler()
{
    // free all owned Book pointers
    for (auto &kv : indexedBooks) {
        delete kv.second;
    }
    indexedBooks.clear();
    // bookList contains pointers owned above, so clear it (no double delete)
    bookList.clear();
    authorList.clear();
}



void BookHandler::loadBooks()
{
    // Free existing books
    for (auto &kv : indexedBooks) {
        delete kv.second;
    }
    indexedBooks.clear();

    bool loadFromSD = Device::getInstance().deviceSettings.storeDataOnSD;
    const char *basePath = loadFromSD ? "/sdcard/book_data" : "/littlefs/books";

    DIR *dir = opendir(basePath);
    if (!dir) {
        ESP_LOGW(TAG, "No book storage directory found: %s", basePath);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) continue;

        std::string filename(entry->d_name);
        if (!ends_with(filename.c_str(), ".json")) continue;

        std::string fullPath = std::string(basePath) + "/" + filename;

        FILE *f = fopen(fullPath.c_str(), "r");
        if (!f) {
            ESP_LOGW(TAG, "Failed to open %s", fullPath.c_str());
            continue;
        }

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (size <= 0) {
            fclose(f);
            continue;
        }

        std::vector<char> buffer(size + 1);
        fread(buffer.data(), 1, size, f);
        fclose(f);
        buffer[size] = '\0';

        Book *book = Book::fromJSON(buffer.data());
        if (book) {
            indexedBooks[book->path] = book;
        } else {
            ESP_LOGW(TAG, "Invalid book JSON: %s", fullPath.c_str());
        }
    }

    closedir(dir);

    ESP_LOGI(
        TAG,
        "Loaded %d books from %s",
        indexedBooks.size(),
        loadFromSD ? "SD card" : "flash"
    );
}


// save this->indexedBooks to NVS
void BookHandler::saveBook(Book *book)
{
    if (!book) return;

    const char *basePath = Device::getInstance().deviceSettings.storeDataOnSD ? "/sdcard/book_data" : "/littlefs/books";

    // Ensure directory exists
    struct stat st;
    if (stat(basePath, &st) != 0) {
        if (mkdir(basePath, 0777) != 0) {
            ESP_LOGE(TAG, "Failed to create directory: %s", basePath);
            return;
        }
    }

    std::string filename = std::string(basePath) + "/" + metaFileName(book->path);
    std::string jsonStr = book->toJSON();

    FILE *f = fopen(filename.c_str(), "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s for writing", filename.c_str());
        return;
    }

    size_t written = fwrite(jsonStr.c_str(), 1, jsonStr.size(), f);
    fclose(f);

    if (written != jsonStr.size()) {
        ESP_LOGE(TAG, "Short write when saving book metadata: %s", filename.c_str());
        return;
    }

    ESP_LOGI(
        TAG,
        "Saved book metadata (%s): %s",
        Device::getInstance().deviceSettings.storeDataOnSD ? "SD" : "FLASH",
        filename.c_str()
    );
}


void BookHandler::deleteBook(const std::string &bookPath)
{
    bool deleteFromSD = Device::getInstance().deviceSettings.storeDataOnSD;
    const char *basePath = deleteFromSD ? "/sdcard/book_data" : "/littlefs/books";

    std::string filename = std::string(basePath) + "/" + metaFileName(bookPath);

    if (remove(filename.c_str()) != 0) {
        ESP_LOGW(TAG, "Failed to delete book file: %s", filename.c_str());
    } else {
        ESP_LOGI(TAG, "Deleted book file: %s", filename.c_str());
    }

    auto it = indexedBooks.find(bookPath);
    if (it != indexedBooks.end()) {
        delete it->second;
        indexedBooks.erase(it);
    }
}

void BookHandler::transferDataToSD()
{
    const char *flashPath = "/littlefs/books";
    const char *sdPath    = "/sdcard/book_data";

    // Ensure SD directory exists
    struct stat st;
    if (stat(sdPath, &st) != 0) {
        if (mkdir(sdPath, 0777) != 0) {
            ESP_LOGE(TAG, "Failed to create SD book_data directory");
            return;
        }
    }

    DIR *dir = opendir(flashPath);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open flash books directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) continue;

        std::string filename(entry->d_name);
        if (!ends_with(filename.c_str(), ".json")) continue;

        std::string srcPath  = std::string(flashPath) + "/" + filename;
        std::string destPath = std::string(sdPath)    + "/" + filename;

        FILE *src = fopen(srcPath.c_str(), "r");
        if (!src) {
            ESP_LOGW(TAG, "Failed to open source file: %s", srcPath.c_str());
            continue;
        }

        FILE *dst = fopen(destPath.c_str(), "w");
        if (!dst) {
            ESP_LOGW(TAG, "Failed to open destination file: %s", destPath.c_str());
            fclose(src);
            continue;
        }

        char buffer[512];
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
            fwrite(buffer, 1, bytes, dst);
        }

        fclose(src);
        fclose(dst);

        ESP_LOGI(TAG, "Transferred book metadata: %s", filename.c_str());
    }

    closedir(dir);

    ESP_LOGI(TAG, "Book data transfer from flash to SD completed");
}

void BookHandler::transferDataToFlash()
{
    const char *sdPath    = "/sdcard/book_data";
    const char *flashPath = "/littlefs/books";

    // Ensure flash directory exists
    struct stat st;
    if (stat(flashPath, &st) != 0) {
        if (mkdir(flashPath, 0777) != 0) {
            ESP_LOGE(TAG, "Failed to create flash books directory");
            return;
        }
    }

    DIR *dir = opendir(sdPath);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open SD book_data directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) continue;

        std::string filename(entry->d_name);
        if (!ends_with(filename.c_str(), ".json")) continue;

        std::string srcPath  = std::string(sdPath)    + "/" + filename;
        std::string destPath = std::string(flashPath) + "/" + filename;

        FILE *src = fopen(srcPath.c_str(), "r");
        if (!src) {
            ESP_LOGW(TAG, "Failed to open source file: %s", srcPath.c_str());
            continue;
        }

        FILE *dst = fopen(destPath.c_str(), "w");
        if (!dst) {
            ESP_LOGW(TAG, "Failed to open destination file: %s", destPath.c_str());
            fclose(src);
            continue;
        }

        char buffer[512];
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
            fwrite(buffer, 1, bytes, dst);
        }

        fclose(src);
        fclose(dst);

        ESP_LOGI(TAG, "Transferred book metadata: %s", filename.c_str());
    }

    closedir(dir);

    ESP_LOGI(TAG, "Book data transfer from SD to flash completed");
}


// Collect all epubs below relDir (relative to /sdcard), recursively up to maxDepth sub folders deep
static void scanEpubs(const std::string &relDir, int depth, int maxDepth, std::vector<std::string> &out)
{
    std::string fullDir = relDir.empty() ? std::string("/sdcard") : std::string("/sdcard/") + relDir;
    DIR *d = opendir(fullDir.c_str());
    if (!d) return;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue; // hidden files and folders
        std::string rel = relDir.empty() ? std::string(entry->d_name) : relDir + "/" + entry->d_name;
        if (entry->d_type == DT_DIR) {
            if (depth < maxDepth) scanEpubs(rel, depth + 1, maxDepth, out);
            continue;
        }
        if (ends_with_epub(entry->d_name)) out.push_back(rel);
    }
    closedir(d);
}

// Finds the epubs on the sd card: the ones in the root folder, and everything inside the "Books" folder
static std::vector<std::string> findEpubFiles()
{
    std::vector<std::string> found;
    DIR *root = opendir("/sdcard");
    if (!root) {
        ESP_LOGE(TAG, "Failed to open directory: /sdcard");
        return found;
    }
    std::vector<std::string> bookFolders;
    struct dirent *entry;
    while ((entry = readdir(root)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (entry->d_type == DT_DIR) {
            if (strcasecmp(entry->d_name, "Books") == 0) bookFolders.push_back(entry->d_name);
        } else if (ends_with_epub(entry->d_name)) {
            found.push_back(entry->d_name);
        }
    }
    closedir(root);
    for (const std::string &folder : bookFolders) scanEpubs(folder, 0, 5, found);
    return found;
}

void BookHandler::listBooks(void)
{
    const char *sdPath = "/sdcard";

    DIR *sdCheck = opendir(sdPath);
    if (!sdCheck) {
        ESP_LOGE(TAG, "Failed to open directory: %s", sdPath);
        return;
    }
    closedir(sdCheck);

    Device &dev = Device::getInstance();
    if (dev.activeBookPath.empty()) dev.activeBookIndex = 0;

    // Step 1: Load all cached books from LittleFS
    loadBooks();  // fills indexedBooks with any JSON files found

    // Build current render settings snapshot
    Book::RenderSettings currentRS {
        dev.renderSettings.fontSize,
        dev.renderSettings.fontBold,
        dev.renderSettings.marginsHorizontal,
        dev.renderSettings.marginsVertical
    };

    // Step 2: Scan /sdcard and process each EPUB
    std::vector<std::string> foundPaths;
    for (const std::string &fileName : findEpubFiles()) {
        foundPaths.push_back(fileName);

        Book *book = nullptr;
        auto it = indexedBooks.find(fileName);

        if (it != indexedBooks.end()) {
            // Cached book exists
            book = it->second;
            this->bookList.push_back(book);  
        } else {
            // New book -> index and save
            std::string fullPath = std::string(sdPath) + "/" + fileName;
            if (rtc_isCurrentlyParsing(fileName)) { //if the book is malformed and somehow crashes the loader, this is triggered
                //notify the user about the error, and store the epub as malformed
                dev.notificationHandler->drawErrorNotification(baseName(fileName));
                book = new Book(fileName,
                                    baseName(fileName),
                                    std::string("error during parsing"),
                                    0,
                                    0,
                                    std::vector<int> {},
                                    currentRS);
                book->badParse = true;
                this->bookList.push_back(book);
                indexedBooks[fileName] = book;
                // Save immediately to LittleFS
                saveBook(book);
                rtc_clearCurrentlyParsing();
                vTaskDelay(100);
            }
            else
            {
                rtc_setCurrentlyParsing(fileName);
                Epub *epub = new Epub(fullPath);
                ESP_LOGI(TAG, "Indexing new book: %s", fullPath.c_str());
                vTaskDelay(20);

                if (epub->load()) {
                    std::string title = epub->get_title();
                    std::string author = epub->get_author();
                    dev.notificationHandler->drawIndexingNotification(title);

                    Reader reader;
                    book = new Book(fileName,
                                    title,
                                    author,
                                    0,
                                    0,
                                    std::vector<int> {},
                                    currentRS);
                    book->series = epub->get_series();
                    book->seriesIndex = epub->get_series_index();
                    book->seriesChecked = true;
                    book->renderSettings.fontSize=-1; //change this to some bad value so it is forced to re-index on opening
                    //reader.init(book, nullptr);
                    //reader.indexPages(); //don't init here actually, it is done upon first opening
                    this->bookList.push_back(book);
                    indexedBooks[fileName] = book;

                    // Save immediately to LittleFS
                    saveBook(book);
                }
                else
                {
                    //if the epub loading is unsuccessful, we also store the epub as malformed and inform the user
                    dev.notificationHandler->drawErrorNotification(baseName(fileName));
                    book = new Book(fileName,
                                        baseName(fileName),
                                        std::string("error during parsing"),
                                        0,
                                        0,
                                        std::vector<int> {},
                                        currentRS);
                    book->badParse = true;
                    this->bookList.push_back(book);
                    indexedBooks[fileName] = book;
                    // Save immediately to LittleFS
                    saveBook(book);
                    vTaskDelay(100);
                }
                delete epub;
                rtc_clearCurrentlyParsing();
            }
        }

        if (dev.activeBookPath.empty()) dev.activeBookPath = fileName;
    }
    // Step 3: Prune metadata for missing books (not found in /sdcard)
const char *basePath =
    Device::getInstance().deviceSettings.storeDataOnSD
        ? "/sdcard/book_data"
        : "/littlefs/books";

for (auto it = indexedBooks.begin(); it != indexedBooks.end();) {
    if (std::find(foundPaths.begin(), foundPaths.end(), it->first) == foundPaths.end()) {

        if (it->second->badParse) {
            std::string metaFile =
                std::string(basePath) + "/" + metaFileName(it->first);

            if (unlink(metaFile.c_str()) == 0) {
                ESP_LOGI(TAG, "Pruned stale bad-parse metadata: %s", metaFile.c_str());
            } else {
                ESP_LOGW(TAG, "Failed to remove stale bad-parse metadata: %s", metaFile.c_str());
            }
        }

        delete it->second;
        it = indexedBooks.erase(it);
    } else {
        ++it;
    }
}

    // Step 4: make sure the series info is available (only needed when grouping by title/series)
    loadMissingSeriesInfo();

    // Step 5: Sort books into authorList (by author or by title/series, depending on the setting)
    groupBooks();
}

// ---- library grouping helpers ----

static std::string lowerCopy(const std::string &in) {
    std::string out = in;
    for (auto &c : out) c = (char)tolower((unsigned char)c);
    return out;
}

// lowercase title without a leading "the " / "a " / "an ", used for sorting and for the letter groups
static std::string titleSortKey(const std::string &title) {
    std::string t = lowerCopy(title);
    size_t start = 0;
    while (start < t.size() && isspace((unsigned char)t[start])) start++;
    t = t.substr(start);
    for (const char *article : {"the ", "a ", "an "}) {
        size_t len = strlen(article);
        if (t.compare(0, len, article) == 0 && t.size() > len) {
            t = t.substr(len);
            break;
        }
    }
    return t;
}

// "Natural" comparison: numbers inside the text are compared by value, so "Vol 2" comes before "Vol 10"
static int naturalCompare(const std::string &a, const std::string &b) {
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (isdigit((unsigned char)a[i]) && isdigit((unsigned char)b[j])) {
            size_t si = i, sj = j;
            while (si < a.size() && a[si] == '0') si++; // skip leading zeros
            while (sj < b.size() && b[sj] == '0') sj++;
            size_t ei = si, ej = sj;
            while (ei < a.size() && isdigit((unsigned char)a[ei])) ei++;
            while (ej < b.size() && isdigit((unsigned char)b[ej])) ej++;
            if (ei - si != ej - sj) return (ei - si) < (ej - sj) ? -1 : 1; // fewer digits = smaller number
            int c = a.compare(si, ei - si, b, sj, ej - sj);
            if (c != 0) return c < 0 ? -1 : 1;
            i = ei;
            j = ej;
        } else {
            if (a[i] != b[j]) return (unsigned char)a[i] < (unsigned char)b[j] ? -1 : 1;
            i++;
            j++;
        }
    }
    if (i < a.size()) return 1;
    if (j < b.size()) return -1;
    return 0;
}

// Looks for a volume number in a title or file name: "Vol. 3", "Book 2", "#4", "v05", or a number at the very end.
// Returns 0 when there is none.
static float volumeFromText(const std::string &text) {
    std::string s = lowerCopy(text);
    if (s.size() > 5 && s.compare(s.size() - 5, 5, ".epub") == 0) s.resize(s.size() - 5);
    static const char *keywords[] = {"vol", "volume", "v", "book", "bk", "part", "pt", "no", "issue", "episode", "ep", "tome", "band", "#"};
    float trailing = 0;
    size_t i = 0;
    while (i < s.size()) {
        if (!isdigit((unsigned char)s[i])) { i++; continue; }
        size_t start = i;
        while (i < s.size() && isdigit((unsigned char)s[i])) i++;
        size_t intEnd = i;
        if (i + 1 < s.size() && s[i] == '.' && isdigit((unsigned char)s[i + 1])) { // decimal volume, e.g. 2.5
            i++;
            while (i < s.size() && isdigit((unsigned char)s[i])) i++;
        }
        float value = (float)atof(s.substr(start, i - start).c_str());
        // the word in front of the number
        size_t k = start;
        while (k > 0 && (s[k - 1] == ' ' || s[k - 1] == '.' || s[k - 1] == '_' || s[k - 1] == '-' || s[k - 1] == ':')) k--;
        size_t wordEnd = k;
        if (k > 0 && s[k - 1] == '#') { return value > 0 ? value : 0; }
        while (k > 0 && isalpha((unsigned char)s[k - 1])) k--;
        std::string word = s.substr(k, wordEnd - k);
        for (const char *keyword : keywords) {
            if (word == keyword) return value;
        }
        // a short number at the very end of the name (ignoring closing brackets / spaces) counts as a volume
        size_t rest = i;
        while (rest < s.size() && (s[rest] == ' ' || s[rest] == ')' || s[rest] == ']')) rest++;
        if (rest >= s.size() && intEnd - start <= 2) trailing = value;
    }
    return trailing;
}


// Volume number from a file name: a leading number ("03 - Title.epub", "2. Title") counts too, otherwise as in volumeFromText
static float volumeFromFileName(const std::string &name) {
    std::string s = name;
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '[' || s[i] == '(')) i++;
    size_t start = i;
    while (i < s.size() && isdigit((unsigned char)s[i])) i++;
    size_t intEnd = i;
    if (intEnd > start && intEnd - start <= 3) {
        if (i + 1 < s.size() && s[i] == '.' && isdigit((unsigned char)s[i + 1])) { // 2.5
            i++;
            while (i < s.size() && isdigit((unsigned char)s[i])) i++;
            return (float)atof(s.substr(start, i - start).c_str());
        }
        // must be followed by a separator (not a letter or more digits), e.g. "03 - ", "03_", "3. ", "03)"
        if (i >= s.size() || s[i] == ' ' || s[i] == '-' || s[i] == '_' || s[i] == '.' || s[i] == ')' || s[i] == ']' || s[i] == ',') {
            float v = (float)atof(s.substr(start, intEnd - start).c_str());
            if (v > 0) return v;
        }
    }
    return volumeFromText(name);
}

// Position of a book in its series / folder: from the file name first (most reliable when the metadata is
// inconsistent), else from the book's own series number, else from the title
static float volumeOf(const Book *b) {
    float v = volumeFromFileName(baseName(b->path));
    if (v > 0) return v;
    if (b->seriesIndex > 0) return b->seriesIndex;
    return volumeFromText(b->title);
}

// Order inside a group: by volume number when known, then by title, then by file name (all natural order)
static bool volumeOrderLess(const Book *x, const Book *y) {
    float vx = volumeOf(x), vy = volumeOf(y);
    bool hx = vx > 0, hy = vy > 0;
    if (hx != hy) return hx; // books with a volume number first
    if (hx && vx != vy) return vx < vy;
    int c = naturalCompare(titleSortKey(x->title), titleSortKey(y->title));
    if (c != 0) return c < 0;
    return naturalCompare(lowerCopy(x->path), lowerCopy(y->path)) < 0;
}

// Folder a book is in, relative to the "Books" folder. Books outside of any folder are "Unsorted"
static std::string folderNameFor(const Book *b) {
    std::string path = b->path;
    size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) return "Unsorted";
    std::string dir = path.substr(0, slash);
    // strip the leading "Books" folder
    size_t first = dir.find('/');
    std::string top = dir.substr(0, first);
    if (strcasecmp(top.c_str(), "Books") == 0) dir = (first == std::string::npos) ? std::string() : dir.substr(first + 1);
    if (dir.empty()) return "Unsorted";
    std::string out;
    for (char c : dir) { if (c == '/') out += " / "; else out += c; }
    return out;
}

// Name of the group (menu entry) a book belongs to
static std::string groupNameFor(const Book *b) {
    int mode = Device::getInstance().deviceSettings.libraryGrouping;
    if (mode == 0) return b->author;
    if (mode == 2) return folderNameFor(b);
    if (!b->series.empty()) return b->series; // part of a series
    std::string key = titleSortKey(b->title);
    if (!key.empty() && isalpha((unsigned char)key[0])) return std::string(1, (char)toupper((unsigned char)key[0]));
    return "#";
}

void BookHandler::loadMissingSeriesInfo()
{
    if (Device::getInstance().deviceSettings.libraryGrouping != 1) return; // only needed when grouping by series
    for (Book *b : bookList) {
        if (b->seriesChecked || b->badParse) continue;
        std::string fileName = b->path;
        std::string fullPath = std::string("/sdcard/") + fileName;
        rtc_setCurrentlyParsing(fileName);
        Epub *epub = new Epub(fullPath);
        if (epub->load()) {
            b->series = epub->get_series();
            b->seriesIndex = epub->get_series_index();
        }
        delete epub;
        rtc_clearCurrentlyParsing();
        b->seriesChecked = true; // don't retry on every boot, even if there was no series info
        saveBook(b);
        vTaskDelay(1);
    }
}

// Called when the grouping setting changes: regroup the already loaded books
void BookHandler::regroupLibrary()
{
    Device &dev = Device::getInstance();
    Device::getInstance().notificationHandler->drawNotification("Grouping library...");
    loadMissingSeriesInfo();
    // keep the active book in a matching group, so it is not mistaken for a favorite
    for (Book *b : bookList) {
        if (b->path == dev.activeBookPath) {
            dev.activeAuthorName = groupNameFor(b);
            break;
        }
    }
    groupBooks();
    dev.menuHandler->layoutReadMenu(authorList);
    dev.menuHandler->authorMenu->selectedChildIndex = 0;
}

void BookHandler::groupBooks()
{
    Device &dev = Device::getInstance();
    authorList.clear();
    authorList.emplace_back("Favorite books");
    dev.activeAuthorIndex = -1;
    dev.activeBookIndex = dev.activeBookPath.empty() ? 0 : -1;

    if (dev.deviceSettings.libraryGrouping == 1) {
        // Series first (alphabetical), then the letter groups; inside a series by volume
        std::stable_sort(bookList.begin(), bookList.end(), [](const Book *x, const Book *y) {
            bool xs = !x->series.empty(), ys = !y->series.empty();
            if (xs != ys) return xs; // series before single titles
            if (xs) {
                int c = naturalCompare(lowerCopy(x->series), lowerCopy(y->series));
                if (c != 0) return c < 0;
                return volumeOrderLess(x, y);
            }
            int c = naturalCompare(titleSortKey(x->title), titleSortKey(y->title));
            if (c != 0) return c < 0;
            return naturalCompare(lowerCopy(x->path), lowerCopy(y->path)) < 0;
        });
    } else if (dev.deviceSettings.libraryGrouping == 2) {
        // Folders alphabetically ("Unsorted" last), inside a folder by volume / title
        std::stable_sort(bookList.begin(), bookList.end(), [](const Book *x, const Book *y) {
            std::string fx = folderNameFor(x), fy = folderNameFor(y);
            bool ux = fx == "Unsorted", uy = fy == "Unsorted";
            if (ux != uy) return uy;
            if (fx != fy) {
                int c = naturalCompare(lowerCopy(fx), lowerCopy(fy));
                if (c != 0) return c < 0;
            }
            return volumeOrderLess(x, y);
        });
    }

    // Sort books into authorList
    for (Book* b : bookList) {
        const std::string groupName = groupNameFor(b);
        Author* foundAuthor = nullptr;
        int currentAuthorIndex = 0;
        for (auto& a : authorList) {
            if (a.name == groupName) {
                foundAuthor = &a;
                break;
            }
            currentAuthorIndex++;
        }

        if (foundAuthor) {
            foundAuthor->bookList.push_back(b);
            if (groupName == dev.activeAuthorName) {
                dev.activeAuthorIndex = currentAuthorIndex;
            }
        } else {
            authorList.emplace_back(groupName);
            authorList.back().bookList.push_back(b);
            if (groupName == dev.activeAuthorName) {
                dev.activeAuthorIndex = authorList.size() - 1;
            }
            foundAuthor = &authorList.back();
        }

        if (dev.activeBookPath == b->path) {
            dev.activeBookIndex = foundAuthor->bookList.size() - 1;
            if(dev.activeAuthorIndex==-1) //this book is the current book, but it's author doesn't match the current author, so it is in the favorites
            {
                dev.activeBookIndex = authorList[0].bookList.size(); //the book is the last entry in the favorites list, but not yet added, so size+1-1
                dev.activeAuthorIndex = 0;
                dev.activeAuthorName = authorList[0].name;
            }
        }

        if(b->favorite)
        {
            authorList[0].bookList.push_back(b);
        }
    }

    if(dev.state == Device::State::simpleReader) 
    {
        dev.activeBookIndex = 0;
        dev.activeAuthorIndex = 0;
        return; //if we boot into the simplereader we don't need to check the activebook
    }

    // Fallback active indices if missing
    if (dev.activeBookIndex == -1 && !bookList.empty()) {
        dev.activeBookIndex = 0;
        dev.activeBookPath = bookList[0]->path;
    }
    if (dev.activeAuthorIndex == -1 && !authorList.empty()) {
        dev.activeAuthorIndex = 0;
        dev.activeAuthorName = authorList[0].name;
    }
}

std::string BookHandler::groupNameForBook(const Book *book)
{
    return groupNameFor(book);
}

void BookHandler::markOpened(Book *book)
{
    if (!book) return;
    uint32_t newest = 0;
    for (Book *b : bookList) if (b->lastOpened > newest) newest = b->lastOpened;
    if (book->lastOpened == newest && newest != 0) return; // already the most recent
    book->lastOpened = newest + 1;
    saveBook(book);
}

std::vector<Book*> BookHandler::getRecentBooks(size_t maxCount)
{
    // Books written by older firmware have no history. Treat the book that was active
    // before the update as the most recent one so the card isn't empty after upgrading.
    bool anyOpened = false;
    for (Book *b : bookList) if (b->lastOpened > 0) { anyOpened = true; break; }
    if (!anyOpened) {
        auto it = indexedBooks.find(Device::getInstance().activeBookPath);
        if (it != indexedBooks.end() && it->second->currentPage > 0) it->second->lastOpened = 1;
    }

    std::vector<Book*> recent;
    for (Book *b : bookList) if (b->lastOpened > 0) recent.push_back(b);
    std::sort(recent.begin(), recent.end(), [](const Book *a, const Book *b) { return a->lastOpened > b->lastOpened; });
    if (recent.size() > maxCount) recent.resize(maxCount);
    return recent;
}

void BookHandler::refreshFavorites()
{
    authorList[0].bookList.clear();
    for (Book* b : bookList) {
        if(b->favorite)
        {
            authorList[0].bookList.push_back(b);
        }
    }
}

void BookHandler::updateReadPage(const std::string &bookPath, int newPageCount) {
    auto it = indexedBooks.find(bookPath);
    if (it == indexedBooks.end()) return;

    Book *book = it->second;
    book->currentPage = newPageCount;

    saveBook(book); // only update this book's file
}

void BookHandler::reindexBook(Book *book) {
    if (!book) return;

    Reader reader;
    reader.init(book, nullptr);
    reader.indexPages();
    book->renderSettings = book->getCurrentRenderSettings();

    saveBook(book); // only update this book's file
    ESP_LOGI(TAG, "Re-indexed and saved book: %s", book->title.c_str());
}

