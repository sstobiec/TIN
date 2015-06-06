#ifndef SEMAPHORE_H
#define SEMAPHORE_H

int sem_create(int semKey);

int sem_init(int semId);

int sem_V(int semId);

int sem_P(int semId);

int sem_remove(int semId);

#endif

