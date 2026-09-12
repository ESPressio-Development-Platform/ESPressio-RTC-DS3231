# ESPressio-RTC-DS3231

Platform-independent DS3231 implementation for `ESPressio-RTC`.

Implemented facilities:

- BCD calendar read/write in 24-hour form;
- century handling with conservative supported writes from 2000 through 2100;
- oscillator-stop validity reporting and clearing;
- temperature readout in centi-degrees Celsius;
- aging-offset access;
- programmable INT/SQW frequency;
- dedicated 32.768 kHz output enable;
- fixed-frequency `ESPressio-Platform::Clock` aliases sourced from the DS3231 hardware outputs.

The driver is templated over the `ESPressio-RTC` register-bus contract and contains no Arduino, ESP-IDF or FreeRTOS dependency.

## Calendar range and oscillator outputs

The implementation accepts calendar writes from 2000 through 2100. This is deliberately conservative: Analog Devices guarantees the DS3231 automatic leap-year compensation through 2100. The device's century bit is still decoded on reads.

The dedicated 32 kHz output and the programmable INT/SQW rates (1 Hz, 1.024 kHz, 4.096 kHz and 8.192 kHz) have matching `ExternalTickClock` aliases. An application must configure the hardware output to the same frequency represented by the alias. Servicing every 32.768 kHz edge in software may be inappropriate on constrained MCUs; a hardware pulse counter/capture peripheral can be used by a future provider without changing the Platform Clock contract.
