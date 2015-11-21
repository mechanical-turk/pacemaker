#include "mbed.h"
#include "TextLCD.h"
#include "rtos.h"
#include <stdlib.h>

#define AP          0x0001
#define AS          0x0002
#define VP          0x0004
#define VS          0x0008
#define TO_RANDOM   0x0010
#define TO_MANUAL   0x0020
#define TO_TEST     0x0040
#define MANUAL_AP   0x0080
#define MANUAL_VP   0x0100

#define RANDOM 0
#define MANUAL 1  
#define TEST 2

#define AP_PIN p5
#define AS_PIN p6
#define VP_PIN p7
#define VS_PIN p8

// Define the LCD output for this code
TextLCD lcd(p15, p16, p17, p18, p19, p20, TextLCD::LCD16x2);
// Keyboard Input
Serial pc(USBTX, USBRX);
DigitalOut ap_led(LED1);
DigitalOut as_led(LED2);
DigitalOut vp_led(LED3);
DigitalOut vs_led(LED4);
// Communication
DigitalOut as_out(AS_PIN);
DigitalOut vs_out(VS_PIN);
InterruptIn ap_interrupt(AP_PIN);
InterruptIn vp_interrupt(VP_PIN);
int mode = RANDOM;
bool mode_switch_input = false;
char last_keyboard = ' ';
Thread * led_addr;

void a_pace() {
    led_addr->signal_set(AP);
}

void v_pace() {
    led_addr->signal_set(VP);
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
            mode_switch_input = false;
        }
        Thread::wait(300);
    }
        
}

void heart_thread(void const * args) {
    while (true) {
        if (mode == RANDOM) {
            int target;
            int next = rand() % 2000;
            osEvent sig = Thread::signal_wait(0x00, next);
            int signum = sig.value.signals;
            if (signum & TO_MANUAL) {
                mode = MANUAL;
            } else if (signum & TO_TEST) {
                mode = TEST;
            } else {
                target = rand() % 2;
                if (target) {
                    target = AS;
                    as_out = 1;
                    Thread::wait(20);
                    as_out = 0;
					lcd.printf("AS\n\n");
                } else {
                    target = VS;
                    vs_out = 1;
                    Thread::wait(20);
                    vs_out = 0;
					lcd.printf("VS\n\n");
                }
				led_addr->signal_set(target);
            }
        } else if (mode == MANUAL) {
            
        } else if (mode == TEST) {
            
        }
    }
}

void led_thread(void const * args) {
    while (true) {
        osEvent sig = Thread::signal_wait(0x00);
        int signum = sig.value.signals;
        if (signum & AP) {
            ap_led = 1;
            Thread::wait(100);
            ap_led = 0;
        } else if (signum & AS) {
            as_led = 1;
            Thread::wait(100);
            as_led = 0;
        } else if (signum & VP) {
            vp_led = 1;
            Thread::wait(100);
            vp_led = 0;
        } else if (signum & VS) {
            vs_led = 1;
            Thread::wait(100);
            vs_led = 0;
        }
    }
}

void alarm_thread(void const * args) {
}

void display_thread(void const * args) {
}

int main() {
    // Assign interrupts
    ap_interrupt.rise(&a_pace);
    vp_interrupt.rise(&v_pace);
    // Initialize the threads
    Thread leds(led_thread);
    led_addr = &leds;
    Thread keyboard(input_thread);
    Thread mode_switch(mode_switch_thread);
    Thread heart(heart_thread);
    Thread alarm(alarm_thread);
    Thread display(display_thread);
    
    while (1) { }
}