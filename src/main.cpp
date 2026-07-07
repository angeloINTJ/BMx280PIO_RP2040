#include <Arduino.h>
#include <hardware/pio.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/clocks.h>
#include "i2c.pio.h"

#define MAX_SAMPLES 8000
static volatile uint8_t  sm_buf[MAX_SAMPLES];
static volatile uint32_t ts_buf[MAX_SAMPLES];
static volatile uint32_t sm_count = 0;
static volatile bool     sm_active = false;
static volatile bool     sm_go = false;

// Core 1: super-sampler (runs automatically via setup1/loop1)
void setup1() {
    gpio_init(2); gpio_set_dir(2, GPIO_IN);
    gpio_init(3); gpio_set_dir(3, GPIO_IN);
}
void loop1() {
    if (sm_go) {
        sm_go = false; sm_active = true; sm_count = 0;
        for (int i = 0; i < MAX_SAMPLES; i++) {
            ts_buf[i] = timer_hw->timelr;
            sm_buf[i] = (sio_hw->gpio_in >> 2) & 0x3;
        }
        sm_count = MAX_SAMPLES; sm_active = false;
    }
    delay(1);
}

void start_sampling() { while(sm_active)delay(1); sm_go=true; while(!sm_active)delay(1); }

void analyze() {
    uint32_t n = sm_count;
    if (n<2) { Serial.println("No data"); return; }
    float us = 1.0f/(float)clock_get_hz(clk_sys)*1000000.0f;
    Serial.print("Samples:"); Serial.print(n); Serial.print(" Dur:"); Serial.print((ts_buf[n-1]-ts_buf[0])*us,0);Serial.println("us");
    uint32_t hi_s=0,lo_s=0,hi_n=0,lo_n=0,last_scl=2,lc=0,bits=0;
    uint32_t min_h=999999,max_h=0,min_l=999999,max_l=0;
    for(uint32_t i=1;i<n;i++){uint8_t s=sm_buf[i];uint8_t sc=s&2;
        if(sc&&!last_scl){uint32_t d=ts_buf[i]-ts_buf[lc];if(lc>0){lo_s+=d;lo_n++;if(d<min_l)min_l=d;if(d>max_l)max_l=d;}lc=i;}
        if(!sc&&last_scl){uint32_t d=ts_buf[i]-ts_buf[lc];if(lc>0){hi_s+=d;hi_n++;bits++;if(d<min_h)min_h=d;if(d>max_h)max_h=d;}lc=i;}
        last_scl=sc;}
    Serial.print("Bits:");Serial.println(bits);
    if(hi_n){Serial.print("SCL_HI avg:");Serial.print((float)hi_s/hi_n*us,2);Serial.print(" min:");Serial.print(min_h*us,2);Serial.print(" max:");Serial.print(max_h*us,2);Serial.println("us");}
    if(lo_n){Serial.print("SCL_LO avg:");Serial.print((float)lo_s/lo_n*us,2);Serial.print(" min:");Serial.print(min_l*us,2);Serial.print(" max:");Serial.print(max_l*us,2);Serial.println("us");}
}

static uint8_t rev8(uint8_t b){b=(b&0xF0)>>4|(b&0x0F)<<4;b=(b&0xCC)>>2|(b&0x33)<<2;b=(b&0xAA)>>1|(b&0x55)<<1;return b;}
static inline uint16_t mk(bool s,bool r,bool p,uint8_t d){uint8_t i=(~rev8(d))&0xFF;return(s?1:0)|((r?1:0)<<1)|(((uint16_t)i)<<2)|((p?1:0)<<10);}
static void sl(){gpio_set_dir(2,GPIO_OUT);gpio_put(2,0);}static void sh(){gpio_set_dir(2,GPIO_IN);}static void cl(){gpio_put(3,0);}static void ch(){gpio_put(3,1);}static void d5(){delayMicroseconds(5);}
static bool wb(uint8_t b){for(uint8_t m=0x80;m;m>>=1){if(b&m)sh();else sl();d5();ch();d5();cl();}sh();d5();ch();d5();bool a=gpio_get(2);cl();d5();return !a;}
static uint8_t rb2(bool l){uint8_t v=0;sh();for(int i=0;i<8;i++){ch();d5();v=(v<<1)|(gpio_get(2)?1:0);cl();d5();}if(l)sh();else sl();d5();ch();d5();cl();d5();sh();return v;}

