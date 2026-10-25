#pragma once
constexpr int SNTP_SYNC_STATUS_COMPLETED = 1;
inline bool esp_sntp_enabled() { return false; }
inline void esp_sntp_stop() {}
inline int sntp_get_sync_status() { return 0; }
