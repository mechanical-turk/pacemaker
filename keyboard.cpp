#include "keyboard.h"

Keyboard::Keyboard(Serial * _pc) {
    pc = _pc;
    last_keyboard = '~';
}

bool Keyboard::my_strequal(char *c1, char *c2, int buffer_size) {
    for (int i = 0; i < buffer_size; i++) {
        if (c1[i] != c2[i]) {
            return false;
        }
    }
    
    return true;
}

void Keyboard::reset_command() {
    for (int i = 0; i < 20; i++) {
        command[i] = '~';
    }
    command_index = 0;
}

void Keyboard::prompt() {
    pc->puts("\n\r$ ");
}

void Keyboard::read_char(char c) {
    if (command_index < 20 && c != '\r') {
        command[command_index] = c;
        command_index ++;
    }
}

bool Keyboard::command_complete() {
    return (command[0] != '~' && 
        (!
            (command[0] == 'o' || 
            command[0] == 'O' ||
            command[0] == 'h' ||
			command[0] == 'H'
            )
        )
    );
}