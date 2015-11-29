#include "logger.h"
LocalFileSystem local("local");

int Logger::logfileno = 0;
FILE* Logger::logfile = NULL;

void Logger::create_log_file(char *c) {
    char filename[64];    
    int n = 0;
    
    while(1) {
        sprintf(filename, "/local/log%03d.txt", n);
        FILE *fp = fopen(filename, "r");
        if(fp == NULL) {
            break;
        }
        fclose(fp);
        n++;
    }
    
    Logger::logfileno = n;
    Logger::logfile = fopen(filename, "w");
    fprintf(Logger::logfile, "%s \n",c);
    fprintf(Logger::logfile, "Log #%d \n",n);
}

void Logger::log(char *c) {
    fprintf(Logger::logfile,"%s\n", c);
}

void Logger::close_log_file() {
    fclose(Logger::logfile);    
}