/*
 * Copyright (c) 2006, Technical University of Munich
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */
#define PRINTF(FORMAT,args...) printf_P(PSTR(FORMAT),##args)

#define ANNOUNCE_BOOT 1    //adds about 600 bytes to program size
#if ANNOUNCE_BOOT
#define PRINTA(FORMAT,args...) printf_P(PSTR(FORMAT),##args)
#else
#define PRINTA(...)
#endif

#define DEBUG 1
#if DEBUG
#define PRINTD(FORMAT,args...) printf_P(PSTR(FORMAT),##args)
#else
#define PRINTD(...)
#endif

#include <avr/pgmspace.h>
#include <avr/fuse.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include <string.h>
#include <dev/watchdog.h>

#include "loader/symbols-def.h"
#include "loader/symtab.h"

#include "params.h"

#include "contiki.h"
#include "contiki-lib.h"

#include "dev/rs232.h"

/* Track interrupt flow through mac, rdc and radio driver */
//#define DEBUGFLOWSIZE 32
#if DEBUGFLOWSIZE
uint8_t debugflowsize,debugflow[DEBUGFLOWSIZE];
#define DEBUGFLOW(c) if (debugflowsize<(DEBUGFLOWSIZE-1)) debugflow[debugflowsize++]=c
#else
#define DEBUGFLOW(c)
#endif

/* Get periodic prints from idle loop, from clock seconds or rtimer interrupts */
/* Use of rtimer will conflict with other rtimer interrupts such as contikimac radio cycling */
/* STAMPS will print ENERGEST outputs if that is enabled. */
#define PERIODICPRINTS 1
#if PERIODICPRINTS
#define STAMPS 60
#define STACKMONITOR 1024
uint32_t clocktime;
#define TESTRTIMER 0
#if TESTRTIMER
uint8_t rtimerflag=1;
struct rtimer rt;
void rtimercycle(void) {rtimerflag=1;}
#endif
#endif

/*-------------------------------------------------------------------------*/
/*----------------------Configuration of the .elf file---------------------*/
/* The proper way to set the signature is */
#include <avr/signature.h>

/* External crystal osc as clock, maximum start-up delay. SPI and EESAVE
 * enabled, brownout on 2.7V */
//FUSES ={.low = 0xff, .high = 0xd7, .extended = 0xfd,};
FUSES = {
  .low = 0xff, // Nothing programmed
  .high = (FUSE_SPIEN & FUSE_EESAVE),
  .extended = (FUSE_BODLEVEL1),
};



uint8_t
rng_get_uint8(void) {
/* Get a pseudo random number using the ADC */
  uint8_t i;
  uint8_t j = 0;
  ADCSRA=1<<ADEN;             //Enable ADC, not free running, interrupt disabled, fastest clock
  for (i=0;i<4;i++) {
    ADMUX = 0;                //toggle reference to increase noise
    ADMUX =0x1E;              //Select AREF as reference, measure 1.1 volt bandgap reference.
    ADCSRA|=1<<ADSC;          //Start conversion
    while (ADCSRA&(1<<ADSC)); //Wait till done
	j = (j<<2) + ADC;
  }
  ADCSRA=0;                   //Disable ADC
  PRINTD("rng issues %d\n",j);
  return j;
}

/*-------------------------Low level initialization------------------------*/
/*------Done in a subroutine to keep main routine stack usage small--------*/
void initialize(void)
{
  uint8_t mcusr_backup = MCUSR;
  MCUSR = 0;
  watchdog_init();
  watchdog_start();

  /* Generic or slip connection on uart0 (USB) */
  rs232_init(RS232_PORT_1, USART_BAUD_57600, USART_PARITY_NONE | USART_STOP_BITS_1 | USART_DATA_BITS_8);
  /* Redirect stdout */
  rs232_redirect_stdout(RS232_PORT_1);

  /* Second rs232 port for debugging or slip alternative */
  //rs232_init(RS232_PORT_1, USART_BAUD_57600,USART_PARITY_NONE | USART_STOP_BITS_1 | USART_DATA_BITS_8);
  
  clock_init();

  // Print reboot reason
  if(mcusr_backup & (1<<PORF )) PRINTD("Power-on reset.\n");
  if(mcusr_backup & (1<<EXTRF)) PRINTD("External reset!\n");
  if(mcusr_backup & (1<<BORF )) PRINTD("Brownout reset!\n");
  if(mcusr_backup & (1<<WDRF )) PRINTD("Watchdog reset!\n");
  if(mcusr_backup & (1<<JTRF )) PRINTD("JTAG reset!\n");

#if STACKMONITOR
  /* Simple stack pointer highwater monitor. Checks for magic numbers in the main
   * loop. In conjuction with PERIODICPRINTS, never-used stack will be printed
   * every STACKMONITOR seconds.
   */
  {
    extern uint16_t __bss_end;
    uint16_t p = (uint16_t) &__bss_end;
    do {
      *(uint16_t *)p = 0x4242;
      p+=10;
    } while(p<SP-10); //don't overwrite our own stack
  }
#endif

  PRINTA("\n*******Booting %s*******\n",CONTIKI_VERSION_STRING);

  /* Initialize rtimers */
  rtimer_init();

  /* Initialize process subsystem */
  process_init();

  /* etimers must be started before ctimer_init */
  process_start(&etimer_process, NULL);
  ctimer_init();

  /* Get a random seed for the 802.15.4 packet sequence number.
   * Some layers will ignore duplicates found in a history (e.g. Contikimac)
   * causing the initial packets to be ignored after a short-cycle restart.
   */
  random_init(rng_get_uint8());

#if ANNOUNCE_BOOT
  PRINTA("MicroPnP boot\n");	  
#endif /* ANNOUNCE_BOOT */

  /* Autostart other processes */
  autostart_start(autostart_processes);

/*--------------------------Announce the configuration---------------------*/
#if ANNOUNCE_BOOT
   PRINTA("Online\n");
#endif /* ANNOUNCE_BOOT */
}

/*-------------------------------------------------------------------------*/
/*------------------------- Main Scheduler loop----------------------------*/
/*-------------------------------------------------------------------------*/
int
main(void)
{
  initialize();

  while(1) {
    process_run();
    watchdog_periodic();

    /* Set DEBUGFLOWSIZE in contiki-conf.h to track path through MAC, RDC, and RADIO */
#if DEBUGFLOWSIZE
    if (debugflowsize) {
      debugflow[debugflowsize]=0;
      PRINTF("%s",debugflow);
      debugflowsize=0;
    }
#endif

#if PERIODICPRINTS
#if TESTRTIMER
    /* Timeout can be increased up to 8 seconds maximum.
     * A one second cycle is convenient for triggering the various debug printouts.
     * The triggers are staggered to avoid printing everything at once.
     */
    if (rtimerflag) {
      rtimer_set(&rt, RTIMER_NOW()+ RTIMER_ARCH_SECOND*1UL, 1,(void *) rtimercycle, NULL);
      rtimerflag=0;
#else
    if (clocktime!=clock_seconds()) {
      clocktime=clock_seconds();
#endif

#if STAMPS
      if ((clocktime%STAMPS)==0) {
#if ENERGEST_CONF_ON
        #include "lib/print-stats.h"
	    print_stats();
#else
        PRINTF("%us\n",clocktime);
#endif

      }
#endif
#if TESTRTIMER
      clocktime+=1;
#endif

#if STACKMONITOR
      if ((clocktime%STACKMONITOR)==3) {
        extern uint16_t __bss_end;
        uint16_t p=(uint16_t)&__bss_end;
        do {
          if(*(uint16_t *)p != 0x4242) {
            PRINTF("Never-used stack > %d bytes\n",p-(uint16_t)&__bss_end);
            break;
          }
          p+=10;
        } while (p<RAMEND-10);
      }
#endif
    }
#endif /* PERIODICPRINTS */
  }
  return 0;
}

/*---------------------------------------------------------------------------*/

void log_message(char *m1, char *m2)
{
  PRINTF("%s%s\n", m1, m2);
}
