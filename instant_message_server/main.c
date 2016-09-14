/*
** cs154 Project 5 (p5ims) Instant Message Server
** Copyright (C)  2014,2015 University of Chicago.
** main.c: starter main() with command-line parsing
**
** Because you are a student in cs154, you received this file through the
** repositories that are maintained for cs154.  This file is not licensed for
** any wider distribution. Specifically, you may not allow this file to be
** copied or downloaded by anyone who is not a current cs154 student. To do so
** would be a violation of copyright.
*/

#include "ims.h"

/* adapted from txtimc.c */
#define WHITESPACE " \t\n\r\v\f"
#define BUFF_SIZE 2048

void usage(const char *me) {
  fprintf(stderr, "\n");
  fprintf(stderr, "usage: %s -d dbname [-p port#] [-i saveInterval] [-v level]\n\n", me);
  fprintf(stderr, "The server will initialize from user database \"dbname\", "
          "listen for connections\n");
  fprintf(stderr, "from clients on port \"port#\" (defaults to %u), and save the user "
          "database every \"saveInterval\"\n", listenPort);
  fprintf(stderr, "seconds (defaults to %d seconds).\n", saveInterval);
  fprintf(stderr, "\nThe -v option allows some level of verbose debugging message to stdout\n");
  fprintf(stderr, "(default to %d); with -v 0 nothing should be printed to stdout.\n", verbose);
  exit(1);
}

int main(int argc, char *argv[]) {
  const char *me;
  int intrv, verb;
  unsigned short port;
  int opt; /* current option being parsed */

  me = argv[0];
  /* enstate defaults */
  intrv = saveInterval;
  verb = verbose;
  port = listenPort;

  while ((opt = getopt(argc, argv, "v:p:d:i:")) != -1)
    switch (opt) {
    case 'd':
      udbaseFilename = optarg;
      break;
    case 'p':
      if (1 != sscanf(optarg, "%hu", &port)) {
        fprintf(stderr, "%s: couldn't parse \"%s\" as port number\n",
                me, optarg);
        usage(me);
      }
      break;
    case 'i':
      if (1 != sscanf(optarg, "%d", &intrv)) {
        fprintf(stderr, "%s: couldn't parse \"%s\" as database save interval\n",
                me, optarg);
        usage(me);
      }
      break;
    case 'v':
      if (1 != sscanf(optarg, "%d", &verb)) {
        fprintf(stderr, "%s: couldn't parse \"%s\" as integer verbose level\n",
                me, optarg);
        usage(me);
      }
      break;
    default:
      usage(me);
      break;
  }
  if (!udbaseFilename) {
    fprintf(stderr, "%s: need \"-d dbname\" option\n", argv[0]);
    usage(me);
  }
  if (intrv <= 0) {
    fprintf(stderr, "%s: need positive save interval (not %d)\n",
            me, intrv);
    usage(me);
  }
  if (verb < 0) {
    fprintf(stderr, "%s: need non-negative verbosity level (not %d)\n",
            me, verb);
    usage(me);
  }
  if (port <= 1024) {
    fprintf(stderr, "%s: need port > 1024 (not %u)\n", me, port);
    usage(me);
  }

  /* command-line options successfully parsed; assign to globals and
     start the server from the database file */
  saveInterval = intrv;
  verbose = verb;
  listenPort = port;
  impEm *iem = impEmNew();
  if (serverStart(iem)) {
    fprintf(stderr, "%s: %s server failed to start:\n", me, FAIL);
    impEmFprint(stderr, iem);
    impEmFree(iem);
    exit(1);
  }
  impEmFree(iem);

  sem_t udbaseMutex;
  udbaseMutexp = &udbaseMutex;
  sem_init(udbaseMutexp, 0, 1);

  /* YOUR CODE HERE.  Probably something related to calling
     accept() on the listening file descriptor. */

  /* adapted from server.c */

  struct sockaddr_in user_addr;
  socklen_t addr_size = sizeof(struct sockaddr_in);
  int *connfdp;
  pthread_t tid;
  while(!quitting) {
    connfdp = malloc(sizeof(int));
    if((*connfdp = accept(listenfd, (struct sockaddr *)&user_addr, &addr_size)) == -1) {
      fprintf(stderr, "%s: accept failure\n", me);
      continue;
    }

    pthread_create(&tid, NULL, userThread, connfdp);
  }


  /* Calling this here from main allows the starting server to respond
     correctly to "quit" on stdin, but you need to decide if this is actually
     the right place to call this function */

  serverStop();
  exit(0);
}

int validUserName(char *name)
{
  int len = strlen(name);
  if(!(1 <= len && len <= IMP_NAME_MAXLEN)) {
    return 0;
  }

  int i;
  for(i = 0; i != len; i++) {
    if(isspace(name[i]) || !isprint(name[i])) {
      return 0;
    }
  }

  return 1;
}

int registerUser(impMsgOp *mop, int connfd)
{
  char *me = "registerUser";
  char *response;

  if(!validUserName(mop->userName)) {
    response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_BAD_COMMAND, IMP_END);
    send(connfd, response, strlen(response), 0);
    fprintf(stderr, "%s: bad user name:\n", me);
    
    free(response);
    return 1;
  }

  dbUser_t *ucur = db.ulist->next;
  dbUser_t *uprev = db.ulist;
  while(ucur && strcmp(mop->userName, ucur->name)) {
    uprev = ucur;
    ucur = ucur->next;    
  }

  if(ucur) {
    if(ucur->connfd == connfd) {
      response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_CLIENT_BOUND, mop->userName, IMP_END);
      send(connfd, response, strlen(response), 0);
      fprintf(stderr, "%s: already bound:\n", me);
      
      free(response);
      return 1;
    } else {
      response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_USER_EXISTS, mop->userName, IMP_END);
      send(connfd, response, strlen(response), 0);
      fprintf(stderr, "%s: user already exists:\n", me);
      
      free(response);
      return 1;
    }
  }

  // valid op - register the user
  sem_wait(udbaseMutexp);
  uprev->next = dbUserInit(mop->userName);
  sem_post(udbaseMutexp);
  response = impStrNew(NULL, IMP_MSG_TYPE_ACK, IMP_OP_REGISTER, mop->userName, IMP_END);
  send(connfd, response, strlen(response), 0);
  free(response);
  
  return 0;
}

int loginUser(impMsgOp *mop, int connfd)
{
  char *response;

  dbUser_t *ucur = db.ulist->next;
  while(ucur && strcmp(ucur->name, mop->userName)) {
    ucur = ucur->next;
  }

  if(!ucur) {
    response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_USER_DOES_NOT_EXIST, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    return 1;
  } else {
    if(ucur->active && ucur->connfd == connfd) {
      response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_CLIENT_BOUND, mop->userName, IMP_END);
      send(connfd, response, strlen(response), 0);
      free(response);
      return 1;
    } else if(ucur->active && ucur->connfd != connfd) {
      response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_USER_ALREADY_ACTIVE, mop->userName, IMP_END);
      send(connfd, response, strlen(response), 0);
      free(response);
      return 1;
    } else { // perform the login
      sem_wait(udbaseMutexp);
      ucur->active = IMP_ACTIVE_YES;
      ucur->connfd = connfd;
      sem_post(udbaseMutexp);
      response = impStrNew(NULL, IMP_MSG_TYPE_ACK, IMP_OP_LOGIN, mop->userName, IMP_END);
      send(connfd, response, strlen(response), 0);
      free(response);
      // now need to send status messages...
      // first notify logged in user about his friends
      dbFriend_t *fcur = ucur->flist->next;
      while(fcur) {
        if(fcur->status == IMP_FRIEND_YES) {
          response = impStrNew(NULL, IMP_MSG_TYPE_STATUS, fcur->friend->name, IMP_FRIEND_YES, fcur->friend->active, IMP_END);
          send(connfd, response, strlen(response), 0);
          free(response);
        } else {
          response = impStrNew(NULL, IMP_MSG_TYPE_STATUS, fcur->friend->name, fcur->status, IMP_ACTIVE_NOT, IMP_END);
          send(connfd, response, strlen(response), 0);
          free(response);
        }

        fcur = fcur->next;
      }
      // next notify friends about logged in user
      fcur = ucur->flist->next;
      while(fcur) {
        if(fcur->friend->active && fcur->status == IMP_FRIEND_YES) {
          response = impStrNew(NULL, IMP_MSG_TYPE_STATUS, mop->userName, IMP_FRIEND_YES, IMP_ACTIVE_YES, IMP_END);
          send(fcur->friend->connfd, response, strlen(response), 0);
          free(response);
        }
        fcur = fcur->next;
      }
      return 0;
    }
  }
}

int logoutUser(int connfd)
{
  char *response;
  dbUser_t *ucur = db.ulist->next;
    // find user with connfd
  while(ucur && ucur->connfd != connfd) {
    ucur = ucur->next;
  }

  if(!ucur) {
    response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_CLIENT_NOT_BOUND, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    return 1;
  }

  if(!ucur->active) {
    printf("%s %d\n", ucur->name, ucur->active);
    response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_CLIENT_NOT_BOUND, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    printf("no over here\n");
    return 1;
  }

  sem_wait(udbaseMutexp);
  ucur->active = IMP_ACTIVE_NOT;
  ucur->connfd = -1;
  sem_post(udbaseMutexp);
  // ack the logout
  response = impStrNew(NULL, IMP_MSG_TYPE_ACK, IMP_OP_LOGOUT, IMP_END);
  send(connfd, response, strlen(response), 0);
  free(response);
  // need to update user's friends about logout
  dbFriend_t *fcur = ucur->flist->next;
  while(fcur) {
    if(fcur->friend->active && fcur->status == IMP_FRIEND_YES) {
      response = impStrNew(NULL, IMP_MSG_TYPE_STATUS, ucur->name, IMP_FRIEND_YES, IMP_ACTIVE_NOT, IMP_END);
      send(fcur->friend->connfd, response, strlen(response), 0);
      free(response);
    }
    fcur = fcur->next;
  }

  return 0;
}

int friendRequest(impMsgOp *mop, int connfd)
{
  char *response;

  // need to locate our own user first
  dbUser_t *ucur = db.ulist->next;

  while(ucur && ucur->connfd != connfd) {
    ucur = ucur->next;
  }

  if(!ucur) {
    response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_CLIENT_NOT_BOUND, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    return 1;
  }

  dbUser_t *requester = ucur;
  if(!strcmp(requester->name, mop->userName)) {
    response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_BAD_COMMAND, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    return 1;
  }

  // need to located the friend being requested
  ucur = db.ulist->next;
  while(ucur && strcmp(ucur->name, mop->userName)) {
    ucur = ucur->next;
  }

  if(!ucur) {
    response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_USER_DOES_NOT_EXIST, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    return 1;
  }

  dbUser_t *requested = ucur;
  // go through requested user's frined list to see if already friends
  dbFriend_t *fcur = requested->flist->next;
  dbFriend_t *fprev = requested->flist;
  while(fcur && strcmp(fcur->friend->name, requester->name)) {
    fprev = fcur;
    fcur = fcur->next;
  }

  if(fcur) {
    if(fcur->status == IMP_FRIEND_TOANSWER) {
      response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_REQUESTED_ALREADY, mop->userName, IMP_END);
      send(connfd, response, strlen(response), 0);
      free(response);
    } else if(fcur->status == IMP_FRIEND_REQUESTED) {
      // then make the users friends
      sem_wait(udbaseMutexp);
      fcur->status = IMP_FRIEND_YES;
      dbFriend_t *rfcur = requester->flist->next;
      while(rfcur && strcmp(rfcur->friend->name, requested->name)) {
        rfcur = rfcur->next;
      }
      rfcur->status = IMP_FRIEND_YES;
      sem_post(udbaseMutexp);
      // send status updates
      // to requester
      response = impStrNew(NULL, IMP_MSG_TYPE_STATUS, requested->name, IMP_FRIEND_YES, requested->active, IMP_END);
      send(connfd, response, strlen(response), 0);
      free(response);
      // to requested
      if(requested->active) {
        response = impStrNew(NULL, IMP_MSG_TYPE_STATUS, requester->name, IMP_FRIEND_YES, requester->active, IMP_END);
        send(requested->connfd, response, strlen(response), 0);
        free(response);
      }
    } else {
      // already friends
      response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_FRIEND_ALREADY, requested->name, IMP_END);
      send(connfd, response, strlen(response), 0);
      free(response);
    }
  } else {
    // not friends.. need to place request
    sem_wait(udbaseMutexp);
    fprev->next = dbFriendInit(requester->name, IMP_FRIEND_TOANSWER);
    dbFriend_t *rfcur = requester->flist->next;
    dbFriend_t *rfprev = requester->flist;
    while(rfcur) {
      rfprev = rfcur;
      rfcur = rfcur->next;
    }
    rfprev->next = dbFriendInit(requested->name, IMP_FRIEND_REQUESTED);
    sem_post(udbaseMutexp);
    // send status updates
    // to requester
    response = impStrNew(NULL, IMP_MSG_TYPE_STATUS, requested->name, IMP_FRIEND_REQUESTED, IMP_ACTIVE_NOT, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    // to requested
    if(requested->active) {
      response = impStrNew(NULL, IMP_MSG_TYPE_STATUS, requester->name, IMP_FRIEND_TOANSWER, IMP_ACTIVE_NOT, IMP_END);
      send(requested->connfd, response, strlen(response), 0);
      free(response);
    }
  }

  return 0;
}

