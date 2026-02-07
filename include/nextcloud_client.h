/*
 *  nextcloud_client.h - Nextcloud WebDAV Backup Client
 *
 *  Part of HB-RF-ETH-ng firmware v2.1
 *  Copyright 2025 Xerolux
 *
 *  Provides automatic backup functionality to Nextcloud via WebDAV protocol.
 */

#ifndef NEXTCLOUD_CLIENT_H
#define NEXTCLOUD_CLIENT_H

#include "esp_err.h"
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Configuration structure for Nextcloud backup
typedef struct {
    bool enabled;                   // Enable automatic backups
    char server_url[256];           // Nextcloud server URL (e.g., "https://cloud.example.com")
    char username[64];              // Nextcloud username
    char password[128];             // Nextcloud app password
    char backup_path[128];          // Remote path (e.g., "/HB-RF-ETH/backups")
    uint32_t backup_interval_hours; // Backup interval in hours (default: 24)
    bool keep_local_backup;         // Also save backup locally
} nextcloud_config_t;

/**
 * Initialize Nextcloud backup client
 * @param config Nextcloud configuration
 * @return ESP_OK on success
 */
esp_err_t nextcloud_init(const nextcloud_config_t *config);

/**
 * Upload backup to Nextcloud
 * @param json_data Backup JSON data
 * @param json_len Length of JSON data
 * @param filename Remote filename (e.g., "backup_2025-01-15.json")
 * @return ESP_OK on success
 */
esp_err_t nextcloud_upload_backup(const char *json_data, size_t json_len, const char *filename);

/**
 * Download backup from Nextcloud
 * @param filename Remote filename to download
 * @param out_data Pointer to receive allocated buffer (caller must free)
 * @param out_len Pointer to receive data length
 * @return ESP_OK on success
 */
esp_err_t nextcloud_download_backup(const char *filename, char **out_data, size_t *out_len);

/**
 * List available backups on Nextcloud
 * @param out_list Pointer to receive allocated buffer with filenames (caller must free)
 * @param out_count Pointer to receive number of backups
 * @return ESP_OK on success
 */
esp_err_t nextcloud_list_backups(char ***out_list, size_t *out_count);

/**
 * Test connection to Nextcloud server
 * @return ESP_OK if connection successful
 */
esp_err_t nextcloud_test_connection(void);

/**
 * Start automatic backup task
 * @return ESP_OK on success
 */
esp_err_t nextcloud_start_auto_backup(void);

/**
 * Stop automatic backup task
 */
void nextcloud_stop_auto_backup(void);

/**
 * Get last backup status
 * @param out_timestamp Pointer to receive last backup timestamp
 * @param out_success Pointer to receive last backup success status
 * @return ESP_OK if backup history available
 */
esp_err_t nextcloud_get_last_backup_status(time_t *out_timestamp, bool *out_success);

#ifdef __cplusplus
}
#endif

#endif // NEXTCLOUD_CLIENT_H
