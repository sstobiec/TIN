#ifndef SHAREDMEMORY_H
#define SHAREDMEMORY_H

int shm_init(int sgmId,int size);

int shm_remove(int shmId);

char* attach_segment( int shmId );

int detach_segment(const void *shmaddr);

#endif
