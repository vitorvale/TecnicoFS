#ifndef FS_H
#define FS_H

#include "lib/bst.h"
#include <pthread.h>
#include <unistd.h>
#include "lib/inodes.h"
#include "tecnicofs-api-constants.h"

#define TABELA_FA_SIZE 5

/* estrutura que representa um ficheiro aberto */
typedef struct openfile{
	char *filename;
	permission mode;
} openfile_t;

typedef openfile_t* openfileLink;

/* estrutura auxiliar para a hashtable */
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
int renameFile(tecnicofs *fs, char *name, char* nameAux, uid_t user, openfileLink *tabFichAbertos);
void renameUnlock(tecnicofs *fs,int flagTrylockOne, int flagTrylockTwo, int ix1, int ix2);
int openFile(tecnicofs *fs, openfileLink *tabFichAbertos, char *filename, permission mode, uid_t user);
int closeFile(tecnicofs *fs, openfileLink *tabFichAbertos, int fd);
int writeToFile(tecnicofs *fs, openfileLink *tabFichAbertos, int index, char* content);
int readFromFile(tecnicofs *fs, openfileLink *tabFichAbertos, int index, char *buffer, int len);

#endif /* FS_H */
