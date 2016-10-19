/*
** cs154-2015 Project 5 ("p5ims") Instant Message Server
** udbase.c: for reading and writing the user database
*/

#include "ims.h"

/* your in-memory representation of user database can be pointed to by some
   global variables, declared in ims.h, defined here, and initialized by
   udbaseRead below.  When server is running with multiple threads, access to
   these globals should be guarded by a mutex. */

/* copied from cndb.c */

int getliner(char **pline, size_t *psize, FILE *file, ssize_t wantlen) {
  char me[] = "getliner";
  ssize_t red;

  red = getline(pline, psize, file);
  if(red <= -1) {
    fprintf(stderr, "%s: hit EOF\n", me);
    return 1;
  } else if(red <= 1) {
    fprintf(stderr, "%s: got empty line\n", me);
    return 1;
  }
  if('\n' != (*pline)[red-1]) {
    fprintf(stderr, "%s: line didn't end with '\\n'\n", me);
    return 1;
  }
  (*pline)[red-1] = '\0';
  if(!( red-1 >= wantlen )) {
    fprintf(stderr, "%s: got line \"%s\" with %u chars before \\n, "
            "but wanted >= %u\n", me, *pline, (unsigned int)(red-1),
            (unsigned int)wantlen);
    return 1;
  }
  return 0;
}

/* next four functions adapted from cndb.c */

dbFriend_t *dbFriendInit(char *name, impFriend_t status) {
  dbFriend_t *result = malloc(sizeof(dbFriend_t));
  result->friend = NULL;
  if(name) {
    // search user database for this user
    dbUser_t *ucur = db.ulist->next;
    while(ucur) {
      if(!strcmp(name, ucur->name)) {
        result->friend = ucur;
        break;
      }
      ucur = ucur->next;
    }
  }
  result->status = status;
  result->next = NULL;
  return result;
}

dbUser_t *dbUserInit(char *name) {
  dbUser_t *result = malloc(sizeof(dbUser_t));
  result->name = NULL;
  if(name) {
    result->name = strdup(name);
  }
  result->active = IMP_ACTIVE_NOT;
  result->connfd = -1;
  result->flist = dbFriendInit(NULL, IMP_FRIEND_NOT);
  result->next = NULL;
  return result;
}

int udbaseRead(impEm *iem) {
  static const char me[] = "udbaseRead";
  FILE *file;

  file = fopen(udbaseFilename, "r");
  if(!file) {
    impEmAdd(iem, "%s: couldn't open \"%s\" for reading: %s", me, udbaseFilename, strerror(errno));
    return 1;
  }

  /* ... YOUR CODE HERE to read database file into memory. Assuming
     that this is called before the server starts taking connections
     from clients, this does not need to be thread-safe. */

  /* adapted from cndb.c */
  char *line;
  size_t lsize;

  lsize = 0;
  line = NULL;
  if(getliner(&line, &lsize, file, 8)) {
    fprintf(stderr, "%s: couldn't read first line\n", me);
    return 1;
  }
  assert(line);

  unsigned int nuser;
  sscanf(line, "%u users:", &nuser);

  db.unum = nuser;
  db.ulist = dbUserInit(NULL);

  dbUser_t *ucur = db.ulist;

  // first read in users, skip friends
  
  unsigned int ui;
  for(ui = 0; ui < nuser; ui++) {
    if(getliner(&line, &lsize, file, 1)) {
      fprintf(stderr, "%s: couldn't read username %u/%u\n", me, ui, nuser);
      return 1;
    }

    ucur->next = dbUserInit(line);
    ucur = ucur->next;

    do {
      if(getliner(&line, &lsize, file, 1)) {
        fprintf(stderr, "%s: couldn't read friend line of user %u (%s)\n", me, ui, ucur->name);
        return 1;
      }
    } while (strcmp(".", line));
  }

  // now read in friends

  fseek(file, 0, SEEK_SET);
  ucur = db.ulist->next;

  getliner(&line, &lsize, file, 8);

  unsigned int li;
  for(ui = 0; ui < nuser; ui++) {
    if(getliner(&line, &lsize, file, 1)) {
      fprintf(stderr, "%s: couldn't read username %u/%u\n", me, ui, nuser);
      return 1;
    }

    dbFriend_t *fcur = ucur->flist;
    ucur = ucur->next;

    li = 0;
    char fname[IMP_NAME_MAXLEN];
    char fstatusstr[10];
    int nmatched, fstatus;
    do {
      if(getliner(&line, &lsize, file, 1)) {
        fprintf(stderr, "%s: couldn't read friend line %u of user %u (%s)\n", me, li, ui, ucur->name);
        return 1;
      }

      if(strcmp(".", line)) {
        if((nmatched = sscanf(line, "- %s %s", fname, fstatusstr)) == 2) {  
          if(!strcmp(fstatusstr, "requested")) {
            fstatus = IMP_FRIEND_REQUESTED;
          } else {
            fstatus = IMP_FRIEND_TOANSWER;
          }
        } else {
          fstatus = IMP_FRIEND_YES;
        }
        fcur->next = dbFriendInit(fname, fstatus);
        fcur = fcur->next;
      }
      li++;
    } while (strcmp(".", line));
  }

  free(line);
  fclose(file);
  return 0;
}

/* you can pass a NULL iem to this if you aren't interested in saving the
   error messages; impEmAdd will have no effect with a NULL iem */
int udbaseWrite(impEm *iem) {
  static const char me[]="udbaseWrite";
  FILE *file;

  /* ... make sure that user database is being written at the same
     that a client thread is modifying it, either with code here,
     or with limits on how udbaseWrite() is called */

  file = fopen(udbaseFilename, "w");
  if (!file) {
    impEmAdd(iem, "%s: couldn't open \"%s\" for writing: %s",
             me, udbaseFilename, strerror(errno));
    return 1;
  }

  /* ... YOUR CODE HERE to write the in-memory database to file */
  fprintf(file, "%u users:\n", db.unum);
  dbUser_t *ucur = db.ulist->next;
  while(ucur) {
    fprintf(file, "%s\n", ucur->name);
    dbFriend_t *fcur = ucur->flist->next;
    while(fcur) {
      fprintf(file, "- %s", fcur->friend->name);
      if(fcur->status == IMP_FRIEND_REQUESTED) {
        fprintf(file, " requested");
      } else if(fcur->status == IMP_FRIEND_TOANSWER) {
        fprintf(file, " toanswer");
      }
      fprintf(file, "\n");
      fcur = fcur->next;
    }
    fprintf(file, ".\n");
    ucur = ucur->next;
  }

  fclose(file);
  return 0;
}
