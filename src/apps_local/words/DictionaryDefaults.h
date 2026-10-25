#pragma once

class CrossPointSettings;

namespace dictionarydefaults {

// Fill unassigned automatic slots from installed EN/ES resource folders.
// Existing names and explicit None are preserved. Returns whether names changed.
bool apply(CrossPointSettings& settings);

}  // namespace dictionarydefaults