int friendRemove(impMsgOp *mop, int connfd)
{
  char *response;

  // need to locate our own user first
  dbUser_t *ucur = db.ulist->next;

  while(ucur && ucur->connfd != connfd) {
    ucur = ucur->next;
  }

  if(!ucur) {
    response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_CLIENT_NOT_BOUND, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    return 1;
  }

  dbUser_t *requester = ucur;
  if(!strcmp(requester->name, mop->userName)) {
    response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_BAD_COMMAND, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    return 1;
  }

  // need to located the friend being removed
  ucur = db.ulist->next;
  while(ucur && strcmp(ucur->name, mop->userName)) {
    ucur = ucur->next;
  }

  if(!ucur) {
    response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_USER_DOES_NOT_EXIST, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    return 1;
  }

  dbUser_t *requested = ucur;
  // go through requested user's frined list to see if already friends
  dbFriend_t *fcur = requested->flist->next;
  dbFriend_t *fprev = requested->flist;
  while(fcur && strcmp(fcur->friend->name, requester->name)) {
    fprev = fcur;
    fcur = fcur->next;
  }

  if(fcur) {
    // valid remove
    sem_wait(udbaseMutexp);
    // remove requester from requested (for removal) friend's list
    fprev->next = fcur->next;
    free(fcur);
    // remove requested from requester's friend list
    dbFriend_t *rfcur = requester->flist->next;
    dbFriend_t *rfprev = requester->flist;
    while(rfcur && strcmp(rfcur->friend->name, requested->name)) {
      rfprev = rfcur;
      rfcur = rfcur->next;
    }
    rfprev->next = rfcur->next;
    free(rfcur);  
    sem_post(udbaseMutexp);
    // send status updates
    // to requester
    response = impStrNew(NULL, IMP_MSG_TYPE_STATUS, requested->name, IMP_FRIEND_NOT, IMP_ACTIVE_NOT, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    // to requested
    if(requested->active) {
      response = impStrNew(NULL, IMP_MSG_TYPE_STATUS, requester->name, IMP_FRIEND_NOT, IMP_ACTIVE_NOT, IMP_END);
      send(requested->connfd, response, strlen(response), 0);
      free(response);
    }
  } else {
    // not friends..
    response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_NOT_FRIEND, requested->name, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    return 1;
  }

  return 0;
}

int friendList(int connfd)
{
  char *response;
  // locate user
  dbUser_t *ucur = db.ulist->next;
  while(ucur && ucur->connfd != connfd) {
    ucur = ucur->next;
  }

  if(!ucur) {
    response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_CLIENT_NOT_BOUND, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    return 1;
  }

  dbFriend_t *fcur = ucur->flist->next;
  while(fcur) {
    if(fcur->status == IMP_FRIEND_YES) {
      response = impStrNew(NULL, IMP_MSG_TYPE_STATUS, fcur->friend->name, IMP_FRIEND_YES, fcur->friend->active, IMP_END);
      send(connfd, response, strlen(response), 0);
      free(response);
    } else {
      response = impStrNew(NULL, IMP_MSG_TYPE_STATUS, fcur->friend->name, fcur->status, IMP_ACTIVE_NOT, IMP_END);
      send(connfd, response, strlen(response), 0);
      free(response);
    }

    fcur = fcur->next;
  }

  return 0;
}

int validIM(char *IM)
{
  int len = strlen(IM);
  if(!(1 <= len && len <= IMP_IM_MAXLEN)) {
    return 0;
  }

  int i;
  for(i = 0; i != len; i++) {
    if(isspace(IM[i]) && IM[i] != ' ') {
      return 0;
    }
    if(!isprint(IM[i])) {
      return 0;
    }
  }

  return 1;
}

int imUser(impMsgOp *mop, int connfd)
{
  char *response;
  // locate sender
  dbUser_t *ucur = db.ulist->next;
  while(ucur && ucur->connfd != connfd) {
    ucur = ucur->next;
  }

  if(!ucur) {
    response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_CLIENT_NOT_BOUND, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    return 1;
  }

  dbUser_t *sender = ucur;

  // locate recipient
  ucur = db.ulist->next;
  while(ucur && strcmp(ucur->name, mop->userName)) {
    ucur = ucur->next;
  }

  if(!ucur) {
    response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_USER_DOES_NOT_EXIST, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    return 1;
  }

  dbUser_t *recipient = ucur;

  // check to see if they are friends
  dbFriend_t *fcur = sender->flist->next;
  while(fcur && strcmp(fcur->friend->name, recipient->name)) {
    fcur = fcur->next;
  }

  // not friends
  if(!fcur || fcur->status != IMP_FRIEND_YES) {
    response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_NOT_FRIEND, recipient->name, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    return 1;
  }

  // friend inactive
  if(!recipient->active) {
    response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_USER_NOT_ACTIVE, recipient->name, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    return 1;
  }

  // check im
  if(!validIM(mop->IM)) {
    response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_BAD_COMMAND, IMP_END);
    send(connfd, response, strlen(response), 0);
    free(response);
    return 1;
  }
  // forward im
  response = impStrNew(NULL, IMP_MSG_TYPE_OP, IMP_OP_IM, sender->name, mop->IM, IMP_END);
  send(recipient->connfd, response, strlen(response), 0);
  free(response);
  // ack to sender
  response = impStrNew(NULL, IMP_MSG_TYPE_ACK, IMP_OP_IM, recipient->name, IMP_END);
  send(connfd, response, strlen(response), 0);
  free(response);


  return 0;
}

/* processUserStr and userThread adapted from / modeled after txtimc.c */

