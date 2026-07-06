#include <Arduino.h>
#include "PIO_I2C.h"
static bool done=false;
static uint8_t chip=0, cal0=0, cal1=0;
void setup(){pinMode(LED_BUILTIN,OUTPUT);Serial.begin(115200);}
void loop(){
  static bool tried=false;
  if(!tried){tried=true;
    PIO_I2C i2c(2,3);
    i2c.begin();
    uint8_t r=0xD0,v=0;
    i2c.writeThenRead(0x76,&r,1,&v,1); chip=v;
    r=0x88; uint8_t c2[2];
    i2c.writeThenRead(0x76,&r,1,c2,2); cal0=c2[0]; cal1=c2[1];
    i2c.end();
    done=true;
  }
  Serial.print("chip=0x");Serial.print(chip,HEX);
  Serial.print(" cal0=0x");Serial.print(cal0,HEX);
  Serial.print(" cal1=0x");Serial.print(cal1,HEX);
  Serial.print(" t=");Serial.println(millis()/1000);
  delay(1000);
}
