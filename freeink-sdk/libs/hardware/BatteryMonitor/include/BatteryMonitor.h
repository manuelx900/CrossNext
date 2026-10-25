#pragma once
#include <cstdint>

// FreeInk SDK — battery monitor.
//
// Two backends behind one API (chosen at compile time, so construction and the
// public methods below are identical for both):
//   * ADC (default) — reads a divided LiPo voltage off an ADC pin and maps it to
//     a percentage; an optional charge-status pin (MCP73832 /STAT, active-LOW)
//     drives isCharging().
//   * I2C fuel gauge (FREEINK_BATTERY_I2C_GAUGE) — reads SoC/voltage/charge from a
//     BQ27220 gauge (+ optional BQ25896 charger) over I2C; the ADC pin/divider are
//     ignored. Used by X3 and LilyGo T5 S3. Config comes from
//     BoardConfig::ACTIVE.batteryGauge, and gauge-vs-ADC is chosen at *runtime*
//     (gaugeAddr != 0), so X3 (gauge) and X4 (ADC) work from one C3 binary.
class BatteryMonitor {
public:
    static constexpr int8_t PIN_NONE = -1;

    struct Status {
        bool supported = false;
        bool percentageKnown = false;
        bool millivoltsKnown = false;
        bool chargingKnown = false;
        bool externalPowerKnown = false;
        uint16_t percentage = 0;
        uint16_t millivolts = 0;
        bool charging = false;
        bool externalPower = false;
        // Raw M5PM1 telemetry for diagnostics; -1 when not read (non-PM1 boards
        // or failed I/O). VIN is the DC input rail, 5VINOUT the bidirectional
        // USB-C rail, powerSource the PM1's PWR_SRC bitmap (reg 0x04 [2:0]:
        // bit0=5VIN, bit1=5VINOUT, bit2=BAT).
        int32_t pm1VinMv = -1;
        int32_t pm1VinOutMv = -1;
        int16_t pm1PowerSource = -1;
    };

    // Uses BoardConfig::ACTIVE's battery pins/backend.
    BatteryMonitor();

    // Optional divider multiplier defaults to 2.0; optional charge-status pin
    // defaults to unused.
    explicit BatteryMonitor(int8_t adcPin, float dividerMultiplier = 2.0f, int8_t chargeStatusPin = PIN_NONE);

    // Read voltage and return percentage (0-100)
    uint16_t readPercentage() const;

    // Like readPercentage(), but reports whether the read succeeded. An I2C gauge
    // can fail transiently; on failure this returns false and leaves `out`
    // unchanged, so a caller can keep its last good value. The ADC path always
    // succeeds.
    bool readPercentageChecked(uint16_t& out) const;

    // State of charge at the gauge's FULL resolution: percent in Q8, so the
    // integer part is the high byte and the low byte is 1/256 of a percent.
    //
    // readPercentage() reports whole percent, which on this cell is about
    // 11 mAh per step -- far too coarse to see a sleeping board move at all,
    // and the reason measuring a deep-sleep floor looked like a fortnight's
    // work. The CW2017 carries the fraction in the register directly after the
    // one we already read, so this costs a single extra byte on a bus the call
    // has already opened. Gauges with no fraction register report the integer
    // percent shifted, so the units are the same everywhere.
    //
    // false if the gauge could not be read; `out` is untouched then.
    bool readSocQ8(uint16_t& out) const;

    // Read every battery field the active board can report. `supported` is false
    // when the board profile has no battery telemetry path. Per-field `Known`
    // flags distinguish a valid false/zero value from unsupported or failed I/O.
    Status readStatus() const;

    // Read the battery voltage in millivolts (accounts for divider)
    uint16_t readMillivolts() const;

    // Read the battery voltage in volts (accounts for divider)
    double readVolts() const;

    // True when the battery is actively charging. Sources by backend:
    //   * ADC boards: the MCP73832-style charge-status pin (LOW = charging);
    //     always false when no charge-status pin is configured.
    //   * Gauge boards: a charger IC's status (BQ25896 CHRG_STAT) when present,
    //     else the gauge's own Current() sign (BQ27220, positive = charging), so
    //     a board with a gauge but no charger IC (e.g. X3) still reports it.
    bool isCharging() const;

    // True when external power (a USB cable or charger) is physically present.
    //
    // This is NOT isCharging(): a full battery stops charging while still
    // plugged in, so isCharging() goes false with the cable still attached.
    // Only a source that reports the input rail itself can answer this, so it
    // is currently the BQ25896's REG0B — VBUS_STAT[7:5] plus PG_STAT — and
    // nothing else. Boards without that charger IC cannot observe it.
    //
    // `known` (optional, out) is set false when the board has no way to tell,
    // or the read failed. Callers MUST branch on it: a bare false means "no
    // opinion" just as often as it means "unplugged", and treating the two the
    // same is how you get a device that thinks the cable is gone because it
    // never had a way to see it.
    bool isExternalPowerPresent(bool* known = nullptr) const;

    // Percentage from a millivolt value, off a standard 1S Li-ion discharge
    // curve. The result is always a multiple of 10: voltage cannot resolve a
    // Li-ion pack any finer than that, and pretending otherwise just produces a
    // number that wanders while the battery sits still. 0 mV (a failed read)
    // maps to 0%, not 100%.
    static uint16_t percentageFromMillivolts(uint16_t millivolts);

    // Same lookup, with a deadband around the notch boundaries so a reading that
    // hovers on a boundary keeps reporting `previousPercent` instead of
    // oscillating. Pass the last value this returned; pass > 100 for no history.
    static uint16_t percentageFromMillivolts(uint16_t millivolts, uint16_t previousPercent);

    // Loads batteryGauge.designCapacityMah into a BQ27220 whose Design Capacity reads
    // TI's 3000 mAh default (it returns there whenever the gauge loses power) or whose
    // FullChargeCapacity() reads more than a quarter above it. One short step per call,
    // about 8 s in all: call from the task that owns the gauge until it returns false.
    static bool loadDesignCapacity();

private:
    bool hasAdcBackend() const;
    bool hasGaugeBackend() const;
    bool hasM5Pm1Backend() const;
    bool readM5Pm1Status(Status& status) const;

    int8_t _adcPin;
    float _dividerMultiplier;
    int8_t _chargeStatusPin;
};
