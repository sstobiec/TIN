#include "semaphore.h"
#include "sharedMemory.h"
#include "localFunc.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>

int main(int argc, char* argv[])
{
    struct sigaction action;

    action.sa_handler = termination_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction (SIGINT, &action, NULL);

    // inicjalizuje semafor
    semID = sem_create((int) SEM_KEY);
    sem_init(semID);
    // inicjalizacja semafora dla pliku logow
    fileSemID = sem_create((int) SEM_FILE_KEY);
    sem_init(fileSemID);
    // inicjalizuje pamięć dzieloną
    shmID = shm_init((int) SHM_ID,(int) ROZM_PAM);
    shmptr = (char*)attach_segment( shmID );

    // zapisuje struktury typu Info w pamieci dzielonej
    sem_P(semID);
    memcpy(shmptr, buf, MAXINFO*sizeof(Info));
    sem_V(semID);

    char *command = NULL; // komenda podawana przez uzytkownika
    char *ptr; // wsk na nazwe pliku
    size_t sizecommand; // rozmiar komendy ( nieuzywany - tylko do wywolania getline)
    pid_t listenerPID = 0;

    createNecessaryFiles(argv[1]);


    while(1)
    {
        printf(">");
        getline(&command, &sizecommand, stdin); // pobranie komendy
        //zamiana 'entera' na koncu stringa na '\0'
        if (command [ strlen(command) - 1 ] == '\n' )
            command [ strlen(command) - 1 ] = '\0';

        if(strstr(command,"find")!=NULL) // jesli w stringu jest find
        {

            ptr = strchr(command, '"'); // znajdz 'spacje'
            if(ptr == NULL)
            {

                printf("Niepoprawna nazwa\n");
                continue;
            }
            char * temp;
            temp = strchr(ptr+1, '"');
            if(temp== NULL)
            {

                printf("Niepoprawna nazwa\n");
                continue;
            }
            *temp = '\0';
        
            printf("Nazwa pliku %s\n", ptr+1);
            findNetwork(ptr+1);
            free(command);



        }
        else if(strcmp(command, "connect") == 0)
        {
            if(connected == 0)
            {
                connected = 1;
                listenerPID = fork();
                if(listenerPID == 0)
                {
                    setpgid(0, 0);
                    listener();
                }
                else printf("connected \n");
            }
            else printf("already connected \n");
        }
        else if(strcmp(command, "disconnect") == 0)
        {
            if(checkTransfers(shmptr, 0) == 1)
            {
                printf("Trwaja transfery. Czy chcesz mimo to zakonczyc tranfery?\n");
                getline(&command, &sizecommand, stdin );
                command[strlen(command)-1] ='\0';
                printf("%s \n", command);
                if(command[0] != 'T')
                {
                    continue;
                }
            }
            if(connected == 1)
            {
                kill(- listenerPID, SIGQUIT);
                wait();
                printf("disconnected \n");
                connected =0;
            }
            else printf("not connected \n");
        }
        else if(strcmp(command, "info") == 0)
        {
            sem_P(semID);
            info(shmptr);
            sem_V(semID);
            printf("info \n");
        }
        else if(strcmp(command, "exit") == 0)
        {
            if(checkTransfers(shmptr, 1) == 1)
            {
                printf("Trwaja transfery. Czy chcesz mimo to zakonczyc?\n");
                getline(&command, &sizecommand, stdin );
                command[strlen(command)-1] ='\0';
                if(command[0] != 'T')
                {
                    continue;
                }
            }

            if(connected == 1)
            {
                kill(- listenerPID, SIGQUIT);
                wait();
                signal(SIGQUIT, SIG_IGN);
                kill(-getpid(), SIGQUIT);
            }
            closeProgram();
        }
        else
        {
            printf("Nie ma takiej komendy. \n");
        }
        command = NULL;
        free(command);
    }
    return 0;
}
