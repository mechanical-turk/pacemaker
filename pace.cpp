#include "mbed.h"
#include "TextLCD.h"
#include "rtos.h"
#include <stdlib.h>

#define AP 0x01
#define AS 0x02
#define VP 0x04
#define VS 0x08

#define AVI_max 100
#define AVI_min 30
#define PVARP 500
#define VRP 500

#define NORMAL 0
#define SLEEP 1 
#define EXERCISE 2
#define MANUAL 3 

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
InterruptIn as_interrupt(AS_PIN);
InterruptIn vs_interrupt(VS_PIN);
DigitalOut ap_out(AP_PIN);
DigitalOut vp_out(VP_PIN);
int LRI[] = {2000, 1500, 600, 2000};
int URI[] = {1000, 600, 343, 343};
int mode = EXERCISE;
bool mode_switch_input = false;
char last_keyboard = ' ';
Timer cA;
Timer cV;
Thread * led_addr;
Thread * display_addr;
Thread * alarm_addr;

void a_sense() {
    led_addr->signal_set(AS);
}

void v_sense() {
    led_addr->signal_set(VS);
    display_addr->signal_set(VS);
	alarm_addr->signal_set(VS);
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

void pace_thread(void const * args) {
    bool state = 1;
    while (true) {
        if (state) {
            if (cV.read_ms() > LRI[mode] - AVI_min) {
                led_addr->signal_set(AP);
                ap_out = 1;
                cA.reset();
                Thread::wait(20);
                ap_out = 0;
                state = 0;
            }
        } else {
            if (cV.read_ms() > LRI[mode] || cA.read_ms() > AVI_max) {
                led_addr->signal_set(VP);
                display_addr->signal_set(VP);
				alarm_addr->signal_set(VP);
                vp_out = 1;
                cV.reset();
                Thread::wait(20);
                vp_out = 0;
                state = 1;
            }
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

void display_thread(void const * args) {
    Timer t;
    int interval = 10000;
    int count = 0;
    lcd.printf("Initialized\n\n");
    t.reset();
    t.start();
    while (true) {
        osEvent sig = Thread::signal_wait(0x00, interval - t.read_ms());
        int signum = sig.value.signals;
        if ((signum & VP) || (signum & VS)) {
            count++;
        } else {
            double bpm = (double) count * 60000.0 / (double) interval;
			lcd.locate(0,0);
            lcd.printf("%f BPM", bpm);
            count = 0;
            t.reset();
        }
    }
}

void alarm_thread(void const * args) {
	Timer t;
	t.reset();
	t.start();
	bool first = true;
	while (true) {
        osEvent sig = Thread::signal_wait(0x00, LRI[mode] - t.read_ms());
        int signum = sig.value.signals;
        if ((signum & VP) || (signum & VS)) {
			if (!first && t.read_ms() < URI[mode]) {
				lcd.locate(0, 1);
				lcd.printf("ERR_FAST");
				Thread::wait(5000);
				lcd.locate(0, 1);
				lcd.printf("        ");
				t.reset();
				first = true;
			} else {
				t.reset();
                first = false;
			}
		} else {
			lcd.locate(0, 1);
			lcd.printf("ERR_SLOW");
			Thread::wait(5000);
			lcd.locate(0, 1);
			lcd.printf("        ");
			t.reset();
			first = true;
		}
	}
}

int main() {
    // Initialize the clocks to some reasonable time
    cA.reset();
    cA.start();
    int startup = rand() % 70 + 30;
    Thread::wait(startup);
    cV.reset();
    cV.start();
    // Assign interrupts
    as_interrupt.rise(&a_sense);
    vs_interrupt.rise(&v_sense);
    // Initialize the threads
    Thread leds(led_thread);
    led_addr = &leds;
    Thread display(display_thread);
    display_addr = &display;
    Thread alarm(alarm_thread);
	alarm_addr = &alarm;
    Thread keyboard(input_thread);
    Thread mode_switch(mode_switch_thread);
    Thread pace(pace_thread);
    
    while (1) { }
}