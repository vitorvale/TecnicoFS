#define _POSIX_C_SOURCE 200809L 

#include "fs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>       
#include <unistd.h>

#include "lib/hash.h"

#define BASE_DELAY 1000

tecnicofs* new_tecnicofs(int numberBuckets){
    int i = 0;
	tecnicofs*fs = malloc(sizeof(tecnicofs));
	if (!fs) {
		perror("failed to allocate tecnicofs");
		exit(EXIT_FAILURE);
	}

    fs->numBst = numberBuckets;
    inode_table_init(); 
    fs->hashtable = (bstLink) malloc(sizeof(Bst*)*numberBuckets);
    if (!fs->hashtable) {
        perror("failed to allocate hashtable");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < fs->numBst; i++){
        fs->hashtable[i] = (Bst*) malloc(sizeof(Bst));
        if (!fs->hashtable[i]) {
            perror("failed to allocate hashtable index");
            exit(EXIT_FAILURE);
        }
        fs->hashtable[i]->bstRoot = NULL;
        if(pthread_rwlock_init(&(fs->hashtable[i]->rwBstLock), NULL) != 0){
            exit(EXIT_FAILURE);
        }
    }
	return fs;
}

void free_tecnicofs(tecnicofs* fs){
    int i = 0;
    for (i = 0; i < fs->numBst; i++){
        free_tree(fs->hashtable[i]->bstRoot);
        if(pthread_rwlock_destroy(&(fs->hashtable[i]->rwBstLock)) != 0){
            exit(EXIT_FAILURE);
        }
    }
	
    for(i = 0; i < fs->numBst; i++){
        free(fs->hashtable[i]);
    }
    free(fs->hashtable);
    inode_table_destroy();
	free(fs);
}

int create(tecnicofs* fs, char *name, char* permissions, uid_t owner){
    int ix = 0, inumber;
    permission ownerPermissions = permissions[0];
    permission othersPermissions = permissions[1];
    ix = hash(name, fs->numBst);

    if(pthread_rwlock_wrlock(&(fs->hashtable[ix]->rwBstLock)))
        exit(EXIT_FAILURE);
    
    if(search(fs->hashtable[ix]->bstRoot, name)){
        if(pthread_rwlock_unlock(&fs->hashtable[ix]->rwBstLock))
            exit(EXIT_FAILURE);
        return TECNICOFS_ERROR_FILE_ALREADY_EXISTS;
    }  
    inumber = inode_create(owner, ownerPermissions, othersPermissions);
    if(inumber == -1){
        if(pthread_rwlock_unlock(&fs->hashtable[ix]->rwBstLock))
            exit(EXIT_FAILURE);
        return TECNICOFS_ERROR_OTHER;
    }
	fs->hashtable[ix]->bstRoot = insert(fs->hashtable[ix]->bstRoot, name, inumber);
    if(pthread_rwlock_unlock(&fs->hashtable[ix]->rwBstLock)){
        exit(EXIT_FAILURE);
    }

    return SUCCESS;
}

int delete(tecnicofs* fs, char *name, uid_t userid, openfileLink *tabFichAbertos){
    int ix = 0, i = 0;
    node *t;
    ix = hash(name, fs->numBst);
    uid_t owner;

    if(pthread_rwlock_wrlock(&(fs->hashtable[ix]->rwBstLock))){
        exit(EXIT_FAILURE);
    }

    if((t = search(fs->hashtable[ix]->bstRoot, name)) == NULL){
        if(pthread_rwlock_unlock(&fs->hashtable[ix]->rwBstLock))
            exit(EXIT_FAILURE);
        return TECNICOFS_ERROR_FILE_NOT_FOUND;
    }

    for(i = 0; i < TABELA_FA_SIZE; i++){
        if((tabFichAbertos[i] != NULL) && (!strcmp(tabFichAbertos[i]->filename, name))){
            if(pthread_rwlock_unlock(&fs->hashtable[ix]->rwBstLock))
                exit(EXIT_FAILURE);
            return TECNICOFS_ERROR_FILE_IS_OPEN;
        }
    }

    if(inode_get(t->inumber, &owner, NULL, NULL, NULL, 0) == -1){
        if(pthread_rwlock_unlock(&fs->hashtable[ix]->rwBstLock))
            exit(EXIT_FAILURE);
        return TECNICOFS_ERROR_OTHER;
    }

    if(owner != userid){
        if(pthread_rwlock_unlock(&fs->hashtable[ix]->rwBstLock))
            exit(EXIT_FAILURE);
        return TECNICOFS_ERROR_PERMISSION_DENIED;
    }

    if(inode_delete(t->inumber) == -1){
        if(pthread_rwlock_unlock(&fs->hashtable[ix]->rwBstLock))
            exit(EXIT_FAILURE);
        return TECNICOFS_ERROR_OTHER;
    }

	fs->hashtable[ix]->bstRoot = remove_item(fs->hashtable[ix]->bstRoot, name);
    
    if(pthread_rwlock_unlock(&(fs->hashtable[ix]->rwBstLock))){
        exit(EXIT_FAILURE);
    }
    return SUCCESS;
}

int lookup(tecnicofs* fs, char *name){
    int ix = 0;
    ix = hash(name, fs->numBst);

    if(pthread_rwlock_rdlock(&(fs->hashtable[ix]->rwBstLock))){
        exit(EXIT_FAILURE);
    }

	int inumber = -1;
	node* searchNode = search(fs->hashtable[ix]->bstRoot, name);
	if (searchNode != NULL){ 
		inumber = searchNode->inumber;
	}
    if(pthread_rwlock_unlock(&(fs->hashtable[ix]->rwBstLock))){
        exit(EXIT_FAILURE);
    }

	return inumber;
}

void print_tecnicofs_trees(FILE * fp, tecnicofs *fs){
    int i = 0;

    for (i = 0; i < fs->numBst; i++){
        print_tree(fp, fs->hashtable[i]->bstRoot);
    }
}

void renameUnlock(tecnicofs *fs,int flagTrylockOne, int flagTrylockTwo, int ix1, int ix2){
    if (flagTrylockOne == 0){
        if(pthread_rwlock_unlock(&(fs->hashtable[ix1]->rwBstLock))){
            exit(EXIT_FAILURE);
        }
        if(pthread_rwlock_unlock(&(fs->hashtable[ix2]->rwBstLock))){
            exit(EXIT_FAILURE);
        }
    }
    else if (flagTrylockTwo == 0){
        if(pthread_rwlock_unlock(&(fs->hashtable[ix1]->rwBstLock))){
            exit(EXIT_FAILURE);
        }
    }
    
}

int renameFile(tecnicofs *fs, char *name, char* nameAux, uid_t user, openfileLink *tabFichAbertos){
    int flagTrylockOne = 0, flagTrylockTwo = 0; /* as flags servem para indicar os locks efetuados */ 
    int iNumberAux = 0;
    int ix1 = 0, ix2 = 0;
    node *nodeAux;
    uid_t owner;

    ix1 = hash(name, fs->numBst);
    ix2 = hash(nameAux, fs->numBst);

    struct timespec tim, tim2;
    tim.tv_sec = 0;
    while(1){
        int numAttempts = 0;
        if (pthread_rwlock_trywrlock(&(fs->hashtable[ix1]->rwBstLock)) == 0){
            if ((ix1 != ix2) && (pthread_rwlock_trywrlock(&(fs->hashtable[ix2]->rwBstLock)) == 0)){
                flagTrylockTwo++;
                break;
            }
            else if (ix1 == ix2){
                flagTrylockOne++;
                break;
            }
            else{
                numAttempts++;
                if(pthread_rwlock_unlock(&(fs->hashtable[ix1]->rwBstLock))){
                    exit(EXIT_FAILURE);
                }  
            }
        }
        else{
            numAttempts++;
        }
        tim.tv_nsec = rand() % (BASE_DELAY * numAttempts);
        if(nanosleep(&tim, &tim2) < 0){
            exit(EXIT_FAILURE);
        }    
    }

    nodeAux = search(fs->hashtable[ix1]->bstRoot, name);
    iNumberAux = nodeAux->inumber;

    if((inode_get(iNumberAux, &owner, NULL, NULL , NULL, 0)) == -1){
        renameUnlock(fs, flagTrylockOne, flagTrylockTwo, ix1, ix2);
        return TECNICOFS_ERROR_OTHER;
    }

    if (owner != user){
        renameUnlock(fs, flagTrylockOne, flagTrylockTwo, ix1, ix2);
        return TECNICOFS_ERROR_PERMISSION_DENIED;
    }

    if (search(fs->hashtable[ix1]->bstRoot, name) == NULL){
        renameUnlock(fs, flagTrylockOne, flagTrylockTwo, ix1, ix2);
        return TECNICOFS_ERROR_FILE_NOT_FOUND;
    }

    if(search(fs->hashtable[ix2]->bstRoot, nameAux) != NULL){
        renameUnlock(fs, flagTrylockOne, flagTrylockTwo, ix1, ix2);
        return TECNICOFS_ERROR_FILE_ALREADY_EXISTS;
    }
    
    for(int i = 0; i < TABELA_FA_SIZE; i++){
        if((tabFichAbertos[i] != NULL) && (!strcmp(tabFichAbertos[i]->filename, name))){
            renameUnlock(fs, flagTrylockOne, flagTrylockTwo, ix1, ix2);
            return TECNICOFS_ERROR_FILE_IS_OPEN;
        }
    }

    if ((flagTrylockOne != 0) || (flagTrylockTwo != 0)){
        fs->hashtable[ix1]->bstRoot = remove_item(fs->hashtable[ix1]->bstRoot, name);
        fs->hashtable[ix2]->bstRoot = insert(fs->hashtable[ix2]->bstRoot, nameAux, iNumberAux);
        renameUnlock(fs, flagTrylockOne, flagTrylockTwo, ix1, ix2);
    }

    return SUCCESS;
}

