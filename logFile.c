#include "logFile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

void createDirectories(char* share, char* download)
{
    struct stat stt = {0};

    if (stat(share, &stt) == -1)
    {
        mkdir(share, 0700);
    }

    if (stat(download, &stt) == -1)
    {
        mkdir(download, 0700);
    }

}

void createNecessaryFiles(char* path)
{
    struct stat stt = {0};

    if (stat(path, &stt) == -1)
    {
        printf("Bledna sciezka do katalogu. \n");
        exit(0);
    }

    sharedDirectoryPath = malloc(strlen(path) + strlen("/udostep/") +1);
    strcpy(sharedDirectoryPath, path);
    strcat(sharedDirectoryPath, "/udostep/");

    downloadDirectoryPath = malloc(strlen(path) + strlen("/pobrane/") +1);
    strcpy(downloadDirectoryPath, path);
    strcat(downloadDirectoryPath, "/pobrane/");

    createDirectories(sharedDirectoryPath, downloadDirectoryPath);

    createLogFile(path);

    printf("Katalog udostepniony ustawiony na: %s \n", sharedDirectoryPath);
    printf("Katalog pobranych plikow ustawiony na: %s \n", downloadDirectoryPath);
    printf("Plik logow ustawiony na: %s \n", logFilePath);

}

void createLogFile(char* path)
{
    // char* logFileName = "/logs.txt";
    logFilePath = malloc(strlen(path) + strlen("/logs.txt") +1);

    strcpy(logFilePath, path);
    strcat(logFilePath, "/logs.txt");


    FILE* fp = fopen(logFilePath, "ab+");
    fclose(fp);
}

int writeToLogFile(Info *temp, int miejsce)
{
    int offset;
    sem_P(fileSemID);
    int fd = open(logFilePath, O_WRONLY);

    //printf("sciezka : %s miejsce: %d\n", logFilePath, miejsce);

    time_t now;
    time(&now);

    if(miejsce == -1 )
    {
        lseek(fd, 0, SEEK_END);
        write(fd, "Plik: ", 6);
        write(fd, temp->filename, strlen(temp->filename));
        offset = lseek(fd, 0, SEEK_CUR);

        if(temp->status == 1)
            write(fd, "\tniezakonczone pobieranie\tdata: ", 32);
        else if(temp->status == 3)
            write(fd, "\tniezakonczone wysylanie \tdata: ", 32);

        write(fd, ctime(&now), 25);
        write(fd, "\n", 1);
        close(fd);
        sem_V(fileSemID);
        return offset;
    }
    else
    {
        lseek(fd, miejsce, SEEK_SET);
        if(temp->status == 2)
        {
            write(fd, "\tzakonczone pobieranie   \tdata: ", 32);
        }
        else if(temp->status == 4)
        {
            write(fd, "\tzakonczone wysylanie    \tdata: ", 32);
        }
         write(fd, ctime(&now), 25);
        write(fd, "\n", 1);
        close(fd);
        sem_V(fileSemID);
        return -1;
    }
}
