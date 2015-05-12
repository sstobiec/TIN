#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define MYPORT 32005
#define SIZEDATAGRAM 516   // 512B danych
#define ROZM_PAM 4096       // jezeli rozmiar jest wiekszy to nie dziala
#define SHM_ID 1234
#define SEM_KEY 1111
#define MAXINFO 100

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

FILE* download(char *fileName, int sockfd, int semId, char *shmptr);
FILE* findResource(char *fileName);
void closeProgram(int semId, int shmId);
void info(char* shmptr);

typedef struct
{
    int status;         // status 0 - wolny, 1 - w trakcie sciagania, 2 - sciagniete, 3 - w trakcie wysylania, 4 - wyslane
    int progress;       // jaki jest postep
    char filename[30];
} Info;

Info buf[MAXINFO];      // jak globalnie to nie ma smieci


int main(int argc, char *argv[])
{
    // inicjalizuje semafor
    int semID = sem_create((int) SEM_KEY);
    sem_init(semID);
    // inicjalizuje pamięć dzieloną
    int shmID = shm_init((int) SHM_ID,(int) ROZM_PAM);
    char *shmptr = (char*)attach_segment( shmID );

    // zapisuje struktury typu Info w pamieci dzielonej
    sem_P(semID);
    memcpy(shmptr, buf, MAXINFO*sizeof(Info));
    sem_V(semID);


    if(fork() == 0)
    {
        char *shmptr1 = (char*)attach_segment( shmID );
        int a = initSocket();
        download("/home/sebastian/Pulpit/TIN/ab", a, semID, shmptr1);
    }
    else
    {

        struct sockaddr_in servaddr;
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
        servaddr.sin_port=htons(MYPORT);
        FILE* fd = fopen("/home/sebastian/Pulpit/TIN/abc", "ab+");
        sendfile(fd, &servaddr);
    }
      /*
        // jezeli chcecie przetestowac to o

        //KLIENT - ROBOCZY KLIENT - TYLKO DO TESTOW!!!!!!!!!
        int sockfd1;
        struct sockaddr_in servaddr,cliaddr;
        char *sendline = malloc(SIZEDATAGRAM);
        char *recvmsg = malloc(sizeof(int));

        sockfd1 = socket(AF_INET,SOCK_DGRAM,0);

        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
        servaddr.sin_port=htons(MYPORT);
        int whichOne;
        int counter = 1;
        int fileSize = 45624,

        // JEZELI CHCECIE ZMIENIC LICZBE DATAGRAMOW TO TUTUAJ MOZNA!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        datagramNumber = 3;
        int i = 0;
        memcpy(sendline, &fileSize, sizeof(int)); // ladujemy rozmiar pliku
        memcpy(sendline+sizeof(int), &datagramNumber, sizeof(int)); // ilosc datagramow
        memcpy(sendline+2*sizeof(int), &i, sizeof(int)); // numer ( 0 ) datagramu

        sendto(sockfd1,sendline, SIZEDATAGRAM+sizeof(int), 0, (struct sockaddr *)&servaddr,sizeof(servaddr));
        if(recvfrom(sockfd1,recvmsg, sizeof(int),0,NULL,NULL) == -1) printf("Błąd w recvfrom");


        while(fgets(sendline+sizeof(int), 10000,stdin) != NULL)
        {
            memcpy(sendline, &counter, sizeof(int)); // ladujemy rozmiar pliku
            //do
            //{
                // nadawanie tej samej wiadomości dopoki nie ma potwierdzenia :
                sendto(sockfd1,sendline, 512, 0, (struct sockaddr *)&servaddr,sizeof(servaddr));

                if(recvfrom(sockfd1,recvmsg, sizeof(int),0,NULL,NULL) == -1)
                    printf("Błąd w recvfrom");

                memcpy(&whichOne, recvmsg, sizeof(int));
            //} while(whichOne != counter);
            if(whichOne != counter)
            {
            //printf("cos nie tak");
            }

            counter++;
        }
        close(sockfd1);
    }
       */

}

int initSocket()
{
    // deskryptor gniazda
    int sockfd;
    struct sockaddr_in server_addr;

    /*struct timeval sendtimeout; // ustawiamy timeout
    sendtimeout.tv_sec = 3;
    sendtimeout.tv_usec = 0;*/

    // tworzy i otwiera plik specjalny typu gniazdo i zwraca jego deskryptor
    sockfd = socket(AF_INET,SOCK_DGRAM,0);             // AF_INET - protokoły Internetu (TCP/IP), SOCK_DGRAM - gniazdo datagramowe
    if (sockfd == -1)
    {
        perror("opening stream socket");
        exit(1);
    }

    //ustawienie funkcji socketa - timeout'u
	//setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&sendtimeout,sizeof(sendtimeout));

    server_addr.sin_family = AF_INET;
    // użyj mojego adresu IP
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(MYPORT);                       // my_addr.sin_port = 0; - wybierz losowo nieużywany port

    if ( bind(sockfd,(struct sockaddr *)&server_addr,sizeof(server_addr)) == -1)
    {
        perror("binding stream socket");
        exit(1);
    }

    return sockfd;
}

int sendfile(FILE* file, struct sockaddr *to)
{
	int fileSize; // rozmiar pliku
	int datagramNumber; // ilosc datagramow
	int ack; // numer potwierdzanego pakietu
	int sockfd; //socket
	int i = 0; // iterator po datagramach
	char * buffer = malloc(SIZEDATAGRAM); // buforach dla danych

    struct timeval sendtimeout; // ustawiamy timeout
    sendtimeout.tv_sec = 3;
    sendtimeout.tv_usec = 0;


	fseek(file,0, SEEK_END); // przesuniecie na koniec pliku
	fileSize = ftell(file); // odczyt rozmiaru

	fseek(file,0, SEEK_SET); // powrot na poczatek

	datagramNumber = fileSize / (SIZEDATAGRAM-4); // ilosc datagramow
	if(fileSize % (SIZEDATAGRAM-4) != 0) datagramNumber++; // jesli z reszta to +1


	memcpy(buffer, &fileSize, sizeof(int)); // ladujemy rozmiar pliku
	memcpy(buffer+4, &datagramNumber, sizeof(int)); // ilosc datagramow
	memcpy(buffer+8, &i, sizeof(int)); // numer ( 0 ) datagramu

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
                printf("3 proby nieudane.Zamykam polaczenie... \n");
                close(sockfd);
                return 0;
            }
            printf("Blad polaczenia. Ponawiam transfer pakietu... \n");
        }
        else
        {
            // odczyt numer pakietu
            memcpy(&ack, buffer, sizeof(int)); // zapis z bufora do zmiennej
            if(ack == 0) break;// jesli odebralismy dobre potwierdzenie to wychodzimy z petli
        }
	}

    // petla - ilosc obiegow - ilosc datagramow
	for(i =1; i < datagramNumber+1; ++i)
	{
		// przemieszamy sie po pliku
		fseek(file, (SIZEDATAGRAM-4)*(i-1), SEEK_SET);

		// jesli to ostatni datagram
		if(i == datagramNumber)
		{
			fread(buffer, 1, fileSize % (SIZEDATAGRAM-4),file);
		}
		else
		{
		    fread(buffer, 1, SIZEDATAGRAM-4,file);
		}

		// na koniec pakietu dodajemy jego numer
		memcpy(buffer+SIZEDATAGRAM-4, &i, sizeof(int));

        //w przypadku braku potw, co 3s wysylamy datagram jeszcze raz
        for(k = 0; k < 3; ++k)
        {
            sendto(sockfd, buffer, SIZEDATAGRAM, 0, to, sizeof(*to)); // wyslanie

            if(recvfrom(sockfd, buffer, 4, 0, NULL, NULL) < 0) // sprawdzenie odp.
            {
                if( k == 2)
                {
                    printf("3 nieudane proby.Zamykam polaczenie... \n");
                    close(sockfd);
                    return 0;
                }
                printf("Blad polaczenia. Ponawiam transfer pakietu... \n");
            }
            else
            {
                // odczyt numeru potwierdzenia
                memcpy(&ack, buffer, sizeof(int));
                if( ack == i ) break; // jesli dobre potwierdzenie
            }
        }

	}
	close(sockfd);
	return 1;

}


