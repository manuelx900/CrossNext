#pragma once

#include <FreeInkUI.h>

namespace keyboardview {
namespace fui = freeink::ui;
constexpr fui::KeyboardKey symbol(const char* text, int16_t value, const char* alternate = nullptr) {
  return {text, text, fui::KeyKind::Normal, fui::StateNormal, value, 1, true, alternate};
}
inline constexpr fui::KeyboardKey digits[] = {symbol("1", '1'), symbol("2", '2'), symbol("3", '3'), symbol("4", '4'),
                                              symbol("5", '5'), symbol("6", '6'), symbol("7", '7'), symbol("8", '8'),
                                              symbol("9", '9'), symbol("0", '0')};
inline constexpr fui::KeyboardKey punctuation[] = {
    symbol("-", '-'), symbol("/", '/'), symbol(":", ':'), symbol(";", ';'), symbol("(", '('),
    symbol(")", ')'), symbol("$", '$'), symbol("&", '&'), symbol("@", '@'), symbol("\"", '"')};
inline constexpr fui::KeyboardKey symbolsFirst[] = {
    symbol("[", '['), symbol("]", ']'), symbol("{", '{'), symbol("}", '}'), symbol("#", '#'),
    symbol("%", '%'), symbol("^", '^'), symbol("*", '*'), symbol("+", '+'), symbol("=", '=')};
inline constexpr fui::KeyboardKey symbolsSecond[] = {
    symbol("_", '_'), symbol("\\", '\\'),  symbol("|", '|'),  symbol("~", '~', "`"), symbol("<", '<'),
    symbol(">", '>'), symbol("€", 0x20ac), symbol("£", 0xa3), symbol("¥", 0xa5),     symbol("•", 0x2022)};
inline constexpr fui::KeyboardKey numberLast[] = {
    {"#+=", nullptr, fui::KeyKind::Shift, fui::StateNormal, fui::QWERTY_KEY_SHIFT, 2},
    symbol(".", '.'),
    symbol(",", ','),
    symbol("?", '?'),
    symbol("!", '!'),
    symbol("'", '\''),
    {nullptr, nullptr, fui::KeyKind::Delete, fui::StateNormal, fui::QWERTY_KEY_BACKSPACE, 2}};
inline constexpr fui::KeyboardKey symbolLast[] = {
    {"123", nullptr, fui::KeyKind::Shift, fui::StateNormal, fui::QWERTY_KEY_SHIFT, 2},
    symbol(".", '.'),
    symbol(",", ','),
    symbol("?", '?'),
    symbol("!", '!'),
    symbol("'", '\''),
    {nullptr, nullptr, fui::KeyKind::Delete, fui::StateNormal, fui::QWERTY_KEY_BACKSPACE, 2}};
inline constexpr fui::KeyboardKey symbolBottom[] = {
    {"ABC", nullptr, fui::KeyKind::Mode, fui::StateNormal, fui::QWERTY_KEY_MODE, 2},
    {"", " ", fui::KeyKind::Space, fui::StateNormal, fui::QWERTY_KEY_SPACE, 5},
    symbol(".", 4096),
    {nullptr, nullptr, fui::KeyKind::Ok, fui::StateNormal, fui::QWERTY_KEY_ENTER, 2}};
inline constexpr fui::KeyboardKey symbolLanguageBottom[] = {
    {"ABC", nullptr, fui::KeyKind::Mode, fui::StateNormal, fui::QWERTY_KEY_MODE, 2},
    {nullptr, nullptr, fui::KeyKind::Lang, fui::StateNormal, fui::QWERTY_KEY_LANG},
    {"", " ", fui::KeyKind::Space, fui::StateNormal, fui::QWERTY_KEY_SPACE, 4},
    symbol(".", 4096),
    {nullptr, nullptr, fui::KeyKind::Ok, fui::StateNormal, fui::QWERTY_KEY_ENTER, 2}};
inline constexpr fui::KeyboardRow numberRows[] = {{digits, 10}, {punctuation, 10}, {numberLast, 7}, {symbolBottom, 4}};
inline constexpr fui::KeyboardRow symbolRows[] = {
    {symbolsFirst, 10}, {symbolsSecond, 10}, {symbolLast, 7}, {symbolBottom, 4}};
inline constexpr fui::KeyboardRow numberLanguageRows[] = {
    {digits, 10}, {punctuation, 10}, {numberLast, 7}, {symbolLanguageBottom, 5}};
inline constexpr fui::KeyboardRow symbolLanguageRows[] = {
    {symbolsFirst, 10}, {symbolsSecond, 10}, {symbolLast, 7}, {symbolLanguageBottom, 5}};
inline constexpr fui::KeyboardLayout numberLayout{numberRows, 4}, symbolLayout{symbolRows, 4},
    numberLanguageLayout{numberLanguageRows, 4}, symbolLanguageLayout{symbolLanguageRows, 4};
inline const fui::KeyboardLayout& symbols(bool second, bool languageKey) {
  return languageKey ? (second ? symbolLanguageLayout : numberLanguageLayout) : (second ? symbolLayout : numberLayout);
}
}  // namespace keyboardview
