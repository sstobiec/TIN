#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* UDP serwer */

#define MYPORT 32000
#define SIZEDATAGRAM 516    // 512B danych + 4B numeracja

char fname[300];

int main(int argc, char *argv[])
{
if(fork() == 0)
{
// SERWER
    // deskryptor gniazda
    int sockfd;
    int n;
    struct sockaddr_in server_addr,client_addr;
    // określa wielkość struktury sockaddr
    socklen_t len;
    char mesg[SIZEDATAGRAM];

    // tworzy i otwiera plik specjalny typu gniazdo i zwraca jego deskryptor
    sockfd = socket(AF_INET,SOCK_DGRAM,0);             // AF_INET - protokoły Internetu (TCP/IP), SOCK_DGRAM - gniazdo datagramowe
    if (sockfd == -1)
    {
        perror("opening stream socket");
        exit(1);
    }


    server_addr.sin_family = AF_INET;
    // użyj mojego adresu IP
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(MYPORT);                       // my_addr.sin_port = 0; - wybierz losowo nieużywany port
    //  przypisanie adresu (adresu węzła i numeru portu) do podanego gniazda
    if ( bind(sockfd,(struct sockaddr *)&server_addr,sizeof(server_addr)) == -1)
    {
        perror("binding stream socket");
        exit(1);
    }


   while(1)
   {
      len = sizeof(client_addr);
      n = recvfrom(sockfd,mesg,1000,0,(struct sockaddr *)&client_addr,&len);
      sendto(sockfd,mesg,n,0,(struct sockaddr *)&client_addr,sizeof(client_addr));
      printf("-------------------------------------------------------\n");
      mesg[n] = 0;
      printf("Received the following:\n");
      printf("%s",mesg);
      printf("-------------------------------------------------------\n");
   }
}
else
{
//KLIENT
   int sockfd1,n1;
   struct sockaddr_in servaddr,cliaddr;
   char sendline[SIZEDATAGRAM];
   char recvline[SIZEDATAGRAM];

   sockfd1=socket(AF_INET,SOCK_DGRAM,0);


   servaddr.sin_family = AF_INET;
   servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
   servaddr.sin_port=htons(32000);

   while (fgets(sendline, 10000,stdin) != NULL)
   {
      sendto(sockfd1,sendline,strlen(sendline),0,
      (struct sockaddr *)&servaddr,sizeof(servaddr));
      n1=recvfrom(sockfd1,recvline,10000,0,NULL,NULL);
      recvline[n1]=0;
      fputs(recvline,stdout);
   }
}
}

// sprawdza czy dany plik istnieje
int findResource(char *fileName)
{
    FILE *fp;
    char fname[261] = "/home/sebastian/Pulpit/TIN/";
    strncat(fname, fileName, 250);
    if((fp=fopen(fname, "rb"))==NULL)
    {
        printf ("Nie moge otworzyc takiego pliku!\n");
        return -1;
    }
    fclose (fp);
    return 0;
}


























