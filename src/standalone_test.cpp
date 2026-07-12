#ifdef BMX280PIO_RP2040_STANDALONE_TEST
#include <Arduino.h>
#include "BMx280PIO_RP2040.h"
BMx280PIO_RP2040 bme(4,5);
void setup(){
    Serial.begin(115200);while(!Serial)delay(100);delay(500);
    if(!bme.begin()){Serial.println("FAIL");while(1);}
    Serial.print("Sensor:");Serial.println(bme.isBME280()?"BME":"BMP");
    bme.setMode(BME280_MODE_NORMAL);delay(100);
    float t,p,h;
    for(int i=0;i<5;i++){bme.readAll(&t,&p,&h);Serial.print("T=");Serial.print(t,2);Serial.print(" P=");Serial.print(p,2);Serial.println("hPa");delay(500);}
}
void loop(){}
#endif
