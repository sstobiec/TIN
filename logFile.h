#ifndef LOGFILE_H
#define LOGFILE_H
#include <time.h>
typedef struct
{
    int status;         // status 0 - wolny, 1 - w trakcie sciagania, 2 - sciagniete, 3 - w trakcie wysylania, 4 - wyslane
    int progress;       // jaki jest postep
    char filename[30];
} Info;

int fileSemID;
char* logFilePath;
char* sharedDirectoryPath;
char* downloadDirectoryPath;

int writeToLogFile(Info *temp, int miejsce);

void createLogFile(char* path);

void createNecessaryFiles(char* path);

void createDirectories(char* shared, char* download);

#endif
