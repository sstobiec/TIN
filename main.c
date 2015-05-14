#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ifaddrs.h>
#include <arpa/inet.h>



#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>


#define SIZEDATAGRAM 512
#define LISTENPORT 5556
#define SENDPORT 5559

#define RESOURCE_FOUND 'A'
#define RESOURCE_NOT_FOUND 0

#define TRANSFER_ACK 'T'


#define MYPORT 32005             // do zmiany!!!
#define ROZM_PAM 4096       // jezeli rozmiar jest wiekszy to nie dziala bo jakies ograniczenia sa
#define SHM_ID 1234
#define SEM_KEY 1111
#define MAXINFO 100

// Kolory tekstu na wyjsciu
#define ANSI_COLOR_RED      "\x1b[31;1m"
#define ANSI_COLOR_YELLOW   "\x1b[33;1m"
#define ANSI_COLOR_CYAN     "\x1b[36;1m"
#define ANSI_COLOR_RESET    "\x1b[0m"
#define ANSI_COLOR_GREEN	"\x1b[32m"
#define ANSI_COLOR_BLUE		"\x1b[34m"
#define ANSI_COLOR_MAGENTA	"\x1b[35m"


char* sharedDirectoryPath;
char* downloadDirectoryPath;
int sendfile(FILE* file, char* filename, struct sockaddr *to, int semId, char *shmptr);
void listener();

void findNetwork(const char* filename);

int checkSource(struct sockaddr_in sourceAddress);

void createDirectories(char* shared, char* download);

char fname[300];

//semafory
int sem_create(int semKey);
int sem_init(int semId);
int sem_V(int semId);
int sem_P(int semId);
int sem_remove(int semId);

// pamiec dzielona
int shm_init(int sgmId,int size);
int shm_remove(int shmId);
char* attach_segment( int shmId );
int detach_segment(const void *shmaddr);

int download(char *fileName, int sockfd, int semId, char *shmptr);
void closeProgram(int semId, int shmId);
void info(char* shmptr);

typedef struct
{
    int status;         // status 0 - wolny, 1 - w trakcie sciagania, 2 - sciagniete, 3 - w trakcie wysylania, 4 - wyslane
    int progress;       // jaki jest postep
    char filename[30];
} Info;

Info buf[MAXINFO];      // jak globalnie to nie ma smieci

int semID;
char *shmptr;
int shmID;

