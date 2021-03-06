
/**
 *  \file   msp430_watchdog.h
 *  \brief  MSP430 Watchdog timer definition 
 *  \author Antoine Fraboulet
 *  \date   2005
 **/

#ifndef MSP430_WATCHDOG_H
#define MSP430_WATCHDOG_H

#if defined(__msp430_have_watchdog)

#if !defined(WATCHDOG_BASE)
#define WATCHDOG_BASE 0x0120
#endif

#define WATCHDOG_START WATCHDOG_BASE
#define WATCHDOG_END   WATCHDOG_BASE

#define WDTCTL WATCHDOG_BASE

enum watchdog_mode_t {
  WDT_MODE_WATCHDOG = 0,
  WDT_MODE_INTERVAL = 1
};

#if defined(WORDS_BIGENDIAN)
struct __attribute__ ((packed)) wdtctl_t {
  uint16_t
    wdtpw:8,
    wdthold:1,
    wdtnmies:1,
    wdtnmi:1,
    wdttmsel:1,
    wdtcntcl:1,
    wdtssel:1,
    wdtis:2;
};
#else
struct __attribute__ ((packed)) wdtctl_t {
  uint16_t
    wdtis:2,
    wdtssel:1,
    wdtcntcl:1,
    wdttmsel:1,
    wdtnmi:1,
    wdtnmies:1,
    wdthold:1,
    wdtpw:8;
};
#endif

/**
 * Watchdog Data Structure
 **/
struct msp430_watchdog_t
{
  union {
    struct wdtctl_t b;
    uint16_t        s;
  } wdtctl;
  
  /* we keep int for counters to detect overflow */
  int wdtcnt;
  int wdtinterval;
};

void    msp430_watchdog_create (void);
void    msp430_watchdog_reset  (void);
void    msp430_watchdog_update (void);
int16_t msp430_watchdog_read   (uint16_t addr);
void    msp430_watchdog_write  (uint16_t addr, int16_t val);

enum watchdog_mode_t msp430_watchdog_getmode(void);
int     msp430_watchdog_chkifg ();

#else
#define msp430_watchdog_create() do { } while (0)
#define msp430_watchdog_reset()  do { } while (0)
#endif /* defined */
#endif /* header */
