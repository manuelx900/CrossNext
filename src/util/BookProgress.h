#pragma once

#include <string>

// Saved reading percentage, or -1 when unavailable. Never builds reading caches.
int loadBookProgress(const std::string& path);
// Activity/render-owned memoization; call only after a successful progress commit.
void publishBookProgress(const std::string& path, int percent);
