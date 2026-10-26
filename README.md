# Ventoy EFI Tool

### Create a Ventoy Bootable USB Directly from EFI — Without Booting into an Operating System

**Ventoy EFI Tool** is a project designed to enable the creation of a Ventoy bootable USB drive directly from the EFI/UEFI environment, without requiring the user to boot into Windows, Linux, or another installed operating system.

The project aims to simplify the USB preparation process at the pre-boot level, making it useful for system installation, maintenance, troubleshooting, and recovery scenarios.

## 🚀 Project Overview

Normally, creating a Ventoy bootable USB requires running a supported operating system and using the Ventoy installation utility.

The goal of this project is to provide an alternative approach by allowing users to prepare a Ventoy disk directly from the EFI environment.

This can be especially useful when:
- No operating system is installed on the computer.
- The existing operating system is corrupted or inaccessible.
- A bootable USB needs to be prepared before installing an OS.
- System maintenance or recovery must be performed outside the operating system.

## ✨ Features

- **EFI-Based Operation:** Designed to run in the pre-boot EFI/UEFI environment.
- **No OS Required:** Aims to create a Ventoy disk without booting into Windows or Linux.
- **USB Preparation:** Intended to prepare a USB drive for Ventoy-based booting.
- **System Recovery:** Potentially useful for recovery and troubleshooting workflows.
- **OS Installation Support:** Helps prepare bootable installation media when the required functionality is implemented.
- **Standalone Workflow:** Aims to minimize dependence on an installed operating system.

## 🎯 Project Goal

The primary goal is to make Ventoy USB creation possible directly from the EFI environment, providing an alternative to traditional OS-based installation methods.

## 🖥️ Requirements

Requirements depend on the current implementation. The expected environment may include:

- A computer with compatible UEFI firmware.
- An EFI-compatible executable or boot application.
- A supported USB storage device.
- The necessary Ventoy files and installation components.
- Appropriate firmware and storage-device access.

## 📖 Usage

The exact procedure depends on the version and implementation of this tool.

The intended workflow is:

1. Prepare the EFI tool and its required files.
2. Launch the tool from a compatible EFI/UEFI environment.
3. Select the target USB storage device, if supported.
4. Run the Ventoy disk preparation process.
5. Verify that the USB drive is correctly prepared before using it to boot ISO images.

**Important:** These steps describe the intended workflow. Actual functionality depends on the features implemented in the current version.

## ⚠️ Disclaimer

- Creating or preparing a Ventoy disk may erase existing data on the selected USB drive.
- Always verify the target storage device before performing disk operations.
- EFI firmware and hardware compatibility may vary.
- This project is intended for educational, development, and system-maintenance purposes.
- This project is not an official Ventoy product unless explicitly stated by its maintainers.

## 🛠️ Development Status

This repository documents an EFI-based approach to Ventoy disk creation.

Implementation details, supported devices, compatibility, and available features should be verified against the current source code and release.

## 🤝 Contributing

Contributions, bug reports, feature suggestions, and compatibility testing are welcome.

If you would like to contribute, please open an issue or submit a pull request.

## 📜 License

GNU GENERAL PUBLIC LICENSE
