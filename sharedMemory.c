#include <stdio.h>
#include <sys/shm.h>

#include "sharedMemory.h"

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
