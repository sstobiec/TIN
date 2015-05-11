#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>

#define SIZEDATAGRAM 512
#define LISTENPORT 5558
#define SENDPORT 5559

#define RESOURCE_FOUND 255
#define RESOURCE_NOT_FOUND 0

const char* directoryPath;
int sendfile(FILE* file, struct sockaddr *to);
void listener();

struct sockaddr findNetwork(const char* filename);

int main(int argc, char* argv[])
{

	char *command = NULL; // komenda podawana przez uzytkownika
	char *ptr; // wsk na nazwe pliku
	int sizecommand; // rozmiar komendy ( nieuzywany - tylko do wywolania getline)
	pid_t listenerPID = 0;
	directoryPath = argv[1]; // katalog wybrany przez uzytkownika
	printf("Katalog glowny ustawiony na: %s \n", directoryPath);

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
	        kill(listenerPID, SIGKILL);
	        printf("disconnected \n");
	    }
	    else if(strcmp(command, "info") == 0)
	    {
	        printf("info \n");
	    }
	    else if(strcmp(command,"exit") == 0)
	    {
	        kill(listenerPID, SIGKILL);
	        exit(0);
	    }
	    else
	    {
            printf("Nie ma takiej komendy. \n");
	    }
    command = NULL;
	//printf("%s \n cmd \n", command);
	}

	FILE *file = fopen("/home/piotrek/Pulpit/doc/dokumentacjaV1.pdf", "rb");
	if(!file)
	{
		printf("blad otwarcia pliku"); return 1;
	}

	sendfile(file,NULL);
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
        for(int k = 0; k < 3; ++k)
        {
            sendto(sockfd, buffer, SIZEDATAGRAM, 0, to, sizeof(&to)); // wyslanie

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

FILE* findResource(char *fileName)
{
	char* filePath = malloc(strlen(directoryPath) + strlen(fileName) +1);
	FILE *fp;
	strcpy(filePath, directoryPath);
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
	listenAddr.sin_port = htons(LISTENPORT);

	if (bind(listenSock, (struct sockaddr *)&listenAddr, sizeof(struct sockaddr)) == -1)
	{
		printf("blad funkcji bind listenSock \n");
		return 0;
	}

	//struktura do zapisania adresu i portu, skad przyszlo zapytanie o plik
	struct sockaddr sourceAddr;
	socklen_t sourceAddrLenght, sendAddrLength;

	while (1)
	{
		//odbiera zapytanie o dany plik, zapisuje do struktury adres i port wezla, ktory pytal
		printf("czekam na polaczenie \n");
		if (recvfrom(listenSock, listenBuffer, SIZEDATAGRAM, 0,(struct sockaddr*) &sourceAddr, &sourceAddrLenght) < 0)
		{
			printf("blad recvfrom listen");
			return 0;
		}
printf("czekam na polaczenie 2\n");
		//tworzymy proces do obslugi wyszukiwania zasobu i ewentulnego transferu zasobu
		if (fork() == 0)
		{
			//kopiowanie adresu do obslugi z rodzica do dziecka...
			//POTRZEBNA SYNCHRONIZACJA - CO JESLI PRZYJDZIE KOLEJNE ZAPYTANIE DO RODZICA, JESLI DZIECKO NIE ZDAZY SKOPIOWAC TEJ STRUKTURY Z ADRESEM?
			//							- OBSLUZYMY NIE TEGO KLIENTA KTOREGO TRZEBA :D
			struct sockaddr_in sendAddr;
			socklen_t sendAddrLenght;
			sendAddrLength = sourceAddrLenght;
			memcpy(&sendAddr, &sourceAddr, sendAddrLength);

			//printf("%s", sourceAddr.)

			//kopiowanie nazwy pliku z rodzica do dziecka
			//POTRZEBNA SYNCHRONIZACJA - JAK WYZEJ
			char * fileName = malloc(SIZEDATAGRAM);
			memcpy(&fileName, &listenBuffer, strlen(listenBuffer));
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
				exit(0);
			}

			//szukamy zasobu
			FILE* file = findResource(fileName);

			//jest
			if (file != NULL)
			{
				respBuffer[0] = RESOURCE_FOUND;

				//if(respBuffer[0] == RESOURCE_FOUND) printf("tak \n");
				//wyslanie odpowiedzi, ze zasob jest
				//WYSYLAMY Z NOWEGO GNIAZDA - WEZEL PYTAJACY DOSTANIE NASZ NOWY ADRES I NOWY PORT

				sleep(3);
				sendto(serviceSock, fileName, SIZEDATAGRAM, 0, (struct sockaddr *)&sendAddr, sizeof(sendAddr));

                printf("wyslalem potw \n");
				//czekanie na potwierdzenie transferu zasobu
				//WEZEL PYTAJACY UTWORZYL NOWE GNIAZDO
				//ODBIERAMY ODPOWIEDZ Z NOWEGO ADRESU I PORTU - zapisujemy go do sendAddr i sendAddrLength
				if (recvfrom(serviceSock, fileName, SIZEDATAGRAM, 0,(struct sockaddr *) &sendAddr, &sendAddrLenght) < 0)
				{
					printf("blad recvfrom listen");
					exit(0);
				}

				//wysylanie pliku
				//WYSYLAMY POD NOWY ADRES I PORT, KTORY OTRZYMALISMY PRZED CHWILA
				//sendfile(file,(struct sockaddr *) &sendAddr);
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
				sendto(serviceSock, respBuffer, sizeof(char), 0, (struct sockaddr *)&sendAddr, sizeof(sendAddr));
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
struct sockaddr findNetwork(const char* filename)
{
    int sockfd; // deskryptor
    socklen_t * sockSize; // rozmiar struktury okreslajacej posiadacza zasobu
    char *buff = malloc(SIZEDATAGRAM); // odpowiedz
    struct sockaddr_in bc_addr; // adres broadcastowy
    struct sockaddr resAdress; // adres posiadacza zasobu
    static int so_broadcast = 1;

    struct timeval sendtimeout; // ustawiamy timeout
    sendtimeout.tv_sec = 6;
    sendtimeout.tv_usec = 0;

    // inicjalizacja socketu
    sockfd = socket(AF_INET,SOCK_DGRAM,0);
    if(sockfd == -1)
    {
        printf("Socket creating error. \n");
       // return NULL;
    }

    // ustawienie opcji - broadcastu
   // if(setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &so_broadcast, sizeof(so_broadcast)) )
    //{
      //  printf("Socket option error. \n");
       // return NULL;
    //}

    // ustawienie adresu
    bc_addr.sin_family = AF_INET;
    //inet_aton("192.168.1.5", &(bc_addr.sin_addr));
    bc_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //bc_addr.sin_addr.s_addr = inet_addr("192.168.1.5");
    bc_addr.sin_port = htons(LISTENPORT);

    // wyslanie zapytania z nazwa pliku
    sendto(sockfd, filename, strlen(filename)+1,0, (struct sockaddr *)&bc_addr, sizeof(bc_addr) );
    printf("wyslane \n");
    // ustawienie opcji - timeout
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&sendtimeout,sizeof(sendtimeout));



	if (bind(sockfd, (struct sockaddr *)&bc_addr, sizeof(struct sockaddr)) == -1)
	{
		printf("blad funkcji bind listenSock \n");
		//return 0;
	}
    // tutaj chyba trzeba to zrobic inaczej
    while(1)
    {
        if(recvfrom(sockfd, buff, SIZEDATAGRAM, 0, NULL, NULL ) < 0) // jesli po 3 sek nie ma odpowiedzi
        { // jesli po 3 sekundach brak odpowiedzi
            printf("Brak zasobu. \n");
            close(sockfd);
            break;

        }
        else
        { // jesli odpowiedz jest pozytywna wychodzimy z petli
            // ale jak bedzie duzo hostow i caly czas negatywna to chyba bedzie slabo
           printf("cos dostalem \n");
            if(buff[0] == RESOURCE_FOUND)
            {
                printf("znaleziono \n");
                break;
            }
        }
    }

    return resAdress;
}

