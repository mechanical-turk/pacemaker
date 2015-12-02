#include "mbed.h"
#include "TextLCD.h"
#include "rtos.h"
#include "keyboard.h"
#include "logger.h"
#include <stdlib.h>
#include <algorithm>

#define AP          0x0001
#define AS          0x0002
#define VP          0x0004
#define VS          0x0008
#define TO_RANDOM   0x0010
#define TO_MANUAL   0x0020
#define TO_TEST     0x0040
#define MANUAL_AS   0x0080
#define MANUAL_VS   0x0100
#define INPUT_READY 0x0200
#define INTERVAL_CHANGE 0x0400
#define TO_DYNAMIC  0x0800
#define TO_EXTENDED 0x1000

#define AVI_max 100
#define AVI_min 30
#define PVARP 500
#define VRP 500 
#define LRI 2000
#define URI 1000

// Values taken from
// https://www.bostonscientific.com/content/dam/bostonscientific/quality/education-resources/english/ACL_AVSH_20091130.pdf
#define AV_INCREASE 1.3
#define DYNAMIC_AV_MIN 80
#define DYNAMIC_AV_MAX 150

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

enum Heartmode { RANDOM, MANUAL, TEST, DYNAMIC_TEST, EXTENDED_TEST };
Heartmode heart_mode = RANDOM;

bool mode_switch_input = false;
bool manual_signal_input = false;
char user_input = '~';
char last_keyboard = ' ';

Thread * led_addr;
Thread * display_addr;
Thread * heart_addr;
Thread * log_addr;
Thread * keyboard_addr;
Keyboard * keyboard;

Timer cA;
Timer cV;
Timer t_global;

bool running = true;

int observation_interval = 10000;

void a_pace() {
    led_addr->signal_set(AP);
    log_addr->signal_set(AP);
    if (heart_mode == TEST || heart_mode == DYNAMIC_TEST || heart_mode == EXTENDED_TEST) {
        heart_addr->signal_set(AP);
    }
}

void v_pace() {
    led_addr->signal_set(VP);
    display_addr->signal_set(VP);
    log_addr->signal_set(VP);
    if (heart_mode == TEST || heart_mode == DYNAMIC_TEST || heart_mode == EXTENDED_TEST) {
        heart_addr->signal_set(VP);
    }
}

void set_observation_interval() {
    int i = 1;
    int interval = 0;
    while (keyboard->command[i] != '~') {
        interval = interval * 10 + (keyboard->command[i] - '0');
        i ++;
    }
    observation_interval = interval;
    display_addr->signal_set(INTERVAL_CHANGE);
}

void interpret_command() {
    if(keyboard->command[0] == 'o') {
        set_observation_interval();
        pc.printf("\n\rObservation interval set to: %d", observation_interval);
        keyboard_addr->signal_set(INPUT_READY);
    } else if (Keyboard::my_strequal(keyboard->command,"help",4)) {
        pc.printf("THIS IS HELP");
        keyboard_addr->signal_set(INPUT_READY);
    } else {
        user_input = keyboard->command[0];
        mode_switch_input = true;
        if (heart_mode == MANUAL && user_input == 'v') {
            heart_addr->signal_set(MANUAL_VS);
        } else if (heart_mode == MANUAL && user_input == 'a') {
            heart_addr->signal_set(MANUAL_AS);
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
            Thread::signal_wait(0x00);
            keyboard->prompt();
        }
    }
}

