//***************************************************************************************************
// rtctest - main.cpp
//***************************************************************************************************
#include <Arduino.h>
#include <Wire.h>                                 // Wire needed for compile
#include <time.h>                                 // For unix time conversion

//***************************************************************************************************
//  RTC clock definitions.                                                                          *
//***************************************************************************************************
// The RTC clock runs on UTC, so use UTC time to synchronise the clock.                             *
// Define the required timezone here by uncommenting one of the two following definitions.          *
//***************************************************************************************************
#define TIMEZONE "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"       // Timezone for Europe/Amsterdam
//#define TIMEZONE "NZST-12NZDT-13,M10.1.0/02:00:00,M3.3.0/03:00:00"  // Timezone for New Zealand
//***************************************************************************************************

//***************************************************************************************************
//  Other configuration definitions.                                                                *
//***************************************************************************************************
#define RANDOMUNIXTIME 1597923721                         // Unix time, 08/20/2020 @ 11:42 (UTC)
#define DATAVALID           31083                         // Pattern for data valid in RTC memory
#define TIME_TO_SLEEP          20                         // Time (in seconds) to sleep

uint32_t    starttime = 1 ;                               // Timestamp at start-up

//**************************************************************************************************
//                                   G E T _ U N I X _ R T C                                       *
//**************************************************************************************************
// Get the Unix timestamp from the counter of the RTC.                                             *
// Guard against race connditions.                                                                 *
//**************************************************************************************************
uint32_t get_unix_rtc()
{
  uint32_t old_res = 0 ;                          // First reading fom counter registers
  uint32_t new_res = ~old_res ;                   // Second reading from counter registers

  while ( new_res != old_res )                    // Need 2 equal readings
  {
    old_res = new_res ;                           // Remember previous reading
    new_res = RTC->CNTH << 16 | RTC->CNTL ;       // Get timestamp, 32 bits
  }
  return new_res ;                                // Return timestamp
}


//**************************************************************************************************
//                                   S E T _ U N I X _ R T C                                       *
//**************************************************************************************************
// Set the Unix timestamp to the counter of the RTC.                                               *
// Guard against race conditions.                                                                  *
//**************************************************************************************************
void set_unix_rtc ( uint32_t newtimestamp )
{
  while ( get_unix_rtc() != newtimestamp )        // Keep trying if not properly set
  {
    RTC->CRL |= RTC_CRL_CNF ;                     // Allow RTC configuration
    RTC->CNTH = newtimestamp >> 16 ;              // Set the counter
    RTC->CNTL = newtimestamp & 0xFFFF ;
    RTC->CRL &= ~RTC_CRL_CNF ;                    // Block RTC configuration
    while ( ( RTC->CRL & RTC_CRL_RTOFF ) == 0 ) ;
  }
}


//**************************************************************************************************
//                             S E T _ U N I X _ R T C _ A L A R M                                 *
//**************************************************************************************************
// Set a Unix timestamp as an alarm (wake-up) time to the ALRH/L registers of the RTC.             *
// EXTI line 17 is connected to the RTC Alarm event.                                               *
//**************************************************************************************************
void set_unix_rtc_alarm ( uint32_t newalarmtime )
{
  RTC->CRL |= RTC_CRL_CNF ;                       // Allow RTC configuration
  RTC->ALRH = newalarmtime >> 16 ;                // Set the counter (write-only)
  RTC->ALRL = newalarmtime & 0xFFFF ;
  RTC->CRL &= ~RTC_CRL_CNF ;                      // Block RTC configuration
  while ( ( RTC->CRL & RTC_CRL_RTOFF ) == 0 ) ;
}


//**************************************************************************************************
//                                   I N I T _ U N I X _ R T C                                     *
//**************************************************************************************************
// Setup RTC clock to facilitate a Unix timestamp.                                                 *
// If the clock is not configurated, for example after a battery power loss, the clock is          *
// initialize at a random time in August 2020.  the first register in the backup domain is used    *
// to hold the initialized state of the RTC as well as other values in the backup domain.          *
// Return true if RTC has been reset.                                                              *
// Clock compensation is roughly the number of seconds per day divided by 2.                       *
// So if the clock is 10 seconds per day too slow, the compensation is -5.  If the clock is 10     *
// seconds per day too fast, the compensation is +5.                                               *
// Be sure to reset the clock (remove power and battery) if you change the compensation.           *
//**************************************************************************************************
bool init_unix_rtc()
{
  bool     res ;                                      // Function result
  uint32_t PRELOAD = 32767 ;                          // Preload for 32.768 xtal

  RCC->APB1ENR |= ( RCC_APB1ENR_PWREN |               // Enable BKP domain and interface clocks
                    RCC_APB1ENR_BKPEN ) ;
  PWR->CR |= PWR_CR_DBP ;
  res = ( BKP->DR1 != DATAVALID ) ;                   // Get BKP domain (and RTC clock) condition
  if ( res )                                          // BKP domain (and RTC clock) in right condition?
  {
    RCC->BDCR |= RCC_BDCR_LSEON ;                     // No, enable LSE clock
    while ( (RCC->BDCR & RCC_BDCR_LSERDY) == 0 ) ;    // Wait until LSE is ready
    RCC->BDCR |= RCC_BDCR_RTCSEL_LSE ;                // Select LSE
    RCC->BDCR |= RCC_BDCR_RTCEN ;                     // Enable RTC clock
    RTC->CRL &= ~RTC_FLAG_RSF ;                       // Clear RSF
    while ( ( RTC->CRL & RTC_FLAG_RSF ) == 0 ) ;      // Wait till flag comes back
    while ( ! ( RTC->CRL & RTC_CRL_RSF ) ) ;          // Wait for RSF bit
    while ( ( RTC->CRL & RTC_CRL_RTOFF ) == 0 ) ;
    RTC->CRL |= RTC_CRL_CNF ;                         // Allow RTC configuration
    RTC->PRLH = PRELOAD >> 16 ;                       // Set preload, high order part
    RTC->PRLL = PRELOAD & 0xFFFF ;                    // Set preload, low order part
    BKP->RTCCR |= 0 ;                                 // Fine calibration BKP_RTCCR_CAL
    RTC->CNTH = ( RANDOMUNIXTIME >> 16 ) ;            // Set to a legal timestamp
    RTC->CNTL = RANDOMUNIXTIME & 0xFFFF ;
    RTC->CRL &= ~RTC_CRL_CNF ;                        // Block RTC configuration
    while ( ! ( RTC->CRL & RTC_CRL_RTOFF ) ) ;
  }
  return res ;                                        // Return the reset status
} 


//***************************************************************************************************
//                                      F O R M A T _ T I M E                                       *
//***************************************************************************************************
// Get formatted date and time for debug pupose.                                                    *
// If parameter is zero, it will use the current date/time.                                         *
//***************************************************************************************************
const char* format_time ( time_t t )
{
  struct tm   ts ;
  const char* format = "%4d-%02d-%02d %02d:%02d:%02d" ;      // Format to use
  static char buf[36] ;                                      // 20 is long enough, but the compiler
                                                             // does not agree...
  if ( t == 0 )                                              // Zero passed?
  {
    t =  get_unix_rtc() ;                                    // Yes, get Unix time from RTC
  }
  ts = *localtime ( &t ) ;
  sprintf ( buf, format,                                     // Format the date and the time
            ts.tm_year + 1900, ts.tm_mon + 1, ts.tm_mday,
            ts.tm_hour,        ts.tm_min,     ts.tm_sec ) ;
  return buf ;                                               // Return pointer to the string
}


//**************************************************************************************************
//                                     S E T U P                                                   *
//**************************************************************************************************
//**************************************************************************************************
void setup()
{
  bool        wakeUpFlag ;
  bool        treset ;                                    // RTC reset status
  const char* startreason = "wake-up from sleep" ;        // Default start-up reason

  wakeUpFlag = ( RTC->CRL & RTC_CRL_ALRF ) != 0 ;         // Get RTC alarm status
  if ( !wakeUpFlag )                                      // True if waked-up from sleep, not by reset
  {
    startreason = "reset or power-cycle" ;                // Change start-up reason
  }
  setenv ( "TZ", TIMEZONE, 1 ) ;                          // Set timezone for NL or NZ
  treset = init_unix_rtc() ;                              // Ininialize RTC clock and backup domain
  starttime = get_unix_rtc() - 1 ;                        // Remember starttime, compensated for start-up time
  BKP->RTCCR |= BKP_RTCCR_CCO_Msk ;                       // Output 512 HZ to PC13
  Serial.begin ( 115200 ) ;
  Serial.printf ( "\n\nStarting RTC test"                 // Log start event
                  " at %s by %s\n",
                  format_time ( starttime ),              // and starttime
                  startreason ) ;                         // and the reason
  if ( treset )                                           // RTC reset?
  {
    Serial.println ( "RTC invalid, reset!") ;             // Yes, report
  }
  BKP->DR1 = DATAVALID ;                                  // Signal data is valid in here
} 


//**************************************************************************************************
//                                     L O O P                                                     *
//**************************************************************************************************
// Main loop of the program.                                                                       *
//**************************************************************************************************
void loop()
{
  if ( millis() > 10000 )                                 // AFter 10 seconds
  {
    uint32_t alrmtime = starttime + TIME_TO_SLEEP ;       // Set wake-up time
    Serial.printf ( "Sleep until : %s...\n",              // Show wake-up time
                    format_time ( alrmtime ) ) ;
    Serial.flush() ;
    set_unix_rtc_alarm ( alrmtime ) ;                     // Set wake-up time
    __disable_irq() ;
    BKP->RTCCR &= ~BKP_RTCCR_CCO_Msk ;                    // Disable cloctest output
    EXTI->PR =  0x007FFFFF ;                              // Clear any pending EXTI interrupts ;
    PWR->CR |= PWR_CR_CWUF ;                              // In case the pin was high already 
    SET_BIT ( PWR->CR, PWR_CR_PDDS ) ;
    SET_BIT ( SCB->SCR, ((uint32_t)SCB_SCR_SLEEPDEEP_Msk)) ;
    __WFI() ;
    // will never arrive here
  }
  Serial.printf ( "Time is: %s\n", format_time ( 0 ) ) ;  // Show time
  delay ( 2000 ) ;
}
