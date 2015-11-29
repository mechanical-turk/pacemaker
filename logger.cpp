#include "logger.h"
#include "mbed.h"
#include <string>
LocalFileSystem local("local");

int Logger::logfileno = 0;

void Logger::create_log_file(char *c) {
    char filename[64];    
    int n = 0;
    
    // set "filename" equal to the next file to write
    while(1) {
        sprintf(filename, "/local/log%03d.txt", n);    // construct the filename fileNNN.txt
        FILE *fp = fopen(filename, "r");                // try and open it
        if(fp == NULL) {                                // if not found, we're done!
            break;
        }
        fclose(fp);                                     // close the file
        n++;                                            // and try the next one
    }
    
    
    Logger::logfileno = n;
    FILE *fp = fopen(filename, "w");
    fprintf(fp, "Logfile #%d \n",n);
    fprintf(fp, "%s \n",c);
    fclose(fp);
}

void Logger::log(char *c) {
    char filename[64]; 
    sprintf(filename, "/local/log%03d.txt", Logger::logfileno); 
    FILE *fp = fopen(filename, "a");
    fprintf(fp,"%s\n", c);
    fclose(fp);
}