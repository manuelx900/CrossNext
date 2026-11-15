#pragma once

namespace petart {
inline constexpr unsigned REGISTERED_RECORDS = 1014;
inline constexpr unsigned EXTRA_FOOD_OFFSET = REGISTERED_RECORDS;
inline constexpr unsigned COMPANION_OFFSET = EXTRA_FOOD_OFFSET + 3 * 4 * 2;
inline constexpr unsigned ART_COUNT = COMPANION_OFFSET + 3 * 3 * 4 * 2 * 2;
}  // namespace petart
