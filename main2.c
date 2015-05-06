#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>

#define SIZEDATAGRAM 512

const char* directoryPath; // katalog z udostep. plikami
int sendfile(FILE* file, struct sockaddr *to);
int main(int argc, char* argv[])
{
	char *command = NULL; // komenda podawana przez uzytkownika
	char *ptr; // wsk na nazwe pliku 
	int sizecommand; // rozmiar komendy ( nieuzywany - tylko do wywolania getline)
	directoryPath = argv[1]; // katalog wybrany przez uzytkownika
	printf("Katalog glowny ustawiony na: %s \n", directoryPath);
	
	while(1)
	{
	    printf(">");
	    getline(&command, &sizecommand, stdin); // pobranie komendy
	    
	// zamiana 'entera' na koncu stringa na '\0'    
	if (command [ strlen(command) - 1 ] == '\n' )
            command [ strlen(command) - 1 ] = '\0';

	    if(strstr(command,"find")!=NULL) // jesli w stringu jest find
	    {
	        printf("find \n");
	        ptr = strchr(command, ' '); // znajdz 'spacje' 
		// jak beda 2 spacje albo spacja przed find to juz dzialac nie bedzie - takze nad tym mozna pomyslec
	        printf("nazwapliku : %s \n", ptr+1); // przesun sie o jedna pozycje dalej ( tutaj jest nazwa pliku)

	    }
	    else if(strcmp(command, "connect") == 0)
	    {
	        printf("connect \n");
	    }
	    else if(strcmp(command, "disconnect") == 0)
	    {
	        printf("disconnect \n");
	    }
	    else if(strcmp(command, "info") == 0)
	    {
	        printf("info \n");
	    }
	    else if(strcmp(command,"exit") == 0) exit(0);
	    else
	    {
            printf("Nie ma takiej komendy. \n");
	    }

	}
	return 0;
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
	if(fileSize / (SIZEDATAGRAM-4)) datagramNumber++; // jesli z reszta to +1

	memcpy(buffer, &fileSize, sizeof(int)); // ladujemy rozmiar pliku
	memcpy(buffer+4, &datagramNumber, sizeof(int)); // ilosc datagramow
	memcpy(buffer+8, &i, sizeof(int)); // numer ( 0 ) datagramu

	sockfd = socket(AF_INET, SOCK_DGRAM,0); // utworzenie gniazda

    //ustawienie funkcji socketa - timeout'u
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&sendtimeout,sizeof(sendtimeout));

    //w przypadku braku potw, co 3s wysylamy datagram jeszcze raz
	for(int k = 0; k < 3; ++k)
	{
       	sendto(sockfd, buffer, 12, 0, to, sizeof(&to)); // wyslanie

        if(recvfrom(sockfd, buffer, 4, 0, NULL, NULL) < 0) // jesli po 3 sek nie ma odpowiedzi
        {
            if( k == 2) // jesli wyslalismy juz 3 razy
            {
                printf("3 proby nieudane.Zamykam polaczenie... \n");
                return 0;
            }
            printf("Blad polaczenia. Ponawiam transfer pakietu... \n");
            continue;
        }
        else break;
	}

    // odczyt numer pakietu
    memcpy(&ack, buffer, sizeof(int)); // zapis z bufora do zmiennej
    if(ack != 0) // jesli odebralismy inny numer datagramu
    {
        // tu nie wiem co robimy :D - to powinno byc w petli moze ?
    }


    // petla - ilosc obiegow - ilosc datagramow
	for(i =1; i < datagramNumber+1; ++i)
	{
		// przemieszamy sie po pliku
		fseek(file, (SIZEDATAGRAM-4)*(i-1), SEEK_SET);

		// jesli to ostatni datagram
		if(i == datagramNumber)
		{
			fread(buffer, 1, fileSize-datagramNumber*(SIZEDATAGRAM-4),file);
		}
		else
		{
		    fread(buffer, 1, SIZEDATAGRAM-4,file);
		}

		// na koniec pakietu dodajemy jego numer
		memcpy(buffer+SIZEDATAGRAM-4, &i, sizeof(int));

        //w przypadku braku potw, co 3s wysylamy datagram jeszcze raz
        for(int k = 0; k < 3; ++k)
        {
            sendto(sockfd, buffer, SIZEDATAGRAM, 0, to, sizeof(&to)); // wyslanie

            if(recvfrom(sockfd, buffer, 4, 0, NULL, NULL) < 0) // sprawdzenie odp.
            {
                if( k == 2)
                {
                    printf("3 nieudane proby.Zamykam polaczenie... \n");
                    return 0;
                }
                printf("Blad polaczenia. Ponawiam transfer pakietu... \n");
                continue;
            }
            else break;
        }

		memcpy(&ack, buffer, sizeof(int));
		if( ack != i )
		{
			// jw :D
		}

	}
	return 1;

}






