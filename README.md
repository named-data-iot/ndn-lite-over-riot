NDN-Lite Over RIOT OS
========================
<img src="https://zhiyi-zhang.com/images/ndn-lite-logo.jpg" alt="logo" width="500"/>
This repo provides RIOT adaptation to NDN Lite on time module and network face. Examples are tested on macOS. The package provide two examples to run on either board (nRF52840DK) or native machine. Such seperation is largely because current RIOT doesn't not support latest macOS version well and may occur compiling errors when use `xtimer` on native port.

# Relation With Other RIOT Related NDN Repos
* [NDN-RIOT](https://github.com/named-data-iot/ndn-riot) uses dynamic memory allocation and this one don't. Besides, NDN-RIOT seperates application and forwarder, thus resulting in larger memory usage. You can consider this repo an evolved NDN-RIOT.
* [NDN-Lite Unit Tests Over RIOT OS](https://github.com/named-data-iot/ndn-lite-test-over-riot) is used for unit tests only and can't run on boards. To best facilitate testing and logging, it employs many memory expensive method that prevent it from a on-boarding run. The most important, it doesn't have RIOT networks stack adaptation.

# Board-runner
This example was tested on nRF52840DK and should also work on other RIOT boards. It uses some encoding functions that not offcially supported in NDN-Lite. Since encoding/decoding is memory expensive, and a typcial RIOT on-boarding thread is around 1.5KB. Current memory holding method not suits such constrained environment. Several Name or Interest will crash the thread. Board-runner employ some lightweight encoding functions specifically designed for this situation, which will be further improved and become another important feature named `light-encoding` provided in NDN-Lite. The program shows how to use these lightweight encoding functions to form Name or Interest TLV, and express an Interest, expecting to fetch Data named `/ndn/name/tests`, with an Interest timeout callback registered with timeout period 200ms. There will be no Data coming back, so related PIT entry will timeout after 200ms. It will return a timeout message to indicate that.

# Native-runner
This example was tested macOS 10.14.3 and should also work on other RIOT supported platforms. It's almost the basic the same as the `board-runner` example but no PIT timeout included, because timeout relies on timer system, but RIOT's `xtimer` seems not work well on macOS when in native port mode.

# Network Stack Adaptation
NDN-Lite leverages RIOT's unique GNRC network stack, which provides uniform network interfaces for all network devices and protocols. Such interfaces serve NDN's abstract communication paradigam well for it focusing on send/receive and hiding implementation details from library users. In this repo, when a NDN-Lite instance is initialized, it will auto-construct hardware network interfaces and bind them to NDN face by one-one match, the face id is defined as PID of the network interfaces thread (RIOT keep one thread per network interface, since most RIOT targeted devices only have handful hardware network interfaces, this solution works fine).

# Known Issues
## FIB Insertion
During network face auto-construction, NDN-Lite self-inserts a `/ndn` prefix to the lastest registered face. There's no interfaces to insert FIB for now, since the face id is determined by RIOT, one can not specify a face unless knowing certain communication thread (for nRF52840 IEEE 802.15.4 thread, PID = 3) PID ahead.
## Timer
You can have time module enabled program compiled for target board, but you can't `make all term` on macOS 10.14.3 for reasons mentioned above. If curious about whether your machine is compatiable with RIOT xtimer, place a `USEMODULE += xtimer` statement in `native-runner` example's makefile and see if errors occur.
