#pragma once

#include <Arduino.h>
#include <LittleFS.h>

/**
 * @brief C3FS_Cache - Persistent Binary Caching System for C3OS
 * Provides O(1) random access to cached items for pagination and survives reboots.
 */
class C3FS_Cache {
public:
    static bool init();
    
    /**
     * @brief Saves a batch of items to a persistent binary cache.
     * @param name The cache identifier (e.g., "fm_root", "store", "wifi").
     * @param items Pointer to the data array.
     * @param itemSize Size of a single item in bytes.
     * @param count Number of items.
     * @return true if successful.
     */
    static bool saveCache(const char* name, const void* items, size_t itemSize, int count);
    
    /**
     * @brief Reads a single item from the cache by index (O(1)).
     * @param name Cache identifier.
     * @param index Item index.
     * @param buffer Pointer to memory where item will be copied.
     * @param itemSize Expected size of the item.
     * @return true if item was read.
     */
    static bool readCacheItem(const char* name, int index, void* buffer, size_t itemSize);
    
    /**
     * @brief Returns the number of items in the specified cache.
     */
    static int getCacheCount(const char* name, size_t itemSize);
    
    /**
     * @brief Verifies cache integrity and shows a progress bar.
     * @param name Cache identifier.
     * @return true if cache is healthy.
     */
    static bool refineCache(const char* name, size_t itemSize);
    
    /**
     * @brief Clears a specific cache or all caches if name is NULL.
     */
    static void clearCache(const char* name = nullptr);

    /**
     * @brief Gets total and used space for cache folder.
     */
    static void getCacheMetrics(size_t &used, size_t &totalCount);

private:
    static String getCachePath(const char* name);
    static void drawRefiningBar(int progress);
};
