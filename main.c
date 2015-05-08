#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* UDP serwer */

#define MYPORT 32005
#define SIZEDATAGRAM 512   // 512B danych

char fname[300];

FILE* receiveFile(char *fileName, int sockfd);
FILE* findResource(char *fileName);
void closeProgram();

int main(int argc, char *argv[])
{
    if(fork() == 0)
    {
        int a = initSocket();
        receiveFile("/home/sebastian/Pulpit/TIN/ab", a);
    }
    else
    {
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

FILE* receiveFile(char *fileName, int sockfd)
{
        struct sockaddr_in client_addr;
        // określa wielkość struktury sockaddr
        socklen_t len = sizeof(client_addr);
        char *mesg = malloc(SIZEDATAGRAM);
        char *ack = malloc(sizeof(int));
        // przechowuje nr datagramu
        int whichOne;
        // zlicza ilosc datagramow ktore nadeszly
        int counter;
        int fileSize, datagramNumber, lastDatagramSize, currentSizeDatagram;
        FILE *fd;

        // odebranie rozmiaru pliku, liczby datagramow i rozmiaru ostatniego datagramu
        if(recvfrom(sockfd, mesg, sizeof(int)*3, 0, (struct sockaddr *)&client_addr, &len) == -1)
                printf("Błąd w recvfrom");

        memcpy(&fileSize, mesg, sizeof(int)); // kopiuje rozmiar pliku
        memcpy(&datagramNumber, mesg+sizeof(int), sizeof(int)); // kopiuje ilosc datagramow
        memcpy(&whichOne, mesg+sizeof(int)*2, sizeof(int)); // kopiuje numer datagramu

        // TESTowe ///////////////////////////////////////////////////////
        printf("%d %d %d \n", fileSize, datagramNumber, whichOne);
        ///////////////////////////////////////////////////////////////////

        // obliczam koncowke pliku
        lastDatagramSize = fileSize % SIZEDATAGRAM;

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
            currentSizeDatagram = SIZEDATAGRAM + sizeof(int);

            while(datagramNumber >= counter)
            {
                if(datagramNumber == counter)
                    currentSizeDatagram = lastDatagramSize + sizeof(int);

                if(recvfrom(sockfd, mesg, currentSizeDatagram, 0, (struct sockaddr *)&client_addr, &len) == -1)
                {
                    printf("Błąd w recvfrom\nPrzerwane połączenie\n");
                    close(sockfd);
                    // usun plik
                    remove(fileName);
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

                printf("-------------------------------------------------------\n");
                printf("Odebranow wiadomosc:\n");
                printf("%s",mesg+sizeof(int));
                printf("-------------------------------------------------------\n");
            }
        }

        fclose(fd);
        // TESTowe
        printf("Udalo sie\n");
        ///////////////////////////
        close(sockfd);
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

void closeProgram()
{
    exit(0);
}



















