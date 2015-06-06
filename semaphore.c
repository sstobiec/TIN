#include <sys/sem.h>

#include "semaphore.h"

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
