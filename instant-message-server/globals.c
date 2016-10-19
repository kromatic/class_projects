/*
** cs154-2015 Project 5 ("p5ims") Instant Message Server
** misc.c: miscellaneous things
*/

#include "ims.h"

db_t db = {NULL, 0};
int verbose = 1;
int saveInterval = 10;
unsigned short listenPort = 15400;
int listenfd = -1;
const char *udbaseFilename = NULL;
int quitting = 0;
sem_t *udbaseMutexp = NULL;
