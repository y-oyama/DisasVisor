# DisasVisor

### Overview

DisasVisor is a hypervisor for displaying disaster notification messages on computer screens.
It is implemented by modifying a bare-metal hypervisor BitVisor.

An intermediate server periodically receives disaster information from a public server
and modifies it into a message that conforms to the DisasVisor protocol.  It then sends the
message to DisasVisor.  DisasVisor receives messages from UDP 26666 port.  When DisasVisor
receives a message, it transforms the message into a disaster notification image and
displays it on the computer screen.

### How to build

See BitVisor manuals and documents for details.

1. `cd DisasVisor/bitvisor`.
2. Configure `defconfig` file according your environment.  Replace IP addresses written in the file.
3. `make`
4. Copy the resulting file `bitvisor.elf` to the directory where kernel image files are placed.
5. Configure the boot loader to allow it to boot `bitvisor.elf` and the original operating system(s).

### How to run

#### Hypervisor

1. Boot `bitvisor.elf` from the boot loader.
2. When the boot loader runs again (on BitVisor), boot an operating system.

#### Intermediate server

1. `cd intermediate_server`
2. `./intermediate_server.py` disasvisor-client-ipaddr server-check-interval-in-second

### Tested platform

* Primary: Dell Optiplex 7010
  * CPU: Intel Core i5-3470
  * Chipset: Q77 Express
  * Graphics hardware: Intel HD 2500
  * Network hardware: Intel 82579LM
  * OS: Ubuntu 14.04.5 (kernel 3.13.0-24-generic), CentOS 6.6 (kernel 2.6.32-504.el6.x86_64), Windows 7
  * Compiler: gcc version 4.8.4 (Ubuntu 4.8.4-2ubuntu1~14.04.3)
  * Desktop environment: GNOME
  * Screen resolution: 1920 x 1200

### Resources

* Yoshihiro Oyama, "An Implementation Scheme of a DisasterWarning Notification System Based on a Hypervisor", The 33th Annual JSSST Conference, Sendai, Japan, September 2016 (in Japanese).

* Yoshihiro Oyama, "Disaster Warning Notification Using a Hypervisor", FIT2015, pp. 213-216, September 2015 (in Japanese).

* Yoshihiro Oyama, "A Hypervisor for Manipulating Guest Screens", In poster session in the 6th ACM SIGOPS Asia-Pacific Workshop on Systems (APSys 2015), Tokyo, Japan, July 2015.

* Yoshihiro Oyama, Natsuki Ogawa, Yudai Kawasaki, Kazuhiro Yamamoto, "ADvisor: A Hypervisor for Displaying Images on a Desktop", In Proceedings of the 2nd International Workshop on Computer Systems and Architectures (CSA '14), held in conjunction with the 2nd International Symposium on Computing and Networking (CANDAR 2014), pages 412-418, Shizuoka, Japan, December 2014. 
