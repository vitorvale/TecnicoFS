#ifndef FS_H
#define FS_H
#include "lib/bst.h"
#include <pthread.h>
#include <unistd.h>
#include "lib/inodes.h"

/* estrutura auxiliar */
typedef struct bst{
    node* bstRoot;
    pthread_mutex_t mutexBstLock;
    pthread_rwlock_t rwBstLock;
} Bst;  

typedef Bst** bstLink;

typedef struct tecnicofs {
    bstLink hashtable;
    int nextInumber;
    int numBst;     
} tecnicofs;

int obtainNewInumber(tecnicofs* fs);
tecnicofs* new_tecnicofs(int numberBuckets);
void free_tecnicofs(tecnicofs* fs);
void create(tecnicofs* fs, char *name, int inumber);
void delete(tecnicofs* fs, char *name);
int lookup(tecnicofs* fs, char *name);
void print_tecnicofs_trees(FILE * fp, tecnicofs *fs);
void renameFile(tecnicofs *fs, char *name, char* nameAux);

#endif /* FS_H */
