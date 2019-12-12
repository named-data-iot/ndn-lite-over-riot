NDN-Lite Over RIOT OS
========================

## Work In Progress

* You need to fetch [ndn-lite](https://github.com/named-data-iot/ndn-lite)  `new-riot` branch  as  outside package.
* RIOT version is not the official repo nor the `named-data-iot` but a one under a personal account [RIOT](https://github.com/Zhiyi-Zhang/RIOT).


## Usage
### Preparation
You need to first have `gcc-arm-none-eabi` toolchain configured.
```
git clone http://github.com/Zhiyi-Zhang/RIOT
git clone http://github.com/named-data-iot/ndn-lite
git clone http://github.com/named-data-iot/ndn-lite-over-riot
cd ndn-lite
git checkout new-riot
```

### Building Consumer-Producer Examples with Native Ports
```
sudo ./RIOT/dist/tools/tapsetup/tapsetup -c 2
cd ndn-lite-over-riot/examples/ndn-producer
make all term

(Open Another Terminal)

cd ndn-lite-over-riot/examples/ndn-consumer
make all term PORT=tap1
```

## Few technical details
* Using slightly modified RIOT for larger stack size, for nRF52840DK, 4096 is the setting. Will test other boards lately.
* UDP face is IPv6 only, and uses link-local multicast address as remote address.
* Follow different fragmentation protocols than NDNLPv2. If one want to communicate with NFD, please avoid fragmentation.

## Why NDN-Lite instead of...

- NDN-RIOT

  NDN-RIOT uses `malloc()`. However, the RIOT is one-way `malloc()` thus the  allocated buffer never be freed. After significant amount of time, application can never successfully apply for a buffer. One should can uses RIOT's dynamic memory allocation buffer to mitigate this issue, which at the cost of performance overhead. NDN-Lite however, no `malloc()`.

  Also, NDN-RIOT doesn't follow latest NDN packet format and has strict assumption on packet size. NDN-Lite however, follow the latest packet format and can directly communicate with NFD.

  NDN-RIOT operate over network layer and auto-configure all link-layer interfaces with NDN. It can't receive the NDN packet which is on a UDP overlay. NDN-Lite however, support both pure NDN and NDN-over-UDP fashion. For example, one can receive a pure NDN packet with IEEE 802.15.4 in one face, and push pack to the network with UDP face. 

  NDN-Lite is a library which's enabled application-layer protocols, which may ease your development and research.



