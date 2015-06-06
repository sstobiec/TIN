#ifndef LOCALFUNC_H
#define LOCALFUNC_H

#include "logFile.h"

#include <stdio.h>
#include <netdb.h>

#define SIZEDATAGRAM 1024
#define LISTENPORT 5556
#define SENDPORT 5559

#define RESOURCE_FOUND 'A'
#define RESOURCE_NOT_FOUND 0

#define TRANSFER_ACK 'T'

#define MYPORT 32005             // do zmiany!!!
#define ROZM_PAM 4096       // jezeli rozmiar jest wiekszy to nie dziala bo jakies ograniczenia sa
#define SHM_ID 1234
#define SEM_KEY 1111
#define SEM_FILE_KEY 2222
#define MAXINFO 100

// Kolory tekstu na wyjsciu
#define ANSI_COLOR_RED      "\x1b[31;1m"
#define ANSI_COLOR_RESET    "\x1b[0m"

char fname[300];
Info buf[MAXINFO];      // jak globalnie to nie ma smieci
int semID;
int fileSemID;
char *shmptr;
int shmID;
static int connected = 0;
static pid_t listenerPID = 0;

int sendfile(FILE* file, char* filename, struct sockaddr *to, int semId, char *shmptr);

int download(char *fileName, int sockfd, int semId, char *shmptr, struct sockaddr_in* from);

void listener();

void createLogFile(char* path);

char* attach_segment( int shmId );

void findNetwork(char* filename);

int checkSource(struct sockaddr_in sourceAddress);

void closeProgram();

void info(char* shmptr);

int checkTransfers(char* shmptr, int which);

void termination_handler(int signum);

#endif

