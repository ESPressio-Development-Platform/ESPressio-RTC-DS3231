#include <array>
#include <cassert>
#include <cstdint>
#include "ESPressio_RTC_DS3231.hpp"

using namespace ESPressio;

struct Bus {
    std::array<std::uint8_t, 256> r{};
    bool Read(std::uint8_t, std::uint8_t reg, std::uint8_t* data, std::size_t n) noexcept {
        for (std::size_t i=0;i<n;++i) { data[i]=r[reg+i]; }
        return true;
    }
    bool Write(std::uint8_t, std::uint8_t reg, const std::uint8_t* data, std::size_t n) noexcept {
        for (std::size_t i=0;i<n;++i) { r[reg+i]=data[i]; }
        return true;
    }
};

using Device = RTC::DS3231::Device<Bus>;
static_assert(RTC::IsRealTimeClockV<Device>);
static_assert(Platform::Clock::IsClockSourceV<RTC::DS3231::Clock32k>);
static_assert(Platform::Clock::FrequencyHz<RTC::DS3231::Clock32k> == 32768U);
static_assert(Platform::Clock::FrequencyHz<RTC::DS3231::Clock1Hz> == 1U);
static_assert(Platform::Clock::FrequencyHz<RTC::DS3231::Clock8192Hz> == 8192U);

int main() {
    Bus bus;
    Device rtc(bus);
    RTC::DateTime in{2026,9,12,16,45,30,6};
    assert(rtc.Write(in) == RTC::Result::Ok);
    RTC::Reading out{};
    assert(rtc.Read(out) == RTC::Result::Ok);
    assert(out.Value.Year == 2026 && out.Value.Hour == 16 && out.Validity == RTC::TimeValidity::Valid);

    bus.r[0x0F] |= 0x80;
    assert(rtc.Read(out) == RTC::Result::Ok);
    assert(out.Validity == RTC::TimeValidity::OscillatorStopped);

    bus.r[0x11] = 25;
    bus.r[0x12] = 0x40;
    std::int16_t temp{};
    assert(rtc.ReadTemperatureCentiCelsius(temp) == RTC::Result::Ok);
    assert(temp == 2525);

    RTC::DS3231::Clock32k clock;
    clock.OnTickFromInterrupt();
    assert(clock.Now() == 1U);
}
