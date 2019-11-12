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