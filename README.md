# DisasVisor

### Overview

DisasVisor is a hypervisor for displaying disaster notification messages on computer screens.
It is implemented by modifying a bare-metal hypervisor BitVisor.

An intermediate server periodically receives disaster information from a public server
and modifies it into a message that conforms to the DisasVisor protocol.  It then sends the
message to DisasVisor.  DisasVisor receives messages from UDP 26666 port.  When DisasVisor
receives a message, it transforms the message into a disaster notification image and
displays it on the computer screen.

![Structure of DisasVisor](/picture/structure.jpg)

![Screenshot of CentOS running on DisasVisor](/picture/screenshot-centos.jpg)

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

### Notes

* DisasVisor is based on the source code of BitVisor that was obtained from the BitVisor repository on Oct 16, 2016.
* The current intermediate server for DisasVisor obtains disaster information from the [Hi-net](http://www.hinet.bosai.go.jp/?LANG=en) developed in [NIED](http://www.bosai.go.jp/e/).
* The current version of DisasVisor is a research prototype.  Understand that DisasVisor still has some defects to be fixed and any data in the operating system running on DisasVisor may be lost at any moment.
* Disclaimer of warranty: you understand and agree that if you download or otherwise obtain materials, information, products, software, programs, or services, you do so at your own discretion and risk and that you will be solely responsible for any damages that may result, including loss of data or damage to your computer system.

### Resources

* Yoshihiro Oyama, "An Implementation Scheme of a DisasterWarning Notification System Based on a Hypervisor", The 33th Annual JSSST Conference, Sendai, Japan, September 2016 (in Japanese). [PDF](http://jssst.or.jp/files/user/taikai/2016/GENERAL/general2-1.pdf)

* Yoshihiro Oyama, "Disaster Warning Notification Using a Hypervisor", FIT2015, pp. 213-216, Matsuyama, Japan, September 2015 (in Japanese). [Link](https://ipsj.ixsq.nii.ac.jp/ej/?action=pages_view_main&active_action=repository_view_main_item_detail&item_id=153652&item_no=1&page_id=4328&block_id=19)

* Yoshihiro Oyama, "A Hypervisor for Manipulating Guest Screens", In poster session in the 6th ACM SIGOPS Asia-Pacific Workshop on Systems (APSys 2015), Tokyo, Japan, July 2015. [PDF](http://www.sslab.ics.keio.ac.jp/apsys2015/assets/posters/4.pdf)

* Yoshihiro Oyama, Natsuki Ogawa, Yudai Kawasaki, Kazuhiro Yamamoto, "ADvisor: A Hypervisor for Displaying Images on a Desktop", In Proceedings of the 2nd International Workshop on Computer Systems and Architectures (CSA '14), held in conjunction with the 2nd International Symposium on Computing and Networking (CANDAR 2014), pages 412-418, Shizuoka, Japan, December 2014. [Link](http://ieeexplore.ieee.org/document/7052219/?arnumber=7052219)
