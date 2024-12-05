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
├── test                    # Test files (alternatively `spec` or `tests`)
│   ├── benchmarks          # Load and stress tests
│   ├── integration         # End-to-end, integration tests (alternatively `e2e`)
│   └── unit                # Unit tests
└── README.md
```