int main(int argc, char* argv[])
{

    // inicjalizuje semafor
    semID = sem_create((int) SEM_KEY);
    sem_init(semID);
    // inicjalizuje pamięć dzieloną
    shmID = shm_init((int) SHM_ID,(int) ROZM_PAM);
    shmptr = (char*)attach_segment( shmID );

    // zapisuje struktury typu Info w pamieci dzielonej
    sem_P(semID);
    memcpy(shmptr, buf, MAXINFO*sizeof(Info));
    sem_V(semID);

/*
    if(fork() == 0)
    {
        char *shmptr1 = (char*)attach_segment( shmID );
        int a = initSocket();
        download("/home/sebastian/Pulpit/TIN/ab", a, semID, shmptr1);
    }
*/


	char *command = NULL; // komenda podawana przez uzytkownika
	char *ptr; // wsk na nazwe pliku
	int sizecommand; // rozmiar komendy ( nieuzywany - tylko do wywolania getline)
	pid_t listenerPID = 0;
    struct stat stt ={0};

    if (stat(argv[1], &stt) == -1)
    {
        printf("Bledna sciezka do katalogu. \n");
        exit(0);
    }


	sharedDirectoryPath = malloc(strlen(argv[1]) + strlen("/udostep/") +1);
	strcpy(sharedDirectoryPath, argv[1]);
	strcat(sharedDirectoryPath, "/udostep/");

    downloadDirectoryPath = malloc(strlen(argv[1]) + strlen("/pobrane/") +1);
	strcpy(downloadDirectoryPath, argv[1]);
	strcat(downloadDirectoryPath, "/pobrane/");

	createDirectories(sharedDirectoryPath, downloadDirectoryPath);

	printf("Katalog udostepniony ustawiony na: %s \n", sharedDirectoryPath);
	printf("Katalog pobranych plikow ustawiony na: %s \n", downloadDirectoryPath);

	while(1)
	{
	    printf(">");
	    getline(&command, &sizecommand, stdin); // pobranie komendy
    //free(command);
	//zamiana 'entera' na koncu stringa na '\0'
	if (command [ strlen(command) - 1 ] == '\n' )
            command [ strlen(command) - 1 ] = '\0';
           // printf("%d", sizecommand);
        printf("%d \n", strlen(command));

	    if(strstr(command,"find")!=NULL) // jesli w stringu jest find
	    {
	        printf("find \n");
	        ptr = strchr(command, ' '); // znajdz 'spacje'
		// jak beda 2 spacje albo spacja przed find to juz dzialac nie bedzie - takze nad tym mozna pomyslec
	        printf("plik : %s \n", ptr+1); // przesun sie o jedna pozycje dalej ( tutaj jest nazwa pliku)
            findNetwork(ptr+1);
            free(command);
	    }
	    else if(strcmp(command, "connect") == 0)
	    {
	        if(listenerPID == 0)
	        {
                listenerPID = fork();
                if(listenerPID == 0)
                {
                    printf("dsfs \n");
                    listener();
                }
                //sendfile(fopen("/home/piotrek/Dokumenty/wtep.odt", "rb" ), NULL);
                else printf("connected \n");
	        }
	        else printf("already connected \n");
	    }
	    else if(strcmp(command, "disconnect") == 0)
	    {
	        kill(listenerPID, SIGTERM);
	        printf("disconnected \n");
	    }
	    else if(strcmp(command, "info") == 0)
	    {
	        printf("info \n");
	    }
	    else if(strcmp(command,"exit") == 0)
	    {
	        kill(0, SIGTERM);
	        exit(0);
	    }
	    else
	    {
            printf("Nie ma takiej komendy. \n");
	    }
    command = NULL;
	free(command);
	//printf("%s \n cmd \n", command);
	}
	return 0;
}



