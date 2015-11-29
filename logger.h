#ifndef LOGGER_H
#define LOGGER_H

#include "mbed.h"

class Logger {
    public:
        
    static void log(char *c);
    static void create_log_file(char *c);
    static void close_log_file();
    
    private:
    
    static int logfileno;
    static FILE *logfile;
    
};

#endif