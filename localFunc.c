#include "localFunc.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ifaddrs.h>

int sendfile(FILE* file, char* filename, struct sockaddr *to, int semId, char *shmptr)
{
    long fileSize; // rozmiar pliku
    int datagramNumber; // ilosc datagramow
    int ack; // numer potwierdzanego pakietu
    int sockfd; //socket
    int i = 0; // iterator po datagramach
    char * buffer; // buforach dla danych 
    int index;
    Info tmpinfo;
    FILE* test;

    // sciezka do pliku ktory wysylamy
    char filePath[(strlen(sharedDirectoryPath) + strlen(filename) +1)];
    strcpy(filePath, sharedDirectoryPath);
    strcat(filePath, filename);

    if(file == NULL)    return -1;

    struct timeval sendtimeout; // ustawiamy timeout
    sendtimeout.tv_sec = 2;
    sendtimeout.tv_usec = 500000;

    fseek(file,0, SEEK_END); // przesuniecie na koniec pliku
    fileSize = ftell(file); // odczyt rozmiaru

    fseek(file,0, SEEK_SET); // powrot na poczatek

    datagramNumber = fileSize / (SIZEDATAGRAM-sizeof(int)); // ilosc datagramow
    if(fileSize % (SIZEDATAGRAM-sizeof(int)) != 0) datagramNumber++; // jesli z reszta to +1

    if(fileSize <= 0)   return -1;

    fileSize = fileSize % (SIZEDATAGRAM-sizeof(int));

    buffer = malloc(SIZEDATAGRAM);
    memcpy(buffer, &i, sizeof(int)); // numer ( 0 ) datagramu
    memcpy(buffer+sizeof(int), &fileSize, sizeof(int)); // ilosc datagramow
    memcpy(buffer+2*sizeof(int), &datagramNumber, sizeof(int)); // ladujemy rozmiar pliku

    sockfd = socket(AF_INET, SOCK_DGRAM,0); // utworzenie gniazda

    //ustawienie funkcji socketa - timeout'u
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&sendtimeout,sizeof(sendtimeout));

    //w przypadku braku potw, co 3s wysylamy datagram jeszcze raz
    int k;
    for(k = 0; k < 3; ++k)
    {
        sendto(sockfd, buffer, 3*sizeof(int), 0, to, sizeof(*to)); // wyslanie

        if(recvfrom(sockfd, buffer, sizeof(int), 0, NULL, NULL) < 0) // jesli po 3 sek nie ma odpowiedzi
        {
            if( k == 2) // jesli wyslalismy juz 3 razy
            {
                printf(ANSI_COLOR_RED "3 proby nieudane.Zamykam polaczenie... \n" ANSI_COLOR_RESET);
                close(sockfd);
                free(buffer);
                return -1;
            }
            printf(ANSI_COLOR_RED "Blad polaczenia. Ponawiam transfer pakietu... \n" ANSI_COLOR_RESET);
        }
        else
        {
            // odczyt numer pakietu
            memcpy(&ack, buffer, sizeof(int)); // zapis z bufora do zmiennej
            if(ack == 0) break;// jesli odebralismy dobre potwierdzenie to wychodzimy z petli
        }
    }

    // uzupelnienie logow
    tmpinfo.status = 3;
    strcpy(tmpinfo.filename, filename);
    tmpinfo.progress = 0;

    // to musi byc tutaj + w sekcji krytycznej - musze zajac index i go w sekcji zapisac w sekcji
    sem_P(semId);
    index = findIndex(shmptr);
    memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
    sem_V(semId);

    int offset= writeToLogFile(&tmpinfo,-1);

    // petla - ilosc obiegow - ilosc datagramow
    for(i =1; i <= datagramNumber; ++i)
    {

        // co 10 przebieg sprawdzamy czy plik nie zostal usuniety
        if(i % 10 == 10)
        {
            test = fopen(filePath, "r");
            if(test == NULL )
            {
                tmpinfo.status = 0;
                sem_P(semId);
                memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
                sem_V(semId);
                close(sockfd);
                free(buffer);
                fclose(test);
                fclose(file);
                return -2;
            }
            fclose(test);
        }

        // przemieszczamy sie po pliku
        fseek(file, (SIZEDATAGRAM-sizeof(int))*(i-1), SEEK_SET);

        // jesli to ostatni datagram
        if(i == datagramNumber)
        {
            fread(buffer+sizeof(int), 1, fileSize, file);
        }
        else
        {
            fread(buffer+sizeof(int), 1, SIZEDATAGRAM-sizeof(int), file);
        }

        // na koniec pakietu dodajemy jego numer
        memcpy(buffer, &i, sizeof(int));

        //w przypadku braku potw, co 3s wysylamy datagram jeszcze raz
        for(k = 0; k < 3; ++k)
        {
            sendto(sockfd, buffer, SIZEDATAGRAM, 0, to, sizeof(*to)); // wyslanie

            if(recvfrom(sockfd, buffer, sizeof(int), 0, NULL, NULL) < 0) // sprawdzenie odp.
            {
                if( k == 2)
                {
                    printf(ANSI_COLOR_RED "3 nieudane proby.Zamykam polaczenie... \n" ANSI_COLOR_RESET);
                    //uzupelnienie logow
                    tmpinfo.status = 0;
                    sem_P(semId);
                    memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
                    sem_V(semId);
                    close(sockfd);
                    free(buffer);
                    return -1;
                }
                printf(ANSI_COLOR_RED "Blad polaczenia. Ponawiam transfer pakietu... \n" ANSI_COLOR_RESET);
            }
            else
            {
                // odczyt numeru potwierdzenia
                memcpy(&ack, buffer, sizeof(int));
                if( ack == i )
                    break; // jesli dobre potwierdzenie
            }
        }

        tmpinfo.progress = (i*1.0 / datagramNumber) * 100;
        sem_P(semId);
        memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
        sem_V(semId);

    }
    //uzupelnienie logow
    tmpinfo.status = 4;
    sem_P(semId);
    memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
    sem_V(semId);

    writeToLogFile(&tmpinfo, offset);

    close(sockfd);
    free(buffer);
    return 1;

}

