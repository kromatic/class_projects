/*
** cs154-2015 Project 5 ("p5ims") Instant Message Server
** basic.c: for functions that aren't called per-client-thread
*/

#include "ims.h"

void *udbaseRewrite(void *vargp) {
  int timer = *((int*)vargp);
  pthread_detach(pthread_self());

  while(!quitting) {
    sleep(timer);
    sem_wait(udbaseMutexp);
    udbaseWrite(NULL);
    sem_post(udbaseMutexp);
  }

  return NULL;
}

/* call serverStop() upon getting "q" or "quit" on stdin */
void *readQuitFromStdin(void *vargp) {
  pthread_detach(pthread_self());
  char *line=NULL;
  size_t lsize=0;
  while (1) {
    ssize_t glret;
    if (verbose) {
      printf("Type \"q\" or \"quit\" to cleanly quit the server\n");
    }
    glret = getline(&line, &lsize, stdin);
    /* Ctrl-D or EOF will also break out of this loop */
    if (glret <= 0 || !strcmp("quit\n", line) || !strcmp("q\n", line)) {
      /* tell things to quit gracefully */
      free(line);
      quitting = 1;
      /* anything else to do here? */
      break;
    }

    /* ... else !strcmp("foo\n", line) to see if user typed "foo" and then
       return. You can use this to add your own additional commands here, like
       for querying globals or other aspects of server's internal state */

  }
  return NULL;
}

int serverStart(impEm *iem) {
  static const char me[]="serverStart";

  if (verbose > 1) {
    printf("%s: hi\n", me);
  }
  if (udbaseRead(iem)) {
    impEmAdd(iem, "%s: failed to read database file \"%s\"",
             me, udbaseFilename);
    return 1;
  }
  /* immediately try writing database, so that any errors here can be
     reported as a failure of server start-up. Whether and how you do
     error handling for subsequent calls to udbaseWrite() is up to you */
  if (udbaseWrite(iem)) {
    impEmAdd(iem, "%s: failed to write database file \"%s\"",
             me, udbaseFilename);
    return 1;
  }

  /* YOUR CODE HERE:
     -- create listening file descriptor for listenPort and listen() on it
     See http://beej.us/guide/bgnet/output/html/multipage/syscalls.html
     and May 18 2015 class slides
     -- start a thread to periodically save database
     -- figure out whether looking for "quit" on stdin should be done
     in a thread that is started here, or in main.c
  */
  pthread_t tid;
  pthread_create(&tid, NULL, udbaseRewrite, &saveInterval);
  pthread_create(&tid, NULL, readQuitFromStdin, NULL);

  /* adapted from Beej's guide and server.c */
  int status;
  struct addrinfo hints, *servinfo, *p;

  memset(&hints, 0 , sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  char portstr[10];
  int yes = 1;
  sprintf(portstr, "%u", listenPort);

  if((status = getaddrinfo(NULL, portstr, &hints, &servinfo)) != 0) {
    impEmAdd(iem, "%s: getaddrinfo failed: %s", me, gai_strerror(status));
    return 1;
  }

  for(p = servinfo; p != NULL; p = p->ai_next) {

    if((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      //impEmAdd(iem, "%s: failed to create listening socket", me);
      continue;
    }

    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      impEmAdd(iem, "%s: setsockopt failure", me);
      return 1;
    }

    if(bind(listenfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(listenfd);
      //impEmAdd(iem, "%s: bind failure", me);
      continue;
    }

    break;
  }

  if(!p) {
    impEmAdd(iem, "%s: bind failure", me);
  }

  if(listen(listenfd, BACKLOG) == -1) {
    impEmAdd(iem, "%s: listen failure", me);
    return 1;
  }

  if(verbose) {
    printf("%s: server started on part %u from %s\n",
           me, listenPort, udbaseFilename);
  }
  if(verbose > 1) {
    printf("%s: bye\n", me);
  }
  return 0;
}

void serverStop(void) {
  static const char me[]="serverStop";

  if (verbose > 1) {
    printf("%s: hi\n", me);
  }


  /* ... YOUR CODE HERE. What needs to be done to clean up
     resources created during server execution? */
  sem_wait(udbaseMutexp);
  udbaseWrite(NULL); // save the database
  sem_post(udbaseMutexp);

  // clean up database
  dbUser_t *ucur = db.ulist;
  dbUser_t *tmp;
  while(ucur) {
    free(ucur->name);
    dbFriend_t *fcur = ucur->flist;
    dbFriend_t *temp;
    while(fcur) {
      temp = fcur;
      fcur = fcur->next;
      free(temp);
    }
    tmp = ucur;
    ucur = ucur->next;
    free(tmp);
  }

  if (verbose > 1) {
    printf("%s: bye\n", me);
  }
  return;
}

