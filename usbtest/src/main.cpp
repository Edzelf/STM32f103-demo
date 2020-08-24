#include <Arduino.h>

// Test for serial USB I/O on STM32F103
// Will print some text and react on Serial input.
//
void setup()
{
	//activate USB CDC driver
	SerialUSB.begin() ;
	SerialUSB.println ( "Hello world" ) ;
  pinMode ( PC13, OUTPUT ) ;
}

void loop()
{
  static int count = 0 ;

  if ( SerialUSB.available() )
  {
    SerialUSB.read() ;
  	SerialUSB.println ( "Once again: Hello world" ) ;
  }
  digitalWrite ( PC13, millis() & 1024 ) ;  // Toggle LED every 1024 msec
  if ( ( millis() % 5000 ) == 0 )
  {
  	SerialUSB.println ( "Enter some characters...") ;
  }
  delay ( 10 ) ;
}