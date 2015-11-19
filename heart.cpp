#include "mbed.h"
#include "TextLCD.h"
#include "rtos.h"
#include <stdlib.h>

// Define the LCD output for this code
TextLCD lcd(p15, p16, p17, p18, p19, p20, TextLCD::LCD16x2);
// Keyboard Input
Serial pc(USBTX, USBRX);
DigitalOut ap_led(LED1);
DigitalOut as_led(LED2);
DigitalOut vp_led(LED3);
DigitalOut vs_led(LED4);
bool mode_switch_input = false;
bool led_input = false;
char last_keyboard = ' ';

void timer0_init(void)
{
    LPC_SC->PCONP |= 1<1;           //timer0 power on
    LPC_TIM0->MR0 = 239800;         //10 msec interrupt, based on clock rate
    LPC_TIM0->MCR = 3;              //interrupt and reset control
    NVIC_EnableIRQ(TIMER0_IRQn);    //enable timer0 interrupt
    LPC_TIM0->TCR = 1;              //enable Timer0
}

void input_thread(void const * args) {
    while (true) {
        char c = pc.getc();
        last_keyboard = c;
        mode_switch_input = true;
    }
}

void mode_switch_thread(void const * args) {
    while (true) {
        if (mode_switch_input) {
            // TODO process input
            mode_switch_input == false
        }
        Thread::wait(300);
    }
        
}

void heart_thread(void const * args) {
}

void light_led(DigitalOut channel, int duration) {
    channel = 1;
    Thread::wait(duration);
    channel = 0;
}

void leds_thread(void const * args) {
    while (true) {
        if (led_input) {
            switch (last_keyboard) {
                case 'a' :
                    Thread light_led(ap_led, 200); 
                    break;
                case 's' :
                    Thread light_led(as_led, 200); 
                    break;
            }
            led_input = false;
        }
    }
}

void alarm_thread(void const * args) {
}

void display_thread(void const * args) {
}

int main() {
    timer0_init();
    Thread keyboard(input_thread);
    Thread mode_switch(mode_switch_thread);
    Thread heart(heart_thread);
    Thread alarm(alarm_thread);
    Thread leds(led_thread);
    Thread display(display_thread);
    
    while (1) { }
}