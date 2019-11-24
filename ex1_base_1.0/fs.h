#ifndef FS_H
#define FS_H

#include "lib/bst.h"
#include <pthread.h>
#include <unistd.h>
#include "lib/inodes.h"
#include "tecnicofs-api-constants.h"

#define TABELA_FA_SIZE 5

typedef struct openfile{
	char *filename;
	permission mode;
} openfile_t;

typedef openfile_t* openfileLink;

/* estrutura auxiliar */
typedef struct bst{
    node* bstRoot;
    pthread_rwlock_t rwBstLock;
} Bst;  

typedef Bst** bstLink;

typedef struct tecnicofs {
    bstLink hashtable;
    int numBst;     
} tecnicofs;

tecnicofs* new_tecnicofs(int numberBuckets);
void free_tecnicofs(tecnicofs* fs);
int create(tecnicofs* fs, char *name, char* permissions, uid_t owner);
int delete(tecnicofs* fs, char *name, uid_t userid, openfileLink *tabFichAbertos);
int lookup(tecnicofs* fs, char *name);
void print_tecnicofs_trees(FILE * fp, tecnicofs *fs);
void renameFile(tecnicofs *fs, char *name, char* nameAux);

#endif /* FS_H */
