CONTIKI_PROJECT=border-router
all: $(CONTIKI_PROJECT) 

CONTIKI=../../../..
TARGET = avr-micropnp

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
PROJECT_SOURCEFILES += slip-bridge.c 

CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include



ifeq ($(PREFIX),)
 PREFIX = aaaa::1/64
endif

ifeq ($(TUNSLIP_BAUD),)
 TUNSLIP_BAUD = 57600
endif

$(CONTIKI)/tools/tunslip6:	$(CONTIKI)/tools/tunslip6.c
	(cd $(CONTIKI)/tools && $(MAKE) tunslip6)

connect-router:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -B $(TUNSLIP_BAUD) $(PREFIX)
