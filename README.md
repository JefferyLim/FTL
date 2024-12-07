# Follow the Light

## Installation

Install the [Arduino IDE](https://support.arduino.cc/hc/en-us/articles/360019833020-Download-and-install-Arduino-IDE) and the [Teensyduino](https://www.pjrc.com/teensy/td_download.html) add-on.

FTL utilizes the following libraries. Utilize the `Import ZIP Library` feature on the Arduino IDE.

[RingBuffer Library](https://github.com/Locoduino/RingBuffer)

[ADC Library](https://github.com/pedvide/ADC)

[CRC](https://github.com/RobTillaart/CRC)

## Architecture

[insert image of system]

### Project Structure

```
.
├── ...
├── documentation           # Documentation
│   ├── drawio              # drawio diagrams
├── modeling                # modeling
├── software                # software
│   ├── adc_test            # initial testing for ADCs
│   ├── irled_tests         # initial testing for the IR LEDs
│   └── receiver            # receiver code
│   └── transmitter         # transmitter code
└── README.md
```