int openFile(tecnicofs *fs, openfileLink *tabFichAbertos, char *filename, permission mode, uid_t user){
    int i = 0, searchResult;

    searchResult = lookup(fs, filename);
    if(searchResult == -1){
        return TECNICOFS_ERROR_FILE_NOT_FOUND;
    }
    else{
        uid_t owner;
        permission ownerPermissions, othersPermissions;

        if(inode_get(searchResult, &owner, &ownerPermissions, &othersPermissions, NULL, 0) == -1)
            return TECNICOFS_ERROR_OTHER;

        if(user != owner){
            if((othersPermissions == READ) && (mode == WRITE))
                return TECNICOFS_ERROR_INVALID_MODE;

            else if((othersPermissions == WRITE) && (mode == READ))
                return TECNICOFS_ERROR_INVALID_MODE;
        }

        openfileLink file = (openfileLink) malloc(sizeof(openfile_t));
        if (!file) {
            perror("failed to allocate openfile\n");
            exit(EXIT_FAILURE);
        }
        file->filename = (char*) malloc(sizeof(char)*strlen(filename));
        if (!(file->filename)) {
            perror("failed to allocate string\n");
            exit(EXIT_FAILURE);
        }
        strcpy(file->filename, filename);
        file->mode = mode;

        while (tabFichAbertos[i] != NULL) i++;

        tabFichAbertos[i] = file;

        return SUCCESS;            
    }    
}

int closeFile(tecnicofs *fs, openfileLink *tabFichAbertos, int fd){
    if (tabFichAbertos[fd] != NULL){
        free(tabFichAbertos[fd]->filename);
        free(tabFichAbertos[fd]);
        tabFichAbertos[fd] = NULL;
        return SUCCESS;
    }
    else{
        return TECNICOFS_ERROR_FILE_NOT_OPEN;
    }
}

int writeToFile(tecnicofs *fs, openfileLink *tabFichAbertos, int index, char* content){
    int inumber;
    
    if (tabFichAbertos[index] == NULL){
        return TECNICOFS_ERROR_FILE_NOT_OPEN;
    }
    
    if (tabFichAbertos[index]->mode == READ){
        return TECNICOFS_ERROR_INVALID_MODE;
    }

    inumber = lookup(fs, tabFichAbertos[index]->filename);

    if (inode_set(inumber, content, strlen(content)) == -1){
        return TECNICOFS_ERROR_OTHER;
    }

    return SUCCESS;
}

int readFromFile(tecnicofs *fs, openfileLink *tabFichAbertos, int index, char *buffer, int len){
    int inumber, size;

    if (tabFichAbertos[index] == NULL){
        return TECNICOFS_ERROR_FILE_NOT_OPEN;
    }

    if (tabFichAbertos[index]->mode == WRITE){
        return TECNICOFS_ERROR_INVALID_MODE;
    }

    inumber = lookup(fs, tabFichAbertos[index]->filename);

    if((size = inode_get(inumber, NULL, NULL, NULL, buffer, len - 1)) == -1){
        return TECNICOFS_ERROR_OTHER;
    }

    return size;
}