#include <avr/sleep.h>
#include <avr/io.h>
#include <util/atomic.h>
#include <stdio.h>

void
pm_sleep()
{
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  ATOMIC_BLOCK(ATOMIC_FORCEON){
    sleep_enable();
    sleep_bod_disable();
  }
  sleep_cpu();
  sleep_disable();
}

uint8_t
pm_io_done()
{
  /* For now only checks UART if done */
  return (UCSR1A & _BV(TXC1))
    && (UCSR0A & _BV(TXC0)) && (PINB & _BV(PB3));
}
