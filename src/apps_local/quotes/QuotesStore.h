#pragma once
#include "QuotesCore.h"
namespace quotes {
enum class Load { Missing, Ok, Error };
Load loadFlags(Entry*, int);
bool saveFlags(const Entry*, int);
}  // namespace quotes