void gpio_test(){
    gpio_init(2);gpio_set_dir(2,GPIO_IN);gpio_pull_up(2);gpio_init(3);gpio_set_dir(3,GPIO_OUT);gpio_put(3,1);
    sl();d5();cl();d5();wb(0xEC);wb(0xE0);wb(0xB6);sl();d5();ch();d5();sh();d5();delay(20);
    sl();d5();cl();d5();wb(0xEC);wb(0xF4);wb(0x25);sl();d5();ch();d5();sh();d5();delay(50);
    start_sampling();
    sl();d5();cl();d5();wb(0xEC);wb(0xF7);sl();d5();ch();d5();sh();d5();
    sl();d5();cl();d5();wb(0xED);uint8_t raw[8];for(int i=0;i<8;i++)raw[i]=rb2(i==7);sl();d5();ch();d5();sh();d5();
    while(sm_active)delay(1);
    Serial.println("--- GPIO ---");analyze();
    Serial.print("RAW:");for(int i=0;i<8;i++){Serial.print(" ");Serial.print(raw[i],HEX);}Serial.println();
}

void pio_test(){
    PIO pio=pio0;int off=pio_add_program(pio,&i2c_master_program);int sm=pio_claim_unused_sm(pio,false);
    pio_sm_config c=i2c_master_program_get_default_config(off);
    sm_config_set_out_pins(&c,2,1);sm_config_set_set_pins(&c,2,1);sm_config_set_in_pins(&c,2);sm_config_set_sideset_pins(&c,3);
    sm_config_set_in_shift(&c,false,true,8);sm_config_set_clkdiv(&c,96.15f);
    pio_sm_init(pio,sm,off,&c);pio_sm_set_pindirs_with_mask(pio,sm,(1u<<3),(1u<<2)|(1u<<3));
    gpio_pull_up(2);gpio_set_function(2,GPIO_FUNC_PIO0);gpio_set_function(3,GPIO_FUNC_PIO0);
    const int N=8;uint32_t cmds[N*4],buf[N*4]={0};
    for(int i=0;i<N;i++){uint8_t r=0xF7+i;cmds[i*4+0]=mk(1,0,0,0xEC);cmds[i*4+1]=mk(0,0,0,r);cmds[i*4+2]=mk(1,0,0,0xED);cmds[i*4+3]=mk(0,1,1,0xFF);}
    int rx=dma_claim_unused_channel(false);dma_channel_config rc=dma_channel_get_default_config(rx);
    channel_config_set_transfer_data_size(&rc,DMA_SIZE_32);channel_config_set_read_increment(&rc,false);channel_config_set_write_increment(&rc,true);
    channel_config_set_dreq(&rc,pio_get_dreq(pio,sm,false));dma_channel_configure(rx,&rc,buf,&pio->rxf[sm],N*4,true);
    int tx=dma_claim_unused_channel(false);dma_channel_config tc=dma_channel_get_default_config(tx);
    channel_config_set_transfer_data_size(&tc,DMA_SIZE_32);channel_config_set_read_increment(&tc,true);channel_config_set_write_increment(&tc,false);
    channel_config_set_dreq(&tc,pio_get_dreq(pio,sm,true));dma_channel_configure(tx,&tc,&pio->txf[sm],cmds,N*4,false);
    start_sampling();
    pio_sm_set_enabled(pio,sm,true);dma_start_channel_mask((1u<<tx)|(1u<<rx));while(dma_channel_is_busy(rx))tight_loop_contents();pio_sm_set_enabled(pio,sm,false);
    while(sm_active)delay(1);
    Serial.println("--- PIO ---");analyze();
    uint8_t raw[8];for(int i=0;i<N;i++)raw[i]=rev8(buf[i*4+3]&0xFF);
    Serial.print("RAW:");for(int i=0;i<8;i++){Serial.print(" ");Serial.print(raw[i],HEX);}Serial.println();
    dma_channel_unclaim(tx);dma_channel_unclaim(rx);
}

void setup(){
    Serial.begin(115200);for(int i=0;i<60&&!Serial;i++)delay(100);delay(500);Serial.println("=== Logic Analyzer ===");
    Serial.println("\n--- GPIO I2C ---");gpio_test();
    delay(500);
    Serial.println("\n--- PIO I2C ---");pio_test();
    Serial.println("\nDONE");
}
void loop(){}
