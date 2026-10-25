#pragma once
#include <array>
using esp_err_t = int;
using gpio_num_t = int;
constexpr int ESP_OK = 0, ESP_ERR_INVALID_STATE = 1, ESP_ERR_INVALID_ARG = 2, ESP_ERR_NO_MEM = 3, GPIO_INTR_ANYEDGE = 4;
enum class MockFailure { None, Type, Add, Enable };
inline MockFailure mockFailure = MockFailure::None;
inline int mockFailedPin = -1, mockInstallResult = ESP_OK;
inline unsigned mockInstalls = 0, mockAdds = 0, mockRemoves = 0;
struct MockHandler {
  void (*fn)(void*) = nullptr;
  void* context = nullptr;
};
inline std::array<MockHandler, 49> mockHandlers;
inline esp_err_t gpio_install_isr_service(int flags) {
  ++mockInstalls;
  return flags == 0 ? mockInstallResult : ESP_ERR_INVALID_ARG;
}
inline esp_err_t gpio_set_intr_type(gpio_num_t pin, int) {
  return pin == mockFailedPin && mockFailure == MockFailure::Type ? ESP_ERR_INVALID_ARG : ESP_OK;
}
inline esp_err_t gpio_isr_handler_add(gpio_num_t pin, void (*fn)(void*), void* context) {
  if (pin == mockFailedPin && mockFailure == MockFailure::Add) return ESP_ERR_NO_MEM;
  ++mockAdds;
  mockHandlers.at(pin) = {fn, context};
  return ESP_OK;
}
inline esp_err_t gpio_intr_enable(gpio_num_t pin) {
  return pin == mockFailedPin && mockFailure == MockFailure::Enable ? ESP_ERR_INVALID_ARG : ESP_OK;
}
inline esp_err_t gpio_isr_handler_remove(gpio_num_t pin) {
  ++mockRemoves;
  mockHandlers.at(pin) = {};
  return ESP_OK;
}
#define GPIO_IS_VALID_GPIO(pin) ((pin) >= 0 && (pin) < 49)