int download(char *fileName, int sockfd, int semId, char *shmptr, struct sockaddr_in * from)
{
    struct sockaddr_in client_addr;
    memcpy(&client_addr, from, sizeof(*from));
    // określa wielkość struktury sockaddr
    socklen_t len = sizeof(client_addr);
    char *mesg = malloc(SIZEDATAGRAM);
    char ack;
    // przechowuje nr datagramu
    int whichOne;
    // zlicza ilosc datagramow ktore nadeszly
    int counter;
    int packetAck;
    int datagramNumber, lastDatagramSize, currentSizeDatagram, index;
    Info tmpinfo;
    FILE *fd, *test;
    int offset;

    //nazwa pliku
    char* filepath = malloc(strlen(downloadDirectoryPath) + strlen(fileName)+1);
    strcpy(filepath, downloadDirectoryPath);
    strcat(filepath, fileName);

    // ustawiamy timeout
    struct timeval sendtimeout;
    sendtimeout.tv_sec = 3;
    sendtimeout.tv_usec = 0;

  
    //ustawienie funkcji socketa - timeout'u
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&sendtimeout,sizeof(sendtimeout));

    // odebranie rozmiaru pliku, liczby datagramow i rozmiaru ostatniego datagramu
   ack = TRANSFER_ACK;
   int i;
   for(i = 0 ; i < 5 ; ++i)
   {
        // wyslanie potw transferu
        sendto(sockfd, &ack, sizeof(char), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));

        // oczekiwanie na zerowy pakiet
        if(recvfrom(sockfd, mesg, sizeof(int)*3, 0, (struct sockaddr *)&client_addr, &len) < 0 )
        {
            if(i == 4)
            {

                printf(ANSI_COLOR_RED "3 proby odebrania pakietu nieudane. Uplynal limit czasu na potwierdzenie transferu.\n" ANSI_COLOR_RESET);
                close(sockfd);
                free(mesg);
                return -1;
            }
        }
        else
        {
              memcpy(&whichOne, mesg, sizeof(int)); // kopiuje numer datagramu
              if(whichOne == 0 ) break;
        }
    }


    memcpy(&lastDatagramSize, mesg + sizeof(int), sizeof(int)); // kopiuje rozmiar pliku
    memcpy(&datagramNumber, mesg + 2*sizeof(int), sizeof(int)); // kopiuje ilosc datagramow

    if(datagramNumber > 0 && whichOne == 0)
    {
        //uzupelnienie logow
        tmpinfo.status = 1;
        strcpy(tmpinfo.filename, fileName);
        tmpinfo.progress = 0;

        offset = writeToLogFile(&tmpinfo, -1);

        // to musi byc tutaj + w sekcji krytycznej - musze zajac index i go w sekcji zapisac w sekcji
        sem_P(semId);
        index = findIndex(shmptr);
        memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
        sem_V(semId);

        fd = fopen(filepath, "w");

        currentSizeDatagram = SIZEDATAGRAM;

        for(counter = 1; counter <= datagramNumber;)
        {
            // co 10 przebieg sprawdzamy czy plik nie zostal usuniety
            if(counter % 10 == 0)
            {
                test = fopen(filepath, "r");
                if(test == NULL )
                {
                    tmpinfo.status = 0;
                    sem_P(semId);
                    memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
                    sem_V(semId);
                    fclose(test);
                    free(mesg);
                    return -2;
                }
                fclose(test);
            }
            if(datagramNumber == counter)
            {
                currentSizeDatagram = lastDatagramSize + sizeof(int);
            }
            int i;

            //powtarzanie potwierdzen
            for(i = 0; i < 5 ; ++i)
            {

                packetAck = counter -1;
                // wyslanie potwierdzenia poprzedniego pakietu
                sendto(sockfd, &packetAck, sizeof(int), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));

                // oczekiwanie na aktualny pakiet
                if(recvfrom(sockfd, mesg, currentSizeDatagram, 0, (struct sockaddr *)&client_addr, &len) < 0)
                {

                    if(i == 4)
                    {

                        printf(ANSI_COLOR_RED "Blad pobierania pliku\nPrzerwane polaczenie\n" ANSI_COLOR_RESET);
                        //uzupelnienie logow
                        tmpinfo.status = 0;
                        sem_P(semId);
                        memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
                        sem_V(semId);
                        remove(filepath);
                        free(mesg);
                        return -2;
                    }
                    else continue;
                }
                int test;
                memcpy(&test, mesg, sizeof(int));
            // kopiuje numer datagramu
                memcpy(&whichOne, mesg, sizeof(int));

            //odebralismy zly pakiet - pomijamy
                if(whichOne == counter)
                {
                    counter++;
                        // zapis danych z datagramu do pakietu
                    fwrite(mesg+sizeof(int), 1, currentSizeDatagram-sizeof(int), fd);
                    break;
                }

            }//for nasz

            //informacja o postepie pobierania
            tmpinfo.progress = ((counter-1.0) / datagramNumber) * 100;
            sem_P(semId);
            memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
            sem_V(semId);
        }
    }

    //odebralismy ostatni pakiet, wysylamy tylko potwierdzenie
    sendto(sockfd, &datagramNumber, sizeof(int), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
    sendto(sockfd, &datagramNumber, sizeof(int), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));

    //uzupelnienie logow
    tmpinfo.status = 2;
    sem_P(semId);
    memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
    sem_V(semId);
    writeToLogFile(&tmpinfo, offset);

    fclose(fd);
    // zwolnienie zaalokowanej pamieci
    free(mesg);
    // prawidłowe zakonczenie
    return 0;


}

