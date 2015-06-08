WORKDIR = `pwd`

CC = gcc
CXX = g++
AR = ar
LD = g++
WINDRES = windres

INC = 
CFLAGS = -Wall
RESINC = 
LIBDIR = 
LIB = 
LDFLAGS = 

INC_DEBUG = $(INC)
CFLAGS_DEBUG = $(CFLAGS) -g
RESINC_DEBUG = $(RESINC)
RCFLAGS_DEBUG = $(RCFLAGS)
LIBDIR_DEBUG = $(LIBDIR)
LIB_DEBUG = $(LIB)
LDFLAGS_DEBUG = $(LDFLAGS)
OBJDIR_DEBUG = ./
DEP_DEBUG = 
OUT_DEBUG = ./program

OBJ_DEBUG = $(OBJDIR_DEBUG)/localFunc.o $(OBJDIR_DEBUG)/logFile.o $(OBJDIR_DEBUG)/main.o $(OBJDIR_DEBUG)/semaphore.o $(OBJDIR_DEBUG)/sharedMemory.o

all: debug

clean: clean_debug

before_debug: 
	test -d $(OBJDIR_DEBUG) || mkdir -p $(OBJDIR_DEBUG)

after_debug: 

debug: before_debug out_debug after_debug

out_debug: before_debug $(OBJ_DEBUG) $(DEP_DEBUG)
	$(LD) $(LIBDIR_DEBUG) -o $(OUT_DEBUG) $(OBJ_DEBUG)  $(LDFLAGS_DEBUG) $(LIB_DEBUG)

$(OBJDIR_DEBUG)/localFunc.o: localFunc.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c localFunc.c -o $(OBJDIR_DEBUG)/localFunc.o

$(OBJDIR_DEBUG)/logFile.o: logFile.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c logFile.c -o $(OBJDIR_DEBUG)/logFile.o

$(OBJDIR_DEBUG)/main.o: main.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c main.c -o $(OBJDIR_DEBUG)/main.o

$(OBJDIR_DEBUG)/semaphore.o: semaphore.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c semaphore.c -o $(OBJDIR_DEBUG)/semaphore.o

$(OBJDIR_DEBUG)/sharedMemory.o: sharedMemory.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c sharedMemory.c -o $(OBJDIR_DEBUG)/sharedMemory.o

clean_debug: 
	rm -f $(OBJ_DEBUG) $(OUT_DEBUG)
	rm -rf $(OBJDIR_DEBUG)

.PHONY: before_debug after_debug clean_debug

