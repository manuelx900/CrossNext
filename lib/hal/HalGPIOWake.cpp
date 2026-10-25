#include <HalGPIO.h>
#include <Logging.h>
#include <driver/gpio.h>

#include <algorithm>

#if FREEINK_DEVICE_X4PRO || FREEINK_DEVICE_STICKY
void IRAM_ATTR HalGPIO::signalActivityFromIsr(void* context) {
  auto* self = static_cast<HalGPIO*>(context);
  BaseType_t higherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(self->activitySemaphore, &higherPriorityTaskWoken);
  if (higherPriorityTaskWoken == pdTRUE) portYIELD_FROM_ISR();
}

void HalGPIO::attachActivityInterrupts() {
  activitySemaphore = xSemaphoreCreateBinaryStatic(&activitySemaphoreStorage);
  if (!activitySemaphore) {
    LOG_ERR("GPIO", "Failed to create static activity semaphore; retaining input polling");
    return;
  }
  // Match Arduino's flags: the shared display dispatcher may reside in flash.
  const esp_err_t installed = gpio_install_isr_service(ARDUINO_ISR_FLAG);
  if (installed != ESP_OK && installed != ESP_ERR_INVALID_STATE) {
    LOG_ERR("GPIO", "Failed to install GPIO ISR service (%d); retaining input polling", installed);
    return;
  }

  const auto& input = BoardConfig::ACTIVE.input;
  const int8_t pins[] = {input.back,
                         input.confirm,
                         input.left,
                         input.right,
                         input.up,
                         input.down,
                         input.power,
                         BoardConfig::ACTIVE.touch.irq,
                         BoardConfig::ACTIVE.usbDetect,
                         BoardConfig::ACTIVE.batteryChargeStatus};
  uint64_t attached = 0;
  for (const int8_t pin : pins) {
    if (pin < 0 || (pin < 64 && (attached & (uint64_t{1} << pin)))) continue;
    const auto gpioPin = static_cast<gpio_num_t>(pin);
    esp_err_t error = GPIO_IS_VALID_GPIO(pin) ? gpio_set_intr_type(gpioPin, GPIO_INTR_ANYEDGE) : ESP_ERR_INVALID_ARG;
    if (error == ESP_OK) error = gpio_isr_handler_add(gpioPin, &HalGPIO::signalActivityFromIsr, this);
    if (error == ESP_OK) {
      attached |= uint64_t{1} << pin;
      error = gpio_intr_enable(gpioPin);
    }
    if (error != ESP_OK) {
      LOG_ERR("GPIO", "Failed to install wake IRQ on GPIO%d (%d); retaining input polling", pin, error);
      for (int previous = 0; previous < 64; ++previous) {
        if (attached & (uint64_t{1} << previous)) gpio_isr_handler_remove(static_cast<gpio_num_t>(previous));
      }
      return;
    }
  }
  activityInterruptsReady = attached != 0;
}
#endif

void HalGPIO::signalActivity() {
#if FREEINK_DEVICE_X4PRO || FREEINK_DEVICE_STICKY
  if (activitySemaphore) xSemaphoreGive(activitySemaphore);
#endif
}

void HalGPIO::waitForActivity(const uint32_t timeoutMs) {
#if FREEINK_DEVICE_X4PRO || FREEINK_DEVICE_STICKY
  if (activityInterruptsReady) {
    const TickType_t ticks = timeoutMs == 0 ? 0 : std::max<TickType_t>(1, pdMS_TO_TICKS(timeoutMs));
    xSemaphoreTake(activitySemaphore, ticks);
    if (timeoutMs == 0) yield();
    return;
  }
#endif
  delay(std::min<uint32_t>(timeoutMs, 10));
}

uint32_t HalGPIO::nextInputServiceDelayMs(const uint32_t maximum) const {
#if FREEINK_DEVICE_X4PRO || FREEINK_DEVICE_STICKY
  if (activityInterruptsReady) return inputMgr.nextServiceDelayMs(maximum);
#endif
  return std::min<uint32_t>(maximum, 10);
}
