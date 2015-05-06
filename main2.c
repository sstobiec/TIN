#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>

#define SIZEDATAGRAM 512
#define LISTENPORT 5555

#define RESOURCE_FOUND 255
#define RESOURCE_NOT_FOUND 0

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



void listen()
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

	if (bind(listenSock, (const struct sockaddr *) &listenAddr, sizeof(struct sockaddr)) != 0){
		printf("blad funkcji bind listenSock");
		return 0;
	}

	//struktura do zapisania adresu i portu, skad przyszlo zapytanie o plik
	struct sockaddr_in sourceAddr;
	socklen_t sourceAddrLenght;

	while (true)
	{
		//odbiera zapytanie o dany plik, zapisuje do struktury adres i port wezla, ktory pytal
		if (recvfrom(listenSock, listenBuffer, SIZEDATAGRAM, 0, &sourceAddr, &sourceAddrLenght) < 0)
		{
			printf("blad recvfrom listen");
			return 0;
		}

		//tworzymy proces do obslugi wyszukiwania zasobu i ewentulnego transferu zasobu
		if (fork() == 0)
		{
			//kopiowanie adresu do obslugi z rodzica do dziecka...
			//POTRZEBNA SYNCHRONIZACJA - CO JESLI PRZYJDZIE KOLEJNE ZAPYTANIE DO RODZICA, JESLI DZIECKO NIE ZDAZY SKOPIOWAC TEJ STRUKTURY Z ADRESEM?
			//							- OBSLUZYMY NIE TEGO KLIENTA KTOREGO TRZEBA :D
			struct sockaddr_in sendAddr;
			socklen_t sendAddrLenght;
			sendAddrLength = sourceAddrLength;
			memcpy(&sendAddr, &sourceAddr, sendAddrLength);

			//kopiowanie nazwy pliku z rodzica do dziecka
			//POTRZEBNA SYNCHRONIZACJA - JAK WYZEJ
			char * fileName = malloc(SIZEDATAGRAM);
			memcpy(&fileName, &listenBuffer);

			//chyba tego nie potrzebujemy w tym procesie, mozemy zamknac?
			close(listenSock);

			//bufor na odpowiedz czy jest zasob czy nie
			char * respBuffer = malloc(SIZEDATAGRAM);

			//utworzenie gniazda obslugujacego wyszukanie i ewentualny transfer zasobu
			int serviceSock;
			serviceSock = socket(AF_INET, SOCK_DGRAM, 0); // utworzenie gniazda

			//przypisanie adresu i portu do gniazda
			struct sockaddr_in serviceAddr;
			listenAddr.sin_family = AF_INET;
			listenAddr.sin_addr.s_addr = htonl(INADDR_ANY);
			listenAddr.sin_port = htons(0);

			if (bind(serviceSock, (const struct sockaddr *) &serviceAddr, serviceAddrLength) != 0)
			{
				printf("blad funkcji bind serviceSock");
				exit(0);
			}

			//szukamy zasobu
			FILE* file = findResource(fileName);

			//jest
			if (file != NULL)
			{
				respBuffer[0] = RESOURCE_FOUND;

				//wyslanie odpowiedzi, ze zasob jest
				//WYSYLAMY Z NOWEGO GNIAZDA - WEZEL PYTAJACY DOSTANIE NASZ NOWY ADRES I NOWY PORT
				sendto(serviceSock, respBuffer, sizeof(char), 0, &sendAddr, sizeof(sendAddr));

				//czekanie na potwierdzenie transferu zasobu
				//WEZEL PYTAJACY UTWORZYL NOWE GNIAZDO
				//ODBIERAMY ODPOWIEDZ Z NOWEGO ADRESU I PORTU - zapisujemy go do sendAddr i sendAddrLength
				if (recvfrom(serviceSock, respBuffer, SIZEDATAGRAM, 0, &sendAddr, &sendAddrLenght) < 0)
				{
					printf("blad recvfrom listen");
					exit(0);
				}

				//wysylanie pliku
				//WYSYLAMY POD NOWY ADRES I PORT, KTORY OTRZYMALISMY PRZED CHWILA
				sendfile(file, &sendAddr);
				fclose(file);
			}
			else
				//nie ma
			{
				respBuffer[0] = RESOURCE_NOT_FOUND;

				//wyslanie odpowiedzi, ze zasobu nie ma
				//WYSYLAMY Z NOWEGO GNIAZDA - WEZEL PYTAJACY DOSTANIE NASZ NOWY ADRES I NOWY PORT
				//PYTAJACY NIE UTWORZYL NOWEGO GNIAZDA
				//WYSYLAMY POD STARY ADRES I PORT, Z KTOREGO PRZYSZLO ZAPYTANIE O PLIK
				sendto(serviceSock, respBuffer, sizeof(char), 0, &sendAddr, sizeof(sendAddr));
			}

			close(serviceSock);

			//koniec potomka
			exit(0);
		}

		//rodzic
		else
		{
			//chyba nic nie robimy - przechodzimy do nasluchiwania na kolejne zapytanie o zasob

		}
	}
}
