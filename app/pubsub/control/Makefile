# name of your application
APPLICATION = ndn-control
# This has to be the absolute path to the RIOT base directory and NDN Lite adaptation :
RIOTBASE ?= $(CURDIR)/../../../../RIOT

ADAPTATION ?= $(CURDIR)/../../../adaptation
NDN_DISABLE_UDP ?= 1

USEPKG += ndn-lite

DIRS += $(ADAPTATION)
USEMODULE += ndn-adaptation


# If no BOARD is found in the environment, use this default:
BOARD ?= native

BOARD_INSUFFICIENT_MEMORY := airfy-beacon chronos msb-430 msb-430h nrf51dongle \
                          nrf6310 nucleo-f103 nucleo-f334 pca10000 pca10005 spark-core \
                          stm32f0discovery telosb weio wsn430-v1_3b wsn430-v1_4 \
                          yunjia-nrf51822 z1

# Include packages that pull up and auto-init the link layer.

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
CFLAGS += -DDEVELHELP


# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(ADAPTATION)/Makefile.include
include $(RIOTBASE)/Makefile.include