int sendfile(FILE* file, char* filename, struct sockaddr *to, int semId, char *shmptr)
{
	int fileSize; // rozmiar pliku
	int datagramNumber; // ilosc datagramow
	int ack; // numer potwierdzanego pakietu
	int sockfd; //socket
	int i = 0; // iterator po datagramach
	char * buffer; // buforach dla danych = malloc(SIZEDATAGRAM); // buforach dla danych
	int index;
    Info tmpinfo;

	if(file == NULL)    return -1;

    struct timeval sendtimeout; // ustawiamy timeout
    sendtimeout.tv_sec = 3;
    sendtimeout.tv_usec = 0;

	fseek(file,0, SEEK_END); // przesuniecie na koniec pliku
	fileSize = ftell(file); // odczyt rozmiaru

	fseek(file,0, SEEK_SET); // powrot na poczatek

	datagramNumber = fileSize / (SIZEDATAGRAM-sizeof(int)); // ilosc datagramow
	if(fileSize % (SIZEDATAGRAM-sizeof(int)) != 0) datagramNumber++; // jesli z reszta to +1

	if(fileSize == 0)   return -1;

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

    // petla - ilosc obiegow - ilosc datagramow
	for(i =1; i <= datagramNumber; ++i)
	{
		// przemieszamy sie po pliku
		fseek(file, (SIZEDATAGRAM-sizeof(int))*(i-1), SEEK_SET);

		// jesli to ostatni datagram
		if(i == datagramNumber)
		{
			fread(buffer+sizeof(int), 1, fileSize % (SIZEDATAGRAM-sizeof(int)), file);
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
        // info tutaj tylko po to zeby pokazac, ze dziala, w wersji finalnej do wywalenia
        info(shmptr);
        //////////////////////////
        sem_V(semId);

	}
    //uzupelnienie logow
    tmpinfo.status = 4;
    sem_P(semId);
    memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
    sem_V(semId);

	close(sockfd);
	free(buffer);
	return 1;

}

int download(char *fileName, int sockfd, int semId, char *shmptr)
{
        struct sockaddr_in client_addr;
        // określa wielkość struktury sockaddr
        socklen_t len = sizeof(client_addr);
        char *mesg = malloc(SIZEDATAGRAM-sizeof(int));
        char ack[4];
        // przechowuje nr datagramu
        int whichOne;
        // zlicza ilosc datagramow ktore nadeszly
        int counter;
        int fileSize, datagramNumber, lastDatagramSize, currentSizeDatagram, index;
        Info tmpinfo;
        FILE *fd;

        //nazwa pliku
        char* filepath = malloc(strlen(downloadDirectoryPath) + strlen(fileName)+1);
        strcpy(filepath, downloadDirectoryPath);
        strcat(filepath, fileName);

        // ustawiamy timeout
        struct timeval sendtimeout;
        sendtimeout.tv_sec = 10;
        sendtimeout.tv_usec = 0;

        // nie moge wczesniej właczyć blokowania bo nie wiemy kiedy nadejdzie pierwsza porcja danych
        //ustawienie funkcji socketa - timeout'u
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&sendtimeout,sizeof(sendtimeout));

        // odebranie rozmiaru pliku, liczby datagramow i rozmiaru ostatniego datagramu
        if(recvfrom(sockfd, mesg, sizeof(int)*3, 0, (struct sockaddr *)&client_addr, &len) == -1)
        {
                printf(ANSI_COLOR_RED "Błąd w recvfrom" ANSI_COLOR_RESET);
                close(sockfd);
                // zwolnienie zaalokowanej pamieci
                free(mesg);
                return -1;
        }

        memcpy(&whichOne, mesg, sizeof(int)); // kopiuje numer datagramu
        memcpy(&fileSize, mesg + sizeof(int), sizeof(int)); // kopiuje rozmiar pliku
        memcpy(&datagramNumber, mesg + 2*sizeof(int), sizeof(int)); // kopiuje ilosc datagramow

        // obliczam koncowke pliku
        lastDatagramSize = fileSize % (SIZEDATAGRAM-sizeof(int));

        // nr pakietu powinien byc rowny 0
        if(whichOne == 0)
            memcpy(ack, mesg, sizeof(int));
        else
        {
                printf(ANSI_COLOR_RED "Blad odebrania pliku :p" ANSI_COLOR_RESET);
                close(sockfd);
                // zwolnienie zaalokowanej pamieci
                free(mesg);
                return -1;
        }

        // wysyla potwierdzenie
        sendto(sockfd, ack, sizeof(int), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));

        // prymitywne zabezpieczenia
        if(datagramNumber > 0 && fileSize > 0 && whichOne == 0)
        {
            //uzupelnienie logow
            tmpinfo.status = 1;
            strcpy(tmpinfo.filename, fileName);
            tmpinfo.progress = 0;

            // to musi byc tutaj + w sekcji krytycznej - musze zajac index i go w sekcji zapisac w sekcji
            sem_P(semId);
            index = findIndex(shmptr);
            memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
            sem_V(semId);

            fd = fopen(filepath, "w");

            currentSizeDatagram = SIZEDATAGRAM;

            for(counter = 1; counter <= datagramNumber;)
            {
                if(datagramNumber == counter)
                    currentSizeDatagram = lastDatagramSize + sizeof(int);

                if(recvfrom(sockfd, mesg, currentSizeDatagram, 0, (struct sockaddr *)&client_addr, &len) == -1)
                {
                    printf(ANSI_COLOR_RED "Blad pobierania pliku\nPrzerwane polaczenie\n" ANSI_COLOR_RESET);
                    //uzupelnienie logow
                    tmpinfo.status = 0;
                    sem_P(semId);
                    memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
                    sem_V(semId);

                    close(sockfd);
                    // usun plik
                    remove(fileName);
                    free(mesg);
                    return -2;
                }

                // kopiuje numer datagramu
                memcpy(&whichOne, mesg, sizeof(int));

                // jezeli nr jest mniejszy badz rowny counter to wysylam w potwierdzeniu => dokumentacja
                if(whichOne <= counter)
                    memcpy(ack, mesg, sizeof(int));
                // to tez powinno byc ale nie chce spowalniac... raczej nie wystapi ten przypadek
                /*else
                {
                    printf(ANSI_COLOR_RED "Błąd pobieranie pliku\nPrzerwane połączenie\n" ANSI_COLOR_RESET);
                    //uzupelnienie logow
                    tmpinfo.status = 0;
                    sem_P(semId);
                    memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
                    sem_V(semId);
                    close(sockfd);
                    // usun plik
                    remove(fileName);
                    // zwolnienie zaalokowanej pamieci
                    free(mesg);
                    return NULL;
                }*/

                if(whichOne == counter)
                {
                    counter++;
                    // zapis danych z datagramu do pakietu
                    fwrite(mesg+sizeof(int), 1, currentSizeDatagram-sizeof(int), fd);
                }


                // wysyla potwierdzenie
                sendto(sockfd, ack, sizeof(int), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));

                tmpinfo.progress = ((counter-1.0) / datagramNumber) * 100;
                sem_P(semId);
                memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
                sem_V(semId);
            }
        }

        //uzupelnienie logow
        tmpinfo.status = 2;
        sem_P(semId);
        memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
        sem_V(semId);


        fclose(fd);
        close(sockfd);
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
	printf("szukam : %s \n", filePath);
	if ((fp = fopen(filePath, "rb")) == NULL)
	{
		printf("Nie moge otworzyc takiego pliku!\n");
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
	//listenAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	listenAddr.sin_port = htons(LISTENPORT);

	if (bind(listenSock, (struct sockaddr *)&listenAddr, sizeof(struct sockaddr)) == -1)
	{
		printf("blad funkcji bind listenSock \n");
		return 0;
	}

	//struktura do zapisania adresu i portu, skad przyszlo zapytanie o plik
	struct sockaddr_in sourceAddr;
	socklen_t sourceAddrLenght;

    char  address[NI_MAXHOST];
	while (1)
	{
		//odbiera zapytanie o dany plik, zapisuje do struktury adres i port wezla, ktory pytal
		printf("czekam na polaczenie \n");
		//inet_ntop(AF_INET, ((const struct sockaddr_in*) &sourceAddr)->sin_addr, address, INET_ADDRSTRLEN);
       // printf("przyszlo zapytanie od %s\n", address);
		if (recvfrom(listenSock, listenBuffer, SIZEDATAGRAM, 0,(struct sockaddr*) &sourceAddr, &sourceAddrLenght) < 0)
		{
			printf("blad recvfrom listen");
			return 0;
		}

		getnameinfo((struct sockaddr*) &sourceAddr, sizeof(struct sockaddr_in),address, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        printf("przyszlo zapytanie od %s\n", address);
		//tworzymy proces do obslugi wyszukiwania zasobu i ewentulnego transferu zasobu
		if (fork() == 0)
		{

            //struct sockaddr_in sendAddr;
			//socklen_t sendAddrLength;
			//sendAddrLength = sourceAddrLenght;
			//memcpy(&sendAddr, &sourceAddr, sendAddrLength);

            //nazwa zasobu
			char * fileName = malloc(SIZEDATAGRAM);
			memcpy(&fileName, &listenBuffer, strlen(listenBuffer)+1);
            printf(" dostalem %s \n", fileName);
			//chyba tego nie potrzebujemy w tym procesie, mozemy zamknac?
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

				//if(respBuffer[0] == RESOURCE_FOUND) printf("tak \n");
				//wyslanie odpowiedzi, ze zasob jest
				//WYSYLAMY Z NOWEGO GNIAZDA - WEZEL PYTAJACY DOSTANIE NASZ NOWY ADRES I NOWY PORT

				sleep(3);
				sendto(serviceSock, respBuffer, SIZEDATAGRAM, 0, (const struct sockaddr*) &sourceAddr, sizeof(sourceAddr));

				getnameinfo((struct sockaddr*) &sourceAddr, sizeof(struct sockaddr_in),address, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                printf("wyslalem potw do %s \n", address);

                struct timeval sendtimeout; // ustawiamy timeout
                sendtimeout.tv_sec = 12;
                sendtimeout.tv_usec = 0;

                // ustawienie opcji - timeout
                setsockopt(serviceSock, SOL_SOCKET, SO_RCVTIMEO,&sendtimeout,sizeof(sendtimeout));

				//czekanie na potwierdzenie transferu zasobu
				if (recvfrom(serviceSock, respBuffer, SIZEDATAGRAM, 0,NULL, NULL) < 0)
				{
					printf("timeout czekania na potwierdzenie transferu\n");
                    free(respBuffer);
					exit(0);
				}

                if(respBuffer[0]==TRANSFER_ACK)
                {
                    getnameinfo((struct sockaddr*) &sourceAddr, sizeof(struct sockaddr_in),address, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                    printf("dostalem potw od %s \n", address);
                }

				//wysylanie pliku
				//sendfile(file,(struct sockaddr *) &sourceAddr);
				char* shmptr1;
				shmptr1 = (char*)attach_segment( shmID );
				sendfile(file, fileName, (struct sockaddr *) &sourceAddr, semID, shmptr1);
                fclose(file);
			}
			else
				//nie ma
			{
			//	respBuffer[0] = RESOURCE_NOT_FOUND;


				//wyslanie odpowiedzi, ze zasobu nie ma
				//WYSYLAMY Z NOWEGO GNIAZDA - WEZEL PYTAJACY DOSTANIE NASZ NOWY ADRES I NOWY PORT
			////	sendto(serviceSock, respBuffer, sizeof(char), 0, (const struct sockaddr*) &sourceAddr, sourceAddrLenght);
			//	getnameinfo((struct sockaddr*) &sourceAddr, sizeof(struct sockaddr_in),address, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                printf("pliku nie ma, nic nie wysylam do %s\n",address);

			}

			close(serviceSock);
			free(respBuffer);

			//koniec potomka
			exit(0);
		}

		//rodzic
		else
		{
			//chyba nic nie robimy - przechodzimy do nasluchiwania na kolejne zapytanie o zasob
			printf("listen nasluchuje nastepnego zapytania\n");

		}
	}
}

void findNetwork(const char* filename)
{
   if(findResource(filename, downloadDirectoryPath)!=NULL)
    {
        printf("Plik o takiej nazwie istnieje, koncze findNetwork\n");
        return;

    }

    int sockfd; // deskryptor
    int sizeresp;
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
    sendtimeout.tv_sec = 4;
    sendtimeout.tv_usec = 0;

    // inicjalizacja socketu
    sockfd = socket(AF_INET,SOCK_DGRAM,0);
    if(sockfd == -1)
    {
        printf("Socket creating error. \n");
       // return NULL;
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

    int i;
    for(i =0; i<3; i++)
    {
        if(i<0)             //jak duzo pakietow z dupy
            i=0;

        // wyslanie zapytania z nazwa pliku
        sendto(sockfd, filename, strlen(filename)+1,0, (struct sockaddr *)&bc_addr, sizeof(bc_addr) );

        printf("wyslane proba %d \n", i);
        // ustawienie opcji - timeout

        // tutaj chyba trzeba to zrobic inaczej
        if(recvfrom(sockfd, buff, SIZEDATAGRAM, 0, (struct sockaddr*)&tmp2, &len ) < 0) // jesli po 3 sek nie ma odpowiedzi
        {
            // jesli po 3 sekundach brak odpowiedzi

            printf("Brak zasobu. \n");
            close(sockfd);
            continue;

        }
        else
            // jesli odpowiedz jest pozytywna wychodzimy z petli
            // ale jak bedzie duzo hostow i caly czas negatywna to chyba bedzie slabo

            printf("cos dostalem: %s \n", buff);
            if(checkSource(tmp2) == 0) printf("OK ");
            else
            {
                printf("OD SIEBIE ");
                continue;

            }
            getnameinfo((struct sockaddr*) &tmp2,sizeof(struct sockaddr_in),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            printf(" Address : <%s>, ", host);
            if(buff[0] == RESOURCE_FOUND)
            {
                printf("JEST \n");
                printf("Znaleziono plik. Aby potwierdzic pobranie wpisz 'T' lub 't': \n");
                getline(&resp, &sizeresp, stdin );
                resp[strlen(resp)-1] ='\0';
                printf("%s \n", resp);
                if(resp[0] == TRANSFER_ACK)
                {
                    int j;

                    for(j =0; j<3; ++j)
                    {

                        sendto(sockfd, resp, sizeof(char),0,(struct sockaddr*)&tmp2, sizeof(tmp2));

                        //nie bylo forka
                        if(no_fork!=1)
                        {
                            if(fork()==0)
                            {
                                shmptr1 = (char*)attach_segment( shmID );
                                if( download(filename, sockfd, semID,shmptr1) == -1)
                                {
                                    no_fork = 1;
                                    continue;

                                }
                                else
                                {
                                    break;
                                }

                            }
                            //proces rodzic
                            else
                            {
                                close(sockfd);
                                free(buff);
                                free(resp);
                                return;

                            }
                        }
                        //byl fork - jestesmy w potomnym
                        else
                        {
                            if( download(filename, sockfd, semID,shmptr1) == -1)
                            {
                                continue;
                            }
                            else
                            {
                                break;
                            }
                        }
                    }//for
                    break;
                }
                else break;
            }
            else --i;           //z dupy pakiet - nie chcemy tracic obiegu


        }

    close(sockfd);
    free(buff);
    free(resp);
    return;
}


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

void createDirectories(char* share, char* download)
{
    struct stat stt ={0};

    if (stat(share, &stt) == -1)
    {
        mkdir(share, 0700);
    }

    if (stat(download, &stt) == -1)
    {
        mkdir(download, 0700);
    }

}



void closeProgram(int semId, int shmId)
{
    sem_remove(semId);
    shm_remove(shmId);
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
            printf("%s      %d%\n", tmp[i].filename, tmp[i].progress);
        }
    }
    printf("\n\nWysyłane pliki:\n");
    for(i = 0; i<MAXINFO; i++)
    {
        if(tmp[i].status == 3 || tmp[i].status == 4)
        {
            printf("%s      %d%\n", tmp[i].filename, tmp[i].progress);
        }
    }
    printf("******************************************\n");
}

// gowniane rozwiazanie, malo wydajne ale liczy :p
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



// inicjalizacja semafora
// jezeli blad to zwraca -1
int sem_create(int semKey)
{
    // tworzony semafor
    int sem_id = semget((key_t) semKey, 1, 0666 | IPC_CREAT);

    return sem_id;
}

// inicjalizacja semafora wartością 1
// jezeli blad to zwraca -1
int sem_init(int semId)
{
    // inicjacja semafora wartością 1
    return semctl(semId, 0, SETVAL, (int)1);
}

// realizuje podnoszenie semafora
int sem_V(int semId)
{
    struct sembuf bufor_sem;

    bufor_sem.sem_num = 0;
    bufor_sem.sem_op = 1;
    bufor_sem.sem_flg = SEM_UNDO;

    if(semop(semId, &bufor_sem, 1) == -1)
    {
        perror("Blad przy podnoszeniu semafora");
        return 0;
    }
    return 1;
}

// realizuje opuszczanie semafora
int sem_P(int semId)
{
    struct sembuf bufor_sem;

    bufor_sem.sem_num = 0;
    bufor_sem.sem_op = -1;
    bufor_sem.sem_flg = SEM_UNDO;

    if(semop(semId, &bufor_sem, 1) == -1)
    {
        perror("Blad przy opuszczaniu semafora");
        return 0;
    }
    return -1;
}

// usuwanie semafora
// jezeli blad to zwraca -1
int sem_remove(int semId)
{
    return semctl(semId, 0 , IPC_RMID, 0);
}

// inicjalizacja pamięci dzielonej
// jezeli blad to zwraca -1
int shm_init(int sgmId, int size)
{
    // tworzony jest segment współdzielonej pamięci o kluczu sgmId,  o rozmiarze ROZM_PAM i prawach do zapisu i odczytu
    int shmid = shmget(sgmId, size, IPC_CREAT|0666);

    return shmid;
}

// usuwa pamiec dzielona
// jezeli blad to zwraca -1
int shm_remove(int shmId)
{
    return shmctl(shmId, IPC_RMID, 0);
}

// utworzony segment włączony zostaje do segmentów danego procesu i zwracany jest adres tego segmentu
char* attach_segment( int shmId )
{
    return (char*) shmat(shmId, NULL, 0);
};

// shmaddr = typ zwracany przez attach_segment;
// jezeli blad to zwraca -1
int detach_segment(const void *shmaddr)
{
    return shmdt(shmaddr);
};

