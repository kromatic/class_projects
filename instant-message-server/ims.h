/*
** cs154-2015 Project 5 ("p5ims") Instant Message Server
** ims.h: header file
*/

#ifndef IMS_INCLUDED
#define IMS_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h> /* inet_ntop */
#include <sys/types.h> /* getaddrinfo */
#include <sys/socket.h>  /* recv, send, getaddrinfo */
#include <netdb.h> /* getaddrinfo */
#include <unistd.h> /* sleep */
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>    /* for isspace() etc */
#include <semaphore.h>

#include "imp.h"  /* the protocol library */

/* the string that you should include in error messages to stderr
   documenting how starting the server failed */
#define FAIL "SERVER_INIT_FAIL"
#define BACKLOG 10

/* YOUR CODE HERE
   ... declaration of structs or other data types you design ... */

/* structs for in-memory representation of database
   -modified structs from cndb.c
   -general ideas discussed with Sean Pierre */

struct dbUser_t;

typedef struct dbFriend_t {
  struct dbUser_t *friend;
  impFriend_t status;
  struct dbFriend_t *next;
} dbFriend_t;

typedef struct dbUser_t {
  char *name;
  impActive_t active;
  int connfd;
  struct dbUser_t *next;
  struct dbFriend_t *flist;
  // unsigned int fnum;
} dbUser_t;

typedef struct {
  dbUser_t *ulist;
  unsigned unum;
} db_t;

/* For each source file (except main.c), declare here (with "extern") the
symbols (the variables and the functions) that it defines, so that #include
"ims.h" at the top of the source allows every source file to use the symbols
from every other source file */

/* globals.c: global variables */
/* this information is stored in globals because it could be awkward to create
   a container for it to be passed to all functions that need to know it */
extern db_t db;

extern int verbose;      /* how much verbose debugging messages to print
                            during operation */
extern int saveInterval; /* interval, in seconds, with which to save
                            user database */
extern unsigned short listenPort;  /* port to listen on */
extern int listenfd;
extern const char *udbaseFilename; /* filename for user database */
extern int quitting;     /* flag to say (by being non-zero) that its time
          to shut things down and quit cleanly. This is
          set by readQuitFromStdin() */
/* add more globals as you see fit */
extern sem_t *udbaseMutexp;


/* udbase.c: for handling the user database, on disk and in memory */
/* possible place to declare globals for in-memory user database */
/* read user database into memory */
extern dbFriend_t *dbFriendInit(char *name, impFriend_t status);
extern dbUser_t *dbUserInit(char *name);
extern int udbaseRead(impEm *iem);
extern int udbaseWrite(impEm *iem);
extern void *udbaseRewrite(void *vargp);

/* basic.c: for high-level and utility functions */
/* if "quit" is typed on on stdin, call serverStop() */
extern void *readQuitFromStdin(void *vargp);
/* start the server */
extern int serverStart(impEm *iem);
/* stop the server and clean up dynamically allocated resources */
extern void serverStop(void);
extern int processUserStr(char *recv_str, int connfd);
extern void *userThread(void *vargp);



#endif /* IMS_INCLUDED */
