# Follow the Light

Follow the Light (FTL) is the final project for ENG EC545. The goal of this project was to explore the different course topics. Our team decided to explore the ideas of a IR-based communication project. Taking inspiration from TeraByte InfraRed Delivery ([TBIRD](https://www.ll.mit.edu/sites/default/files/other/doc/2023-02/TVO_Technology_Highlight_12_TBird.pdf)), we wanted to create a system that would transmit data to a receiver. The receiver, would utilize the sensor data to track and follow the transmitter.

## Installation

Install the [Arduino IDE](https://support.arduino.cc/hc/en-us/articles/360019833020-Download-and-install-Arduino-IDE) and the [Teensyduino](https://www.pjrc.com/teensy/td_download.html) add-on.

FTL utilizes the following libraries. Utilize the `Import ZIP Library` feature on the Arduino IDE.

[RingBuffer Library](https://github.com/Locoduino/RingBuffer)

[ADC Library](https://github.com/pedvide/ADC)

[CRC](https://github.com/RobTillaart/CRC)

## Architecture

### Transmitter
![transmitter_system drawio](https://github.com/user-attachments/assets/45543223-8e2f-48b6-bc26-d829aa6363bd)
The transmitter can be set in one of three modes:
1. LED Mode - A simple 50% duty cycle square wave used mostly for testing.
2. Message Mode - Takes a user's input, and transmit the message out.
3. Counter Mode - Increments a counter, and transmits the number. 

The transmitter works similarly to UART. It has a start bit, 8 bit preamble, 8 bit payload, and 8 bit CRC. The payload could be longer, but we chose to use 8 bits in order to debug it. 

### Receiver

![reciever_system drawio](https://github.com/user-attachments/assets/201bb00c-31d3-4361-9fd7-11cc45c7dde6)

The receiver is comprised of three subsystems:
1. ADC Sampling - Samples the 3 photodiodes at 5x the transmit rate
2. Message Parsing - Parses 5 samples at a time to determine a `1` or a `0` bit
3. Motor Control - Moves the motor to maintain sight to the transmitter

We oversample the recieving signal in order to conduct a vote on the value of a bit. A specific part of the hardware is unable to meet high throughput, causing major skewing in the signal when the signal goes from 0 to 1. 

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
