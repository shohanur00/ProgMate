# ProgMate PM-01

### Standalone STM32 Programmer

**Your Firmware. Any Target.**

ProgMate is a standalone STM32 programming device designed to store firmware uploaded from a PC and program supported STM32 microcontrollers through the Serial Wire Debug (SWD) interface—without requiring a PC during target programming.

The project aims to combine firmware storage, target programming, verification, and a simple user interface into one compact embedded device.

> **Project Status:** In Development

---

## Features

* **USB Firmware Upload** — Transfer firmware from a PC to the programmer.
* **Standalone Operation** — Program supported targets without a PC connected.
* **SWD Programming Interface** — Communicate with target microcontrollers through SWD.
* **External Firmware Storage** — Store firmware images in SPI NOR Flash.
* **OLED User Interface** — Navigate firmware files and programming options.
* **Three-Button Navigation** — Operate the device through physical controls.
* **Target Power Management** — Planned selectable, current-limited 3.3 V output.
* **Firmware Verification** — Read back and verify programmed data where supported.
* **Expandable Architecture** — Designed to accommodate additional STM32 device families.

*Feature availability depends on implementation and hardware validation.*

## Hardware Architecture

| Component              | Planned implementation       |
| ---------------------- | ---------------------------- |
| Main MCU               | STM32G431CBT6                |
| Firmware storage       | External SPI NOR Flash       |
| PC connection          | USB-C, USB Full-Speed        |
| Target interface       | SWD                          |
| Display                | I²C OLED                     |
| User controls          | 3 push buttons               |
| Target power           | Current-limited 3.3 V supply |
| Target voltage sensing | VTref input                  |

The main MCU handles the user interface, firmware management, communication, and programming control. External Flash provides storage for firmware images, while the SWD interface connects ProgMate to the target device.

## System Workflow

1. Connect ProgMate to a PC through USB.
2. Upload a firmware image to the device.
3. Store and validate the uploaded firmware.
4. Disconnect the PC and connect the target MCU through SWD.
5. Select the firmware and initiate programming.
6. Erase, program, and verify the target using the appropriate device-specific algorithm.
7. Display the programming result on the OLED.

## Firmware Support Strategy

ProgMate is intended to support multiple STM32 families through a modular programming architecture.

However, **SWD communication alone does not provide universal flash programming**. Each supported MCU may require device identification, flash-controller handling, erase/program sequences, and protection-state management.

The initial development will focus on validating the complete programming workflow with a specific STM32 target before expanding compatibility.

## Development Environment

The embedded firmware project is based on:

* STM32Cube toolchain
* C and ARM assembly
* CMake
* Ninja build system
* ARM GNU Toolchain
* Visual Studio Code

The project uses CMake to configure and build the firmware. Post-build commands generate `.bin` and `.hex` files for firmware distribution and programming.

## Repository Structure

The directory structure may evolve as development progresses.

```text
ProgMate/
├── CMakeLists.txt
├── CMakePresets.json
├── Core/
├── Drivers/
├── Middlewares/
├── Application/
├── Bootloader/
├── Hardware/
├── Docs/
└── README.md
```

*Adjust this structure to match the actual repository.*

## Development Roadmap

* [ ] Finalize hardware architecture and schematic
* [ ] Validate USB firmware upload
* [ ] Implement external Flash storage
* [ ] Develop OLED and button interface
* [ ] Validate SWD communication
* [ ] Implement programming for the first target MCU
* [ ] Add erase, program, and verification routines
* [ ] Implement target power control and fault reporting
* [ ] Expand supported MCU families
* [ ] Complete hardware testing and documentation

## Safety and Compatibility

* Verify target voltage and SWD pin orientation before connecting.
* Avoid simultaneously powering a target from ProgMate and an external supply unless the power architecture explicitly supports it.
* Use only firmware built for the intended target device and memory layout.
* Device support will be documented as each target is implemented and validated.

## Project Goal

The goal of ProgMate is to make firmware deployment more convenient for embedded developers, electronics engineers, and hardware prototyping workflows by providing a compact, reusable, standalone programming tool.

## License

License: To be decided.

---

**ProgMate PM-01**
*Your Firmware. Any Target.*