FILE* download(char *fileName, int sockfd, int semId, char *shmptr)
{
        struct sockaddr_in client_addr;
        // określa wielkość struktury sockaddr
        socklen_t len = sizeof(client_addr);
        char *mesg = malloc(SIZEDATAGRAM-sizeof(int));
        char *ack = malloc(sizeof(int));
        // przechowuje nr datagramu
        int whichOne;
        // zlicza ilosc datagramow ktore nadeszly
        int counter;
        int fileSize, datagramNumber, lastDatagramSize, currentSizeDatagram, index;
        Info tmpinfo;
        FILE *fd;

        //uzupelnienie logow
        tmpinfo.status = 1;
        strcpy(tmpinfo.filename, fileName);

        // to musi byc tutaj + w sekcji krytycznej - musze zajac index i go w sekcji zapisac w sekcji
        sem_P(semId);
        index = findIndex(shmptr);
        memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
        sem_V(semId);


        // odebranie rozmiaru pliku, liczby datagramow i rozmiaru ostatniego datagramu
        if(recvfrom(sockfd, mesg, sizeof(int)*3, 0, (struct sockaddr *)&client_addr, &len) == -1)
        {
                printf("Błąd w recvfrom");
                close(sockfd);
                // zwolnienie zaalokowanej pamieci
                free(ack);
                free(mesg);
                return NULL;
        }

        memcpy(&fileSize, mesg, sizeof(int)); // kopiuje rozmiar pliku
        memcpy(&datagramNumber, mesg+sizeof(int), sizeof(int)); // kopiuje ilosc datagramow
        memcpy(&whichOne, mesg+sizeof(int)*2, sizeof(int)); // kopiuje numer datagramu

        // TESTowe ///////////////////////////////////////////////////////
        printf("Dane o pobieranym pliku: %d %d %d \n", fileSize, datagramNumber, whichOne);
        ///////////////////////////////////////////////////////////////////

        // obliczam koncowke pliku
        lastDatagramSize = fileSize % (SIZEDATAGRAM-sizeof(int));

        // nr pakietu powinien byc rowny 0
        if(whichOne == 0)
            memcpy(ack, mesg+sizeof(int)*2, sizeof(int));
        else
            perror("Blad odebrania pliku :p");

        // wysyla potwierdzenie
        sendto(sockfd, ack, sizeof(int), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));

        // prymitywne zabezpieczenia :p
        if(datagramNumber > 0 && fileSize > 0 && whichOne == 0)
        {
            fd = fopen(fileName, "ab+");

            // ustawiamy timeout
            struct timeval sendtimeout;
            sendtimeout.tv_sec = 10;
            sendtimeout.tv_usec = 0;

            // nie moge wczesniej właczyć blokowania bo nie wiemy kiedy nadejdzie pierwsza porcja danych
            //ustawienie funkcji socketa - timeout'u
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&sendtimeout,sizeof(sendtimeout));

            counter = 1;
            currentSizeDatagram = SIZEDATAGRAM;

            while(datagramNumber >= counter)
            {
                if(datagramNumber == counter)
                    currentSizeDatagram = lastDatagramSize + sizeof(int);

                if(recvfrom(sockfd, mesg, currentSizeDatagram, 0, (struct sockaddr *)&client_addr, &len) == -1)
                {
                    printf("Błąd pobieranie pliku\nPrzerwane połączenie\n");
                    //uzupelnienie logow
                    tmpinfo.status = 0;
                    sem_P(semId);
                    memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
                    sem_V(semId);

                    close(sockfd);
                    // usun plik
                    remove(fileName);
                    // zwolnienie zaalokowanej pamieci
                    free(ack);
                    free(mesg);
                    return NULL;
                }

                memcpy(&whichOne, mesg, sizeof(int)); // kopiuje numer datagramu

                // jezeli nr jest mniejszy badz rowny counter to wysylam w potwierdzeniu => dokumentacja
                if(whichOne <= counter)
                    memcpy(ack, mesg, sizeof(int));

                if(whichOne == counter)
                {
                    counter++;
                    // nalezy napisac kopiowanie zawartosci :p
                    fwrite(mesg+sizeof(int), 1, currentSizeDatagram-sizeof(int), fd);
                    printf("%d", whichOne);
                }


                // wysyla potwierdzenie
                sendto(sockfd, ack, sizeof(int), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));

                tmpinfo.progress = ((counter-1.0) / datagramNumber) * 100;
                sem_P(semId);
                memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
                info(shmptr);
                sem_V(semId);


                printf("-------------------------------------------------------\n");
                printf("Odebranow wiadomosc:\n");
                printf("%s",mesg+sizeof(int));
                printf("-------------------------------------------------------\n");
            }
        }

        //uzupelnienie logow
        tmpinfo.status = 2;
        sem_P(semId);
        memcpy(shmptr+index*sizeof(Info), &tmpinfo, sizeof(Info));
        sem_V(semId);


        fclose(fd);
        // TESTowe
        printf("Udalo sie\n");
        ///////////////////////////
        close(sockfd);
        // zwolnienie zaalokowanej pamieci
        free(ack);
        free(mesg);
        // prawidłowe zakonczenie
        return fd;
}

// sprawdza czy dany plik istnieje
FILE* findResource(char *fileName)
{
	FILE *fp;
	char fname[261] = "/home/sebastian/Pulpit/TIN/";
	strncat(fname, fileName, 250);
	if ((fp = fopen(fname, "rb")) == NULL)
	{
		printf("Nie moge otworzyc takiego pliku!\n");
		return NULL;
	}
	return fp;
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














