#include "mbed.h"
#include "TextLCD.h"
#include "rtos.h"
#include <stdlib.h>

// Define the LCD output for this code
TextLCD lcd(p15, p16, p17, p18, p19, p20, TextLCD::LCD16x2);
// Keyboard Input
Serial pc(USBTX, USBRX);
DigitalOut led1(LED1);

void input_thread(void const * args) {
    while (true) {
        char c = pc.getc();
        // TODO: Character input
        if (c == 'c') {
            led1 = 1;
        } else {
            led1 = 0;
        }
    }
}

int main() {
    Thread keyboard(input_thread);
}