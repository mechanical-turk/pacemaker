#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "mbed.h"
#include "rtos.h"

class Keyboard {
    private:
    // Keyboard Input
    Serial * pc;
    public:
    char command[20];
    int command_index;
    char last_keyboard;
    
    Keyboard(Serial * _pc);
    
    static bool my_strequal(char *c1, char *c2, int buffer_size);
    
    void reset_command();
    
    void prompt();
    
    void read_char(char c);
    
    bool command_complete();
};

#endif