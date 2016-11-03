# DisasVisor

### Overview

DisasVisor is a hypervisor for displaying disaster notification messages on computer screens.
DisasVisor is implemented by modifying BitVisor.

### How to build

See BitVisor manuals and documents for details.

1. `cd DisasVisor/bitvisor`.
2. Configure `defconfig` file according your environment.  Replace IP addresses written in the file.
3. `make`
4. Copy the resulting file `bitvisor.elf' to the directory where kernel image files are placed.
5. Configure the boot loader to allow it to boot `bitvisor.elf` and the original operating system(s).

### How to run

#### Hypervisor

1. Boot `bitvisor.elf` from the boot loader.
2. When the boot loader runs again (on BitVisor), boot an operating system.

#### Intermediate server


### Resources


