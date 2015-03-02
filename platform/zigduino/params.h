#ifndef PARAMS_H_
#define PARAMS_H_
/* PARAMETER_STORAGE =
 * 0 Hard coded, minmal program and eeprom usage.
 * 1 Stored in fixed eeprom locations, rewritten from flash if corrupt.
 *   This allows parameter changes using a hardware programmer or custom application code.
 *   Corruption test is based on channel verify so get the channel before anything else!
 * 2 Obtained from eeprom using the general settings manager and read from program flash if not present.
 *   Useful for for testing builds without wearing out flash memory.
 * 3 Obtained from eeprom using the settings manager and rewritten from flash if not present.
 *   This ensures all parameters are present in upper eeprom flash.
 *
 * Note the parameters in this file can be changed without forcing a complete rebuild.
 */

#define PARAMETER_STORAGE 1
// Generated random MAC. When PARAM_STORAGE = 1 | 3, also stored in EEPROM
// after generation and only regenerated upon corruption.
#define CONTIKI_CONF_RANDOM_MAC 0        //adds 78 bytes

/* Enable Contiki settings manager when PARAMETER_STORAGE 2 or 3 */
#if PARAMETER_STORAGE > 1
#define CONTIKI_CONF_SETTINGS_MANAGER 0  //adds 1696 bytes
#endif

/* If PARAMETER_STOREGE 2, dummy out the write routines of setting manager */
#include "settings.h"
#if PARAMETER_STORAGE==2
#define settings_add(...) 0
#define settings_add_uint8(...) 0
#define settings_add_uint16(...) 0
#endif

#ifdef CHANNEL_802_15_4
#define PARAMS_CHANNEL CHANNEL_802_15_4
#else
#define PARAMS_CHANNEL 26
#endif
#ifdef IEEE802154_PANID
#define PARAMS_PANID IEEE802154_PANID
#else
#define PARAMS_PANID 0xABCD
#endif
#ifdef IEEE802154_PANADDR
#define PARAMS_PANADDR IEEE802154_PANADDR
#else
#define PARAMS_PANADDR 0
#endif
#ifdef RF230_MAX_TX_POWER
#define PARAMS_TXPOWER RF230_MAX_TX_POWER
#else
#define PARAMS_TXPOWER 0
#endif
#ifdef EUI64_ADDRESS
#define PARAMS_EUI64ADDR EUI64_ADDRESS
#else
/* This form of of EUI64 mac allows full 6LoWPAN header compression from mac address */
#if UIP_CONF_LL_802154
//#define PARAMS_EUI64ADDR {0x02, 0xNN, 0xNN, 0xNN, 0xNN, 0xNN, 0xNN, 0xNN}
#define PARAMS_EUI64ADDR {0x02, 0x00, 0x00, 0xff, 0xfe, 0x00, 0x00, 0x01}
#else
//#define PARAMS_EUI64ADDR {0x02, 0xNN, 0xNN, 0xff, 0xfe, 0xNN, 0xNN, 0xNN}
#define PARAMS_EUI64ADDR {0x00, 0x00, 0x00, 0xff, 0xfe, 0x00, 0x00, 0x01}
#endif
/* This form of of EUI64 mac allows 16 bit 6LoWPAN header compression on multihops */
//#define PARAMS_EUI64ADDR {0x02, 0x00, 0x00, 0xff, 0xfe, 0x00, 0xNN, 0xNN}
#endif

/* EUI64 always obtained through function */
uint8_t params_get_eui64(uint8_t *eui64);

#if PARAMETER_STORAGE==0
/* Hard coded program flash parameters */
#define params_get_channel(...) PARAMS_CHANNEL
#define params_get_panid(...) PARAMS_PANID
#define params_get_panaddr(...) PARAMS_PANADDR
#define params_get_txpower(...) PARAMS_TXPOWER
#else
/* Parameters stored in eeprom */
uint8_t params_get_channel(void);
uint16_t params_get_panid(void);
uint16_t params_get_panaddr(void);
uint8_t params_get_txpower(void);
#endif

/* AVR webserver has parameters defined elsewhere, which are read out directly
 * (no special params_get functions for these). Make them accessible here. */
#if AVR_WEBSERVER
/* Webserver builds have EEMEM vars specified in httpd-fsdata.c via makefsdata.h */
extern uint8_t eemem_server_name[16];
extern uint8_t eemem_domain_name[30];
#endif

#endif /* PARAMS_H_ */