void mode_switch_thread(void const * args) {
    while(1) {
        if (mode_switch_input) {
            if (user_input != 't' && user_input != 'T' &&
                user_input != 'd' && user_input != 'D' &&
                user_input != 'x' && user_input != 'X') {
                keyboard_addr->signal_set(INPUT_READY);
            }
            if (user_input == 'r' || user_input == 'R') {
                heart_addr->signal_set(TO_RANDOM);
            } else if (user_input == 'm' || user_input == 'M') {
                heart_addr->signal_set(TO_MANUAL);
            } else if (user_input == 't' || user_input == 'T') {
                heart_addr->signal_set(TO_TEST);
            } else if (user_input == 'd' || user_input == 'D') {
                heart_addr->signal_set(TO_DYNAMIC);
            } else if (user_input == 'x' || user_input == 'X') {
                heart_addr->signal_set(TO_EXTENDED);
            } else if (user_input == 'q' || user_input == 'Q') {
                running = false;
                Logger::close_log_file();\
            }
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

bool wait_assert(int signal, int timeout) {
    osEvent sig;
    int signum = 0;
    if (timeout > 0) sig = Thread::signal_wait(0x00, timeout);
	else sig = Thread::signal_wait(0x00);
    signum = sig.value.signals;
    return signum == signal;
}

void wait_for(int signal) {
    osEvent sig;
    int signum = 0;
    while (signum != signal) {
        sig = Thread::signal_wait(0x00);
        signum = sig.value.signals;
    }
}

void send_AS() {
    log_addr->signal_set(AS);
    led_addr->signal_set(AS);
    as_out = 1;
    Thread::wait(5);
    as_out = 0;
}

void send_VS() {
    log_addr->signal_set(VS);
    led_addr->signal_set(VS);
    vs_out = 1;
    Thread::wait(5);
    vs_out = 0;
}

void report(bool assert) {
    if (!assert) {
        Logger::log("Test failed!");
        pc.printf("\n\rTest failed!\n\r");
    } else {
        Logger::log("Test passed!");
        pc.printf("\n\rTest passed!\n\r");
    }
}

int update_AVI(int ms) {
    return max(DYNAMIC_AV_MIN,
        min(DYNAMIC_AV_MAX, (int) (AV_INCREASE * ms)));
}

void heart_thread(void const * args) {
    while (true) {
        if (heart_mode == RANDOM) {
            int target;
            int next = rand() % 3000;
            osEvent sig = Thread::signal_wait(0x00, next);
            int signum = sig.value.signals;
            if (signum & TO_MANUAL) {
                heart_mode = MANUAL;
            } else if (signum & TO_TEST) {
                heart_mode = TEST;
            } else if (signum & TO_DYNAMIC) {
                heart_mode = DYNAMIC_TEST;
            } else if (signum & TO_EXTENDED) {
                heart_mode = EXTENDED_TEST;
            } else {
                target = rand() % 2;
                if (target) {
                    send_AS();
                } else {
                    send_VS();
                    display_addr->signal_set(VS);
                }
            }
        } else if (heart_mode == MANUAL) {
            osEvent sig = Thread::signal_wait(0x00);
            int signum = sig.value.signals;
            if (signum & TO_RANDOM) {
                heart_mode = RANDOM;
            } else if (signum & TO_TEST) {
                heart_mode = TEST;
            } else if (signum & TO_DYNAMIC) {
                heart_mode = DYNAMIC_TEST;
            } else if (signum & TO_EXTENDED) {
                heart_mode = EXTENDED_TEST;
            } else if (signum & MANUAL_VS) {
                send_VS();
            } else if (signum & MANUAL_AS) {
                send_AS();
            }
        } else if (heart_mode == TEST) {
            bool assert = true;
            int interval = 15;
            // Initialize test cases
            cA.start();
            cV.start();
            Logger::log("Test started");
            pc.printf("\n\rTest started!\n\r");
            
            // Test normal operation
            wait_for(AP);
            cA.reset();
            wait_for(VP);
            cV.reset();
            assert = assert & wait_assert(0x0000,
                max(PVARP - cV.read_ms(),
                    URI - AVI_max - cV.read_ms()));
            send_AS();
            cA.reset();
            assert = assert & wait_assert(0x0000,
                max(URI - cV.read_ms(), 
                    max(VRP - cV.read_ms(),
                        AVI_min - cA.read_ms())));
            send_VS();
            cV.reset();
            assert = assert & wait_assert(0x0000,
                LRI - AVI_max - cV.read_ms() - interval);
            send_AS();
            cA.reset();
            assert = assert & wait_assert(0x0000,
                min(LRI - cV.read_ms(),
                    AVI_max - cA.read_ms()) - interval);
            send_VS();
            cV.reset();
            report(assert);
            // Test VS exceeds time
            wait_for(AP);
            cA.reset();
            wait_for(VP);
            cV.reset();
            assert = true;
            assert = assert & wait_assert(0x0000,
                PVARP - cV.read_ms());
            send_AS();
            cA.reset();
            assert = assert & wait_assert(VP,
                min(LRI - cV.read_ms(), AVI_max - cA.read_ms()) + interval);
            cV.reset();
            send_VS();
            assert = assert & wait_assert(AP,
                LRI - AVI_min - cV.read_ms() + interval);
            cA.reset();
            report(assert);
            // Test AS exceeds time
            wait_for(VP);
            cV.reset();
            wait_for(AP);
            cA.reset();
            wait_for(VP);
            cV.reset();
            assert = true;
            assert = assert & wait_assert(AP,
                LRI - AVI_min - cV.read_ms() + interval);
            cA.reset();
            send_AS();
            assert = assert & wait_assert(VP, 
                min(LRI - cV.read_ms(),
                    AVI_max - cA.read_ms()) + interval);
            cV.reset();
            report(assert);
            // Test AS too soon
            wait_for(AP);
            cA.reset();
            wait_for(VP);
            cV.reset();
            assert = true;
            send_AS();
            assert = assert & wait_assert(AP,
                AVI_max - LRI - cV.read_ms());
            cA.reset();
            report(assert);
            // Test VS too soon
            wait_for(VP);
            cV.reset();
            wait_for(AP);
            cA.reset();
            wait_for(VP);
            cV.reset();
            assert = true;
            assert = assert & wait_assert(0x0000,
                PVARP - cV.read_ms());
            send_AS();
            cA.reset();
            send_VS();
            assert = assert & wait_assert(VP,
                min(LRI - cV.read_ms(),
                    AVI_max - cA.read_ms()) + interval);
            report(assert);
            // Tests are complete
            Logger::log("Test finished");
            cA.stop();
            cV.stop();
            cA.reset();
            cV.reset();
            pc.puts("Tests complete!\n");
            keyboard_addr->signal_set(INPUT_READY);
            heart_mode = RANDOM;
        } else if (heart_mode == DYNAMIC_TEST) {
            bool assert = true;
            int interval = 15;
            // Initialize test cases
            cA.start();
            cV.start();
            Logger::log("Dynamic Test started");
            pc.printf("\n\rDynamic Test started!\n\r");
            
            // Test normal operation
            wait_for(AP);
            cA.reset();
            wait_for(VP);
            int AVI = update_AVI(cA.read_ms());
            cV.reset();
            assert = assert & wait_assert(0x0000,
                max(PVARP - cV.read_ms(),
                    URI - AVI - cV.read_ms()));
            send_AS();
            cA.reset();
            assert = assert & wait_assert(0x0000,
                max(URI - cV.read_ms(), 
                    max(VRP - cV.read_ms(),
                        AVI_min - cA.read_ms())));
            AVI = update_AVI(cA.read_ms());
            send_VS();
            cV.reset();
            assert = assert & wait_assert(0x0000,
                LRI - AVI - cV.read_ms() - interval);
            send_AS();
            cA.reset();
            assert = assert & wait_assert(0x0000,
                min(LRI - cV.read_ms(),
                    AVI - cA.read_ms()) - interval);
            AVI = update_AVI(cA.read_ms());
            send_VS();
            cV.reset();
            report(assert);
            // Test VS exceeds time
            wait_for(AP);
            cA.reset();
            wait_for(VP);
            AVI = update_AVI(cA.read_ms());
            cV.reset();
            assert = true;
            assert = assert & wait_assert(0x0000,
                PVARP - cV.read_ms());
            send_AS();
            cA.reset();
            assert = assert & wait_assert(VP,
                min(LRI - cV.read_ms(), AVI - cA.read_ms()) + interval);
            AVI = update_AVI(cA.read_ms());
            cV.reset();
            send_VS();
            assert = assert & wait_assert(AP,
                LRI - AVI_min - cV.read_ms() + interval);
            cA.reset();
            report(assert);
            // Test AS exceeds time
            wait_for(VP);
            AVI = update_AVI(cA.read_ms());
            cV.reset();
            wait_for(AP);
            cA.reset();
            wait_for(VP);
            AVI = update_AVI(cA.read_ms());
            cV.reset();
            assert = true;
            assert = assert & wait_assert(AP,
                LRI - AVI_min - cV.read_ms() + interval);
            
            cA.reset();
            send_AS();
            assert = assert & wait_assert(VP, 
                min(LRI - cV.read_ms(),
                    AVI - cA.read_ms()) + interval);
            AVI = update_AVI(cA.read_ms());
            cV.reset();
            report(assert);
            // Test AS too soon
            wait_for(AP);
            cA.reset();
            wait_for(VP);
            AVI = update_AVI(cA.read_ms());
            cV.reset();
            assert = true;
            send_AS();
            assert = assert & wait_assert(AP,
                AVI - LRI - cV.read_ms());
            cA.reset();
            report(assert);
            // Test VS too soon
            wait_for(VP);
            AVI = update_AVI(cA.read_ms());
            cV.reset();
            wait_for(AP);
            cA.reset();
            wait_for(VP);
            AVI = update_AVI(cA.read_ms());
            cV.reset();
            assert = true;
            assert = assert & wait_assert(0x0000,
                PVARP - cV.read_ms());
            send_AS();
            cA.reset();
            send_VS();
            assert = assert & wait_assert(VP,
                min(LRI - cV.read_ms(),
                    AVI - cA.read_ms()) + interval);
            AVI = update_AVI(cA.read_ms());
            report(assert);
            // Tests are complete
            Logger::log("Test finished");
            cA.stop();
            cV.stop();
            cA.reset();
            cV.reset();
            pc.puts("Tests complete!\n");
            keyboard_addr->signal_set(INPUT_READY);
            heart_mode = RANDOM;
        } else if (heart_mode == EXTENDED_TEST) {
            bool assert = true;
            int interval = 15;
            // Initialize test cases
            cA.start();
            cV.start();
            Logger::log("Extended Test started");
            pc.printf("\n\rExtended Test started!\n\r");
            
            // Test one VS too soon, AS too soon
            wait_for(AP);
            cA.reset();
            wait_for(VP);
            cV.reset();
            assert = true;
            assert = assert & wait_assert(0x0000,
                URI - cV.read_ms());
            // Generate V before A
            send_VS();
            // Send A after PVARP, but before PVARP + EXTEND
            assert = assert & wait_assert(0x0000, PVARP);
            send_AS();
            assert = assert & wait_assert(AP, 0);
            report(assert);
            
            // Test one VS too soon, AS on time
            wait_for(AP);
            cA.reset();
            wait_for(VP);
            cV.reset();
            assert = true;
            assert = assert & wait_assert(0x0000,
                URI - cV.read_ms());
            // Generate V before A
            send_VS();
            // Send A after PVARP, but before PVARP + EXTEND
            assert = assert & wait_assert(0x0000, PVARP + 50);
            send_AS();
            assert = assert & wait_assert(VP, 0);
            report(assert);
            
            // Tests are complete
            Logger::log("Test finished");
            cA.stop();
            cV.stop();
            cA.reset();
            cV.reset();
            pc.puts("Tests complete!\n");
            keyboard_addr->signal_set(INPUT_READY);
            heart_mode = RANDOM;
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
        osEvent sig = Thread::signal_wait(0x00, max(observation_interval - t.read_ms(), 1));
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

void log_thread(void const * args) {
    char buffer[100];
    while (running) {
        osEvent sig = Thread::signal_wait(0x00);
        int signum = sig.value.signals;
        if (signum & AP) {
            if (heart_mode == TEST || heart_mode == DYNAMIC_TEST || heart_mode == EXTENDED_TEST)
                pc.printf("\n\rAP: %d", t_global.read_ms());
            sprintf(buffer, "AP: cv - %d | ca - %d | t - %d",
                cV.read_ms(), cA.read_ms(), t_global.read_ms());
            Logger::log(buffer);
        } else if (signum & VP) {
            if (heart_mode == TEST || heart_mode == DYNAMIC_TEST || heart_mode == EXTENDED_TEST)
                pc.printf("\n\rVP: %d", t_global.read_ms());
            sprintf(buffer, "VP: cv - %d | ca - %d | t - %d",
                cV.read_ms(), cA.read_ms(), t_global.read_ms());
            Logger::log(buffer);
        } else if (signum & AS) {
            if (heart_mode == TEST || heart_mode == DYNAMIC_TEST || heart_mode == EXTENDED_TEST)
                pc.printf("\n\rAS: %d", t_global.read_ms());
            sprintf(buffer, "AS: cv - %d | ca - %d | t - %d",
                cV.read_ms(), cA.read_ms(), t_global.read_ms());
            Logger::log(buffer);
        } else if (signum & VS) {
            if (heart_mode == TEST || heart_mode == DYNAMIC_TEST || heart_mode == EXTENDED_TEST)
                pc.printf("\n\rVS: %d", t_global.read_ms());
            sprintf(buffer, "VS: cv - %d | ca - %d | t - %d",
                cV.read_ms(), cA.read_ms(), t_global.read_ms());
            Logger::log(buffer);
        }
    }
}

int main() {
    t_global.start();
    Logger::create_log_file("LOG FILE");
    Thread log(log_thread);
    log_addr = &log;
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
    keyboard_addr = &keyboard;
    Thread mode_switch(mode_switch_thread);
    Thread heart(heart_thread);
    heart_addr = &heart;
    
    while (true) { }
}