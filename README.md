# SMBIOS Utility

UPDATE: the UEFI implementation asked for below has landed - macOS now reads SMBIOS natively via IOKit/IORegistry, and Linux reads it via sysfs (`/sys/firmware/dmi/tables`) then the UEFI systab, neither needing root. Both still fall back to the legacy physical-memory scan if the native path is unavailable.

The application parses SMBIOS firmware table and reports information about your system's hardware according to the SMBIOS/DMI standard. Typically includes information about BIOS, motherboard, CPU, RAM etc. Application written in C++ and portable, should under Windows/Linux/OS X. Not finished. Pull requests are welcome! 

* libsmbios - library contains all system calls
* smbios - command-line interface
