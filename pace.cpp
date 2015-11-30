#include "mbed.h"
#include "TextLCD.h"
#include "rtos.h"
#include "keyboard.h"
#include <stdlib.h>
#include <algorithm>

#define AP			0x01
#define AS			0x02
#define VP			0x04
#define VS			0x08
#define TO_NORMAL	0x0010
#define TO_EXERCISE	0x0020
#define TO_SLEEP	0x0040
#define TO_MANUAL	0x0080
#define MANUAL_AP	0x0100
#define MANUAL_VP	0x0200
#define INTERVAL_CHANGE	0x0400

#define AVI_max 100
#define AVI_min 30
#define PVARP 500
#define VRP 500 

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

enum Pacemode { NORMAL, SLEEP, EXERCISE, MANUAL };
Pacemode pace_mode = NORMAL;

bool mode_switch_input = false;
bool manual_signal_input = false;
char user_input = '~';
char last_keyboard = ' ';

int LRI[] = {2000, 1500, 600, 2000};
int URI[] = {1000, 600, 343, 343};

Keyboard * keyboard;

Timer cA;
Timer cV;
Thread * led_addr;
Thread * display_addr;
Thread * alarm_addr;
Thread * pace_addr;

int observation_interval = 10000;

void a_sense() {
    led_addr->signal_set(AS);
    if (pace_mode != MANUAL) pace_addr->signal_set(AS);
}

void v_sense() {
    led_addr->signal_set(VS);
    display_addr->signal_set(VS);
    alarm_addr->signal_set(VS);
    if (pace_mode != MANUAL) pace_addr->signal_set(VS);
}
void set_observation_interval() {
    int i = 1;
    observation_interval = 0;
    while (keyboard->command[i] != '~') {
        observation_interval = observation_interval * 10 + (keyboard->command[i] - '0');
        i ++;
    }
	display_addr->signal_set(INTERVAL_CHANGE);
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
        if (pace_mode == MANUAL && user_input == 'v') {
            pace_addr->signal_set(MANUAL_VP);
        } else if (pace_mode == MANUAL && user_input == 'a') {
            pace_addr->signal_set(MANUAL_AP);
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
            if (user_input == 'n' || user_input == 'N') {
                pace_addr->signal_set(TO_NORMAL);
            } else if (user_input == 'e' || user_input == 'E') {
                pace_addr->signal_set(TO_EXERCISE);
            } else if (user_input == 's' || user_input == 'S') {
                pace_addr->signal_set(TO_SLEEP);
            } else if (user_input == 'm' || user_input == 'M') {
                pace_addr->signal_set(TO_MANUAL);
            }
            mode_switch_input = false;
        }    
    }
}

void send_AP() {
	ap_out = 1;
	Thread::wait(5);
	ap_out = 0;
}

void send_VP() {
	vp_out = 1;
	Thread::wait(5);
	vp_out = 0;
}

void pace_thread(void const * args) {
    bool vnext = 0;
    while (true) {
        if (pace_mode == MANUAL) {
            osEvent sig = Thread::signal_wait(0x00);
            int signum = sig.value.signals;
            if (signum & TO_EXERCISE) {
                pace_mode = EXERCISE;
            } else if (signum & TO_SLEEP) {
                pace_mode = SLEEP;
            } else if (signum & TO_NORMAL) {
                pace_mode = NORMAL;
            } else if (signum & MANUAL_VP) {
                led_addr->signal_set(VP);
                display_addr->signal_set(VP);
                alarm_addr->signal_set(VP);
                cV.reset();
                send_VP();
                vnext = false;
            } else if (signum & MANUAL_AP) {
                led_addr->signal_set(AP);
                cA.reset();
                send_AP();
                vnext = true;
            }
        } else {
            int next;
            if (vnext) {
                next = std::min(LRI[pace_mode]-cV.read_ms(), AVI_max-cA.read_ms());
            } else {
                next = LRI[pace_mode] - AVI_min - cV.read_ms();
            }
            next = max(next, 1);
            osEvent sig = Thread::signal_wait(0x00, next);
            int signum = sig.value.signals;
            if (signum & TO_MANUAL) {
                pace_mode = MANUAL;
            } else if (signum & TO_EXERCISE) {
                pace_mode = EXERCISE;
            } else if (signum & TO_SLEEP) {
                pace_mode = SLEEP;
            } else if (signum & TO_NORMAL) {
                pace_mode = NORMAL;
            } else if (signum & AS) {
                if (!vnext && cV.read_ms() >= PVARP) {
                    cA.reset();
                    vnext = true;
                }
            } else if (signum & VS) {
                if (vnext && (cV.read_ms() >= URI[pace_mode]) &&
                    (cV.read_ms() >= VRP) && (cV.read_ms() >= AVI_min)) {
                    cV.reset();
                    vnext = false;
                }
            } else {
                if (vnext) {
                    cV.reset();
                    send_VP();
                    led_addr->signal_set(VP);
                    display_addr->signal_set(VP);
                    alarm_addr->signal_set(VP);
                    vnext = false;
                } else {
                    cA.reset();
                    send_AP();
                    led_addr->signal_set(AP);
                    vnext = true;
                }
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
    int count = 0;
    lcd.printf("Initialized\n\n");
    t.reset();
    t.start();
    while (true) {
        osEvent sig = Thread::signal_wait(0x00, observation_interval - t.read_ms());
        int signum = sig.value.signals;
        if ((signum & VP) || (signum & VS)) {
            count++;
        } else if (signum & INTERVAL_CHANGE) {
			t.reset();
			count = 0;
			lcd.locate(0,0);
			lcd.printf("Initialized\n\n");
		} else {
            double bpm = (double) count * 60000.0 / (double) observation_interval;
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
	int interval = 15;
    while (true) {
        osEvent sig = Thread::signal_wait(0x00, LRI[pace_mode] - t.read_ms() + interval);
        int signum = sig.value.signals;
        if ((signum & VP) || (signum & VS)) {
            if (!first && t.read_ms() < URI[pace_mode]) {
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
    // Initialize keyboard
    keyboard = new Keyboard(&pc);
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
    pace_addr = &pace;
    
    while (true) { }
}