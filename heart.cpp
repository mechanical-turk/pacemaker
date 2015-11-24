#include "mbed.h"
#include "TextLCD.h"
#include "rtos.h"
#include "keyboard.h"
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

enum Heartmode { RANDOM, MANUAL, TEST };
Heartmode heart_mode = RANDOM;

bool mode_switch_input = false;
bool manual_signal_input = false;
char user_input = '~';
char last_keyboard = ' ';

Thread * led_addr;
Thread * display_addr;
Thread * heart_addr;
Keyboard * keyboard;

int observation_interval = 0;

void a_pace() {
    led_addr->signal_set(AP);
}

void v_pace() {
    led_addr->signal_set(VP);
    display_addr->signal_set(VP);
}

void set_observation_interval() {
    int i = 1;
    observation_interval = 0;
    while (keyboard->command[i] != '~') {
        observation_interval = observation_interval * 10 + (keyboard->command[i] - '0');
        i ++;
    }
}

void interpret_command() {
    if(keyboard->command[0] == 'o') {
        set_observation_interval();
        pc.printf("\n\rObservation interval set to: %d", observation_interval);
    } else if (Keyboard::my_strequal(keyboard->command,"help",4)) {
        pc.printf("THIS IS HELP");
    } else {
        user_input = keyboard->command[0];
        mode_switch_input = true;
		if (heart_mode == MANUAL && user_input == 'v') {
			heart_addr->signal_set(MANUAL_VP);
		} else if (heart_mode == MANUAL && user_input == 'a') {
			heart_addr->signal_set(MANUAL_AP);
		}
    }
}

void input_thread(void const * args) {
    keyboard->prompt();
    keyboard->reset_command();
    while(1) {
        keyboard->last_keyboard = pc.getc();
        keyboard->read_char(keyboard->last_keyboard);
        
        if (keyboard->last_keyboard != '\r') {
            pc.putc(keyboard->last_keyboard);
        }
        
        if (keyboard->last_keyboard == '\r' || keyboard->command_complete()) {
            interpret_command();
            keyboard->reset_command();
            keyboard->prompt();
        }
    }
}

void mode_switch_thread(void const * args) {
    while(1) {
        if (mode_switch_input) {
            if (user_input == 'r' || user_input == 'R') {
				heart_addr->signal_set(TO_RANDOM);
            } else if (user_input == 'm' || user_input == 'M') {
				heart_addr->signal_set(TO_MANUAL);
            } else if (user_input == 't' || user_input == 'T') {
				heart_addr->signal_set(TO_TEST);
            }
            lcd.printf("Mode: %d, ",heart_mode);
            mode_switch_input = false;
        }    
    }
}

void wait_v() {
    while (true) {
        osEvent sig = Thread::signal_wait(0x00);
        int signum = sig.value.signals;
        if (signum & AP) break;
    }
    while (true) {
        osEvent sig = Thread::signal_wait(0x00);
        int signum = sig.value.signals;
        if (signum & VP) break;
    }
}


void heart_thread(void const * args) {
    while (true) {
        if (heart_mode == RANDOM) {
            int target;
            int next = rand() % 2000;
            osEvent sig = Thread::signal_wait(0x00, next);
            int signum = sig.value.signals;
            if (signum & TO_MANUAL) {
                heart_mode = MANUAL;
            } else if (signum & TO_TEST) {
                heart_mode = TEST;
            } else {
                target = rand() % 2;
                if (target) {
                    target = AS;
                    as_out = 1;
                    Thread::wait(20);
                    as_out = 0;
                } else {
                    target = VS;
                    vs_out = 1;
                    Thread::wait(20);
                    vs_out = 0;
                    display_addr->signal_set(VS);
                }
                led_addr->signal_set(target);
            }
        } else if (heart_mode == MANUAL) {
            osEvent sig = Thread::signal_wait(0x00);
            int signum = sig.value.signals;
            if (signum & TO_RANDOM) {
                heart_mode = RANDOM;
            } else if (signum & TO_TEST) {
				heart_mode = TEST;
			} else if (signum & MANUAL_VP) {
				vs_out = 1;
				Thread::wait(20);
				vs_out = 0;
				display_addr->signal_set(VS);
				led_addr->signal_set(VP);
			} else if (signum & MANUAL_AP) {
				as_out = 1;
				Thread::wait(20);
				as_out = 0;
				led_addr->signal_set(AP);
			}
        } else if (heart_mode == TEST) {
            
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

int main() {
    // Initialize keyboard
    keyboard = new Keyboard(&pc);
    // Assign interrupts
    ap_interrupt.rise(&a_pace);
    vp_interrupt.rise(&v_pace);
    // Initialize the threads
    Thread leds(led_thread);
    led_addr = &leds;
    Thread display(display_thread);
    display_addr = &display;
    Thread keyboard(input_thread);
    Thread mode_switch(mode_switch_thread);
    Thread heart(heart_thread);
	heart_addr = &heart;
    
    while (1) { }
}