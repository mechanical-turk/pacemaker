#ifndef LOGGER_H
#define LOGGER_H

class Logger {
    public:
        
    static void log(char *c);
    static void create_log_file(char *c);
    
    private:
    
    static int logfileno;
    
};

#endif