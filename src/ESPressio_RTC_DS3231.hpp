#pragma once

#include <cstdint>

#include <ESPressio_RTC.hpp>

namespace ESPressio::RTC::DS3231 {

inline constexpr std::uint8_t I2CAddress = 0x68U;

/** Provider provenance for DS3231-backed services. */
struct Origin final : ESPressio::Platform::Backend {};

enum class SquareWaveFrequency : std::uint8_t {
    Hz1 = 0,
    Hz1024 = 1,
    Hz4096 = 2,
    Hz8192 = 3
};

/**
 * DS3231 civil-time/calendar driver.
 *
 * The bus is structural and therefore independent of Arduino Wire, ESP-IDF,
 * Linux, or any future ESPressio I2C abstraction.
 */
template <typename TBus>
class Device final
    : public RTC::DeviceProviderDeclaration<
          Origin,
          ESPressio::Platform::CapabilitySet<
              RTC::Capability::Temperature,
              RTC::Capability::ClockOutput>> {
public:
    explicit constexpr Device(TBus& bus) noexcept : registers_(bus) {}

    Result Read(Reading& reading) noexcept {
        std::uint8_t data[7]{};
        auto result = registers_.Read(0x00U, data, sizeof(data));
        if (result != Result::Ok) return result;

        DateTime value{};
        value.Second = FromBcd(static_cast<std::uint8_t>(data[0] & 0x7FU));
        value.Minute = FromBcd(static_cast<std::uint8_t>(data[1] & 0x7FU));
        value.Hour = DecodeHour(data[2]);
        const auto rawWeekday = static_cast<std::uint8_t>(data[3] & 0x07U);
        value.Weekday = rawWeekday >= 1U ? static_cast<std::uint8_t>(rawWeekday - 1U) : 0U;
        value.Day = FromBcd(static_cast<std::uint8_t>(data[4] & 0x3FU));
        value.Month = FromBcd(static_cast<std::uint8_t>(data[5] & 0x1FU));
        value.Year = static_cast<std::uint16_t>(2000U + FromBcd(data[6]) +
                                                ((data[5] & 0x80U) != 0U ? 100U : 0U));
        if (!RTC::IsValid(value)) return Result::InvalidDateTime;

        std::uint8_t status{};
        result = registers_.ReadByte(0x0FU, status);
        if (result != Result::Ok) return result;

        reading.Value = value;
        reading.Validity = (status & 0x80U) != 0U
                               ? TimeValidity::OscillatorStopped
                               : TimeValidity::Valid;
        return Result::Ok;
    }

    Result Write(const DateTime& value) noexcept {
        if (!RTC::IsValid(value) || value.Year < 2000U || value.Year > 2100U)
            return Result::InvalidDateTime;

        const auto yearInCentury = static_cast<std::uint8_t>(value.Year % 100U);
        std::uint8_t data[7]{
            ToBcd(value.Second),
            ToBcd(value.Minute),
            ToBcd(value.Hour),
            static_cast<std::uint8_t>(value.Weekday + 1U),
            ToBcd(value.Day),
            static_cast<std::uint8_t>(ToBcd(value.Month) | (value.Year >= 2100U ? 0x80U : 0U)),
            ToBcd(yearInCentury)
        };

        auto result = registers_.Write(0x00U, data, sizeof(data));
        if (result != Result::Ok) return result;
        return ClearOscillatorStopFlag();
    }

    Result ClearOscillatorStopFlag() noexcept {
        return registers_.UpdateBits(0x0FU, 0x80U, 0x00U);
    }

    Result Enable32kHzOutput(bool enabled) noexcept {
        return registers_.UpdateBits(0x0FU, 0x08U, enabled ? 0x08U : 0x00U);
    }

    Result ConfigureSquareWave(SquareWaveFrequency frequency,
                               bool runOnBattery = false) noexcept {
        std::uint8_t control{};
        auto result = registers_.ReadByte(0x0EU, control);
        if (result != Result::Ok) return result;

        control = static_cast<std::uint8_t>(control & static_cast<std::uint8_t>(~0x5CU));
        control = static_cast<std::uint8_t>(control |
                    (static_cast<std::uint8_t>(frequency) << 3U));
        if (runOnBattery) control = static_cast<std::uint8_t>(control | 0x40U);
        // INTCN=0 selects the square-wave function on INT/SQW.
        return registers_.WriteByte(0x0EU, control);
    }

    Result ReadTemperatureCentiCelsius(std::int16_t& value) noexcept {
        std::uint8_t data[2]{};
        const auto result = registers_.Read(0x11U, data, sizeof(data));
        if (result != Result::Ok) return result;
        const auto whole = static_cast<std::int8_t>(data[0]);
        const auto quarter = static_cast<std::int16_t>(data[1] >> 6U);
        value = static_cast<std::int16_t>((static_cast<std::int16_t>(whole) * 4 + quarter) * 25);
        return Result::Ok;
    }

    Result ReadAgingOffset(std::int8_t& value) noexcept {
        std::uint8_t raw{};
        const auto result = registers_.ReadByte(0x10U, raw);
        value = static_cast<std::int8_t>(raw);
        return result;
    }

    Result WriteAgingOffset(std::int8_t value) noexcept {
        return registers_.WriteByte(0x10U, static_cast<std::uint8_t>(value));
    }

private:
    static constexpr std::uint8_t DecodeHour(std::uint8_t value) noexcept {
        if ((value & 0x40U) == 0U)
            return FromBcd(static_cast<std::uint8_t>(value & 0x3FU));
        const auto hour12 = FromBcd(static_cast<std::uint8_t>(value & 0x1FU));
        const bool pm = (value & 0x20U) != 0U;
        return static_cast<std::uint8_t>((hour12 % 12U) + (pm ? 12U : 0U));
    }

    RTC::RegisterDevice<TBus, I2CAddress> registers_;
};

/** ISR-counted Platform Clock aliases for DS3231 hardware clock outputs.
 *
 * The application must configure the matching physical output frequency and
 * forward one selected edge per period to OnTickFromInterrupt(). For the
 * dedicated 32 kHz pin use Enable32kHzOutput(true); the lower-frequency aliases
 * correspond to the programmable INT/SQW output. At higher frequencies a
 * hardware counter/capture peripheral is generally preferable to servicing an
 * interrupt for every edge.
 */
using Clock1Hz = RTC::ExternalTickClock<Origin, 1U>;
using Clock1024Hz = RTC::ExternalTickClock<Origin, 1024U>;
using Clock4096Hz = RTC::ExternalTickClock<Origin, 4096U>;
using Clock8192Hz = RTC::ExternalTickClock<Origin, 8192U>;
using Clock32k = RTC::ExternalTickClock<
    Origin,
    32768U,
    32U,
    ESPressio::Platform::CapabilitySet<
        ESPressio::Platform::Capability::HighResolutionClock>>;

} // namespace ESPressio::RTC::DS3231