FILE* findResource(char *fileName, char * path)
{
    char* filePath = malloc(strlen(path) + strlen(fileName) +1);
    FILE *fp;
    strcpy(filePath, path);
    strcat(filePath, fileName);
    if ((fp = fopen(filePath, "rb")) == NULL)
    {
        return NULL;
    }
    free(filePath);
    return fp;
}

void listener()
{

    //bufor na dane z gniazda nasluchujacego
    char * listenBuffer = malloc(SIZEDATAGRAM);

    //utworzenie gniazda nasluchujacego na pytania o zasoby
    int listenSock;
    listenSock = socket(AF_INET, SOCK_DGRAM, 0); // utworzenie gniazda

    //przypisanie adresu i portu do gniazda
    struct sockaddr_in listenAddr;
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    listenAddr.sin_port = htons(LISTENPORT);

    if (bind(listenSock, (struct sockaddr *)&listenAddr, sizeof(struct sockaddr)) == -1)
    {
        printf("blad funkcji bind listenSock \n");
        return;
    }

    //struktura do zapisania adresu i portu, skad przyszlo zapytanie o plik
    struct sockaddr_in sourceAddr;
    socklen_t sourceAddrLenght = sizeof(sourceAddr);

    char  address[NI_MAXHOST];
    while (1)
    {
        //odbiera zapytanie o dany plik, zapisuje do struktury adres i port wezla, ktory pytal
        // printf("przyszlo zapytanie od %s\n", address);
        if (recvfrom(listenSock, listenBuffer, SIZEDATAGRAM, 0,(struct sockaddr*) &sourceAddr, &sourceAddrLenght) < 0)
        {
            printf("blad recvfrom listen");
            return ;
        }

        getnameinfo((struct sockaddr*) &sourceAddr, sizeof(struct sockaddr_in),address, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
       // printf("przyszlo zapytanie od %s\n", address);

        //czy dostalismy zapytanie od siebie, czy z zewnatrz
        if(checkSource(sourceAddr) == 1) continue;

        //tworzymy proces do obslugi wyszukiwania zasobu i ewentulnego transferu zasobu
        if (fork() == 0)
        {
            //nazwa zasobu
            char * fileName = malloc(SIZEDATAGRAM);
            memcpy(&fileName, &listenBuffer, strlen(listenBuffer)+1);
            //printf(" dostalem %s \n", fileName);
     
            close(listenSock);

            //bufor na odpowiedz czy jest zasob czy nie
            char * respBuffer = malloc(SIZEDATAGRAM);

            //utworzenie gniazda obslugujacego wyszukanie i ewentualny transfer zasobu
            int serviceSock;
            serviceSock = socket(AF_INET, SOCK_DGRAM, 0); // utworzenie gniazda

            //przypisanie adresu i portu do gniazda
            struct sockaddr_in serviceAddr;
            serviceAddr.sin_family = AF_INET;
            serviceAddr.sin_addr.s_addr = htonl(INADDR_ANY);
            serviceAddr.sin_port = htons(0);

            if (bind(serviceSock, (struct sockaddr *) &serviceAddr, sizeof(serviceAddr)) != 0)
            {
                printf("blad funkcji bind serviceSock \n");
                free(respBuffer);
                exit(0);
            }

            //szukamy zasobu
            FILE* file = findResource(fileName, sharedDirectoryPath);

            //jest
            if (file != NULL)
            {
                respBuffer[0] = RESOURCE_FOUND;

                 //wyslanie odpowiedzi, ze zasob jest
                //WYSYLAMY Z NOWEGO GNIAZDA - WEZEL PYTAJACY DOSTANIE NASZ NOWY ADRES I NOWY PORT

                sendto(serviceSock, respBuffer, SIZEDATAGRAM, 0, (const struct sockaddr*) &sourceAddr, sizeof(sourceAddr));

                getnameinfo((struct sockaddr*) &sourceAddr, sizeof(struct sockaddr_in),address, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
               // printf("wyslalem potw do %s \n", address);

                struct timeval sendtimeout; // ustawiamy timeout
                sendtimeout.tv_sec = 10;
                sendtimeout.tv_usec = 0;

                // ustawienie opcji - timeout
                setsockopt(serviceSock, SOL_SOCKET, SO_RCVTIMEO,&sendtimeout,sizeof(sendtimeout));

                //czekanie na potwierdzenie transferu zasobu
                if (recvfrom(serviceSock, respBuffer, SIZEDATAGRAM, 0,NULL, NULL) < 0)
                {
                  //  printf("timeout czekania na potwierdzenie transferu\n");
                    free(respBuffer);
                    exit(0);
                }

                if(respBuffer[0]==TRANSFER_ACK)
                {
                    getnameinfo((struct sockaddr*) &sourceAddr, sizeof(struct sockaddr_in),address, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                   // printf("dostalem potw od %s \n", address);
                

                //wysylanie pliku
                char* shmptr1;
                shmptr1 = (char*)attach_segment( shmID );
                sendfile(file, fileName, (struct sockaddr *) &sourceAddr, semID, shmptr1);
                fclose(file);
		}
            }
            else
                //nie ma
            {
              

                //printf("pliku nie ma, nic nie wysylam do %s\n",address);

            }

            close(serviceSock);
            free(respBuffer);

            //koniec potomka
            exit(0);
        }

        //rodzic
        else
        {
            // przechodzimy do nasluchiwania na kolejne zapytanie o zasob
           // printf("listen nasluchuje nastepnego zapytania\n");

        }
    }
}

void findNetwork( char* filename)
{
    if(findResource(filename, downloadDirectoryPath)!=NULL)
    {
        printf("Plik o takiej nazwie istnieje, koncze findNetwork.\n");
        return;

    }

    int sockfd; // deskryptor
    size_t sizeresp;
    char* resp = NULL;
    socklen_t * sockSize; // rozmiar struktury okreslajacej posiadacza zasobu
    char *buff = malloc(SIZEDATAGRAM); // odpowiedz
    char host[NI_MAXHOST];  //adres zrodlowy
    struct sockaddr_in bc_addr; // adres broadcastowy
    struct sockaddr resAdress; // adres posiadacza zasobu
    static int so_broadcast = 1;
    int no_fork = 0;
    char * shmptr1;         //wskaznik na wspolna pamiec

    struct timeval sendtimeout; // ustawiamy timeout
    sendtimeout.tv_sec = 10;
    sendtimeout.tv_usec = 0;

    // inicjalizacja socketu
    sockfd = socket(AF_INET,SOCK_DGRAM,0);
    if(sockfd == -1)
    {
        printf("Socket creating error. \n");
         return;
    }

    // ustawienie opcji - broadcastu
    if(setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &so_broadcast, sizeof(so_broadcast)) )
    {
        printf("Socket option error. \n");
        return;
    }

    struct sockaddr_in temp; // adres broadcastowy
    // ustawienie adresu
    temp.sin_family = AF_INET;
    temp.sin_addr.s_addr = htonl(INADDR_ANY);

    temp.sin_port = htons(0);

   if (bind(sockfd, (struct sockaddr *)&temp, sizeof(temp)) == -1)
    {
        printf("blad funkcji bind listenSock find\n");
        return;
    }

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&sendtimeout,sizeof(sendtimeout));

    struct sockaddr_in tmp2;
    socklen_t len = sizeof(tmp2);
    // ustawienie adresu
    bc_addr.sin_family = AF_INET;
    bc_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    bc_addr.sin_port = htons(LISTENPORT);

        // wyslanie zapytania z nazwa pliku
        sendto(sockfd, filename, strlen(filename)+1,0, (struct sockaddr *)&bc_addr, sizeof(bc_addr) );

        // ustawienie opcji - timeout

        if(recvfrom(sockfd, buff, SIZEDATAGRAM, 0, (struct sockaddr*)&tmp2, &len ) < 0) // jesli po 3 sek nie ma odpowiedzi
        {

            printf("Nie znaleziono podanego pliku w sieci. \n");
            close(sockfd);

        }
        else
        {

        getnameinfo((struct sockaddr*) &tmp2,sizeof(struct sockaddr_in),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        printf("Address : <%s> \n", host);
        if(buff[0] == RESOURCE_FOUND)
        {
            printf("Znaleziono plik. Wpisz 'T', aby pobrac lub cokolwiek innego, aby anulowac: \n");
            getline(&resp, &sizeresp, stdin );
            resp[strlen(resp)-1] ='\0';
            if(resp[0] == TRANSFER_ACK)
            {
                if(fork()== 0)
                {
                    shmptr1 = (char*)attach_segment( shmID );
                    download(filename, sockfd, semID,shmptr1, &tmp2);
                    close(sockfd);
                    free(buff);
                    free(resp);
                    //koniec procesu
                    exit(0);
                }
        }

    }
}
    close(sockfd);
    free(buff);
    free(resp);
    return;
}

// sprawdzenie czy pakiet został wysłany z jednego z naszych interfejsów
int checkSource(struct sockaddr_in from)
{
    int result;
    char interfaceAddress[NI_MAXHOST];
    char sourceAddress[NI_MAXHOST];
    struct ifaddrs *ifaddr, *ifa;

    getnameinfo((struct sockaddr*) &from,sizeof(struct sockaddr_in),sourceAddress, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;

        result=getnameinfo(ifa->ifa_addr,sizeof(struct sockaddr_in),interfaceAddress, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

        if(ifa->ifa_addr->sa_family==AF_INET)
        {
            if (result != 0)
            {
                printf("getnameinfo() failed: %s\n", gai_strerror(result));
                exit(EXIT_FAILURE);
            }
            if(strcmp(sourceAddress,interfaceAddress) == 0)
            {
                freeifaddrs(ifaddr);
                return 1;
            }
        }
    }
    freeifaddrs(ifaddr);
    return 0;
}

void closeProgram()
{
    sem_remove(semID);
	sem_remove(fileSemID);
	shm_remove(shmID);
    exit(0);
}

void info(char* shmptr)
{
    Info* tmp = (Info*) shmptr;

    int i;
    printf("******************************************\n");
    printf("Sciagane pliki:\n");
    for(i = 0; i<MAXINFO; i++)
    {
        if(tmp[i].status == 1 || tmp[i].status == 2)
        {
            printf("%s      %d%%\n", tmp[i].filename, tmp[i].progress);
        }
    }
    printf("\n\nWysyłane pliki:\n");
    for(i = 0; i<MAXINFO; i++)
    {
        if(tmp[i].status == 3 || tmp[i].status == 4)
        {
            printf("%s      %d%%\n", tmp[i].filename, tmp[i].progress);
        }
    }
    printf("******************************************\n");
}

// wyszukuje wolny indeks w obszarze pamieci dzielonej
int findIndex(char *shmptr)
{
    int i, j, index = -1;
    Info* inf = (Info*) shmptr;

    for(i=0; i<MAXINFO; i++)
    {
        if(inf[i].status == 0)
        {
            index = i;
            break;
        }
    }
    if(index == -1)
    {
        for(i=0; i<MAXINFO; i++)
        {
            if(inf[i].status == 2 || inf[i].status == 4)
            {
                index = i;
                break;
            }
        }
    }
    if(index == -1)
        index == 0;

    return index;
}

int checkTransfers(char* shmptr, int which)
{
    Info* tmp = (Info*) shmptr;
    int i;
    if(which == 1)
        for(i = 0; i<MAXINFO; i++)
        {
            if(tmp[i].status == 1 || tmp[i].status == 3)
                return 1;
        }
    else
        for(i = 0; i<MAXINFO; i++)
        {
            if(tmp[i].status == 3)
                return 1;
        }

    return 0;
}

void termination_handler(int signum)
{
    if(connected == 1)
    {
        kill(- listenerPID, SIGKILL);
        wait();
    }
    closeProgram();
}
