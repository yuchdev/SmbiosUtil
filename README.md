# SMBIOS Utility

NOTE: If someone can offer UEFI implementation in order for library to work on Mac and newer distros of Linux, I can provide support with the pull request.

The application parses SMBIOS firmware table and reports information about your system's hardware according to the SMBIOS/DMI standard. Typically includes information about BIOS, motherboard, CPU, RAM etc. Application written in C++ and portable, should under Windows/Linux/OS X. Not finished. Pull requests are welcome! 

* smbios - library contains all system calls
* smbios_util - command-line interface
