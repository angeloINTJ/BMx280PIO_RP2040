#include <Arduino.h>
#include <hardware/pio.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/clocks.h>
#include "i2c.pio.h"
static uint8_t rev8(uint8_t b){b=(b&0xF0)>>4|(b&0x0F)<<4;b=(b&0xCC)>>2|(b&0x33)<<2;b=(b&0xAA)>>1|(b&0x55)<<1;return b;}
static inline uint16_t mk(bool s,bool r,bool p,uint8_t d){uint8_t i=(~rev8(d))&0xFF;return(s?1:0)|((r?1:0)<<1)|(((uint16_t)i)<<2)|((p?1:0)<<10);}

void setup(){
    Serial.begin(115200);for(int i=0;i<60&&!Serial;i++)delay(100);delay(500);Serial.println("DELAY FIX TEST");
    auto sl=[]{gpio_set_dir(2,GPIO_OUT);gpio_put(2,0);};auto sh=[]{gpio_set_dir(2,GPIO_IN);};auto cl=[]{gpio_put(3,0);};auto ch=[]{gpio_put(3,1);};auto d5=[]{delayMicroseconds(5);};
    auto wb=[&](uint8_t b){for(uint8_t m=0x80;m;m>>=1){if(b&m)sh();else sl();d5();ch();d5();cl();}sh();d5();ch();d5();bool a=gpio_get(2);cl();d5();return !a;};
    gpio_init(2);gpio_set_dir(2,GPIO_IN);gpio_pull_up(2);gpio_init(3);gpio_set_dir(3,GPIO_OUT);gpio_put(3,1);
    sl();d5();cl();d5();wb(0xEC);wb(0xE0);wb(0xB6);sl();d5();ch();d5();sh();d5();delay(20);
    sl();d5();cl();d5();wb(0xEC);wb(0xF4);wb(0x25);sl();d5();ch();d5();sh();d5();delay(50);

    PIO pio=pio0;int off=pio_add_program(pio,&i2c_master_program);int sm=pio_claim_unused_sm(pio,false);
    pio_sm_config c=i2c_master_program_get_default_config(off);
    sm_config_set_out_pins(&c,2,1);sm_config_set_set_pins(&c,2,1);sm_config_set_in_pins(&c,2);sm_config_set_sideset_pins(&c,3);
    sm_config_set_in_shift(&c,false,true,8);sm_config_set_clkdiv(&c,96.15f);
    pio_sm_init(pio,sm,off,&c);pio_sm_set_pindirs_with_mask(pio,sm,(1u<<3),(1u<<2)|(1u<<3));
    gpio_pull_up(2);gpio_set_function(2,GPIO_FUNC_PIO0);gpio_set_function(3,GPIO_FUNC_PIO0);pio_sm_set_enabled(pio,sm,true);

    for(int n=0;n<5;n++){
        sl();d5();cl();d5();wb(0xEC);wb(0xF4);wb(0x25);sl();d5();ch();d5();sh();d5();delay(50);
        const int N=8;uint32_t cmds[N*4],buf[N*4]={0};
        for(int i=0;i<N;i++){uint8_t r=0xF7+i;cmds[i*4+0]=mk(1,0,0,0xEC);cmds[i*4+1]=mk(0,0,0,r);cmds[i*4+2]=mk(1,0,0,0xED);cmds[i*4+3]=mk(0,1,1,0xFF);}
        int rx=dma_claim_unused_channel(false);dma_channel_config rc=dma_channel_get_default_config(rx);
        channel_config_set_transfer_data_size(&rc,DMA_SIZE_32);channel_config_set_read_increment(&rc,false);channel_config_set_write_increment(&rc,true);
        channel_config_set_dreq(&rc,pio_get_dreq(pio,sm,false));dma_channel_configure(rx,&rc,buf,&pio->rxf[sm],N*4,true);
        int tx=dma_claim_unused_channel(false);dma_channel_config tc=dma_channel_get_default_config(tx);
        channel_config_set_transfer_data_size(&tc,DMA_SIZE_32);channel_config_set_read_increment(&tc,true);channel_config_set_write_increment(&tc,false);
        channel_config_set_dreq(&tc,pio_get_dreq(pio,sm,true));dma_channel_configure(tx,&tc,&pio->txf[sm],cmds,N*4,false);
        dma_start_channel_mask((1u<<tx)|(1u<<rx));while(dma_channel_is_busy(rx))tight_loop_contents();pio_sm_set_enabled(pio,sm,false);
        uint8_t raw[8];for(int i=0;i<N;i++)raw[i]=rev8(buf[i*4+3]&0xFF);
        int ack_ok=0;for(int i=0;i<N;i++)if(buf[i*4+1]==0)ack_ok++;
        Serial.print(n);Serial.print(": ACK_OK=");Serial.print(ack_ok);Serial.print("/8 RAW:");for(int i=0;i<8;i++){Serial.print(" ");Serial.print(raw[i],HEX);}Serial.println();
        dma_channel_unclaim(tx);dma_channel_unclaim(rx);delay(500);
    }
    pio_sm_set_enabled(pio,sm,false);Serial.println("DONE");
}
void loop(){}