int processUserStr(char *recv_str, int connfd)
{
  char me[] = "processUserStr";
  int i, start;
  size_t len = strlen(recv_str);
  impEm *iem = impEmNew();

  printf("%s <--\n", recv_str);

  char *pstr;
  start = 0;
  do {
    pstr = recv_str + start;
    for(i = start; i < len; i++) {
      if (recv_str[i] == '\n') {
        recv_str[i] = '\0';
        start = i+1;
        break;
      }
    }

    printf("%s <--\n", pstr);

    impMsg *pm = impStrToMsg(iem, pstr);
    if(!pm) {
      char *response = impStrNew(NULL, IMP_MSG_TYPE_ERROR, IMP_ERROR_BAD_COMMAND, IMP_END);
      send(connfd, response, strlen(response), 0);
      fprintf(stderr, "%s: problem parsing response \"%s\":\n", me, pstr);
      impEmFprint(stderr, iem);
      impEmFree(iem);
      continue;
    }

    if(pm->mt != IMP_MSG_TYPE_OP) {
      fprintf(stderr, "%s: invalid msg type (need op)\n", me);
      impEmFprint(stderr, iem);
      impEmFree(iem);
      continue;
    }

    impMsgOp *mop = (impMsgOp *)pm;
    // handle the various operations

    switch(mop->op) {
      case IMP_OP_REGISTER:
        if(registerUser(mop, connfd)) {
          fprintf(stderr, "%s: invalid registration:\n", me);
          impEmFprint(stderr, iem);
          impEmFree(iem);
          continue;
        }
        break;

      case IMP_OP_LOGIN:
        if(loginUser(mop, connfd)) {
          fprintf(stderr, "%s: invalid login:\n", me);
          impEmFprint(stderr, iem);
          impEmFree(iem);
          continue;
        }
        break;

      case IMP_OP_LOGOUT:
        if(logoutUser(connfd)) {
          fprintf(stderr, "%s: invalid logout:\n", me);
          impEmFprint(stderr, iem);
          impEmFree(iem);
          continue;
        }
        break;

      case IMP_OP_FRIEND_REQUEST:
        if(friendRequest(mop, connfd)) {
          fprintf(stderr, "%s: invalid friend request:\n", me);
          impEmFprint(stderr, iem);
          impEmFree(iem);
          continue;
        }
        break;

      case IMP_OP_FRIEND_REMOVE:
        if(friendRemove(mop, connfd)) {
          fprintf(stderr, "%s: invalid friend remove:\n", me);
          impEmFprint(stderr, iem);
          impEmFree(iem);
          continue;
        }
        break;

      case IMP_OP_FRIEND_LIST:
        if(friendList(connfd)) {
          fprintf(stderr, "%s: invalid friend list:\n", me);
          impEmFprint(stderr, iem);
          impEmFree(iem);
          continue;
        }
        break;

      case IMP_OP_IM:
        if(imUser(mop, connfd)) {
          fprintf(stderr, "%s: invalid im:\n", me);
          impEmFprint(stderr, iem);
          impEmFree(iem);
          continue;
        }
        break;

      default:
        continue;
    }

    impMsgFree((impMsg *)mop);
  } while(start < len);

  return 0;
}

void *userThread(void *vargp)
{
  int connfd = *((int *)vargp);
  pthread_detach(pthread_self());
  free(vargp);
  char me[] = "userThread";
  // send ack of connect
  char *prot_str = impStrNew(NULL, IMP_MSG_TYPE_ACK, IMP_OP_CONNECT, IMP_END);
  send(connfd, prot_str, strlen(prot_str), 0);
  // enter loop where we recv commands from client
  int numrecvd;
  char recv_str[BUFF_SIZE];
  do {
    bzero(recv_str, BUFF_SIZE);
    numrecvd = recv(connfd, recv_str, BUFF_SIZE, 0);
    if(numrecvd == -1) {
      fprintf(stderr, "%s: recv error\n", me);
      continue;
    }
    if(numrecvd) {
      processUserStr(recv_str, connfd);
    }
  } while(numrecvd && !quitting);

  logoutUser(connfd);

  close(connfd);
  return NULL;
}