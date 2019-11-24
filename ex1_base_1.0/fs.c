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
    fs->hashtable = (bstLink) malloc(sizeof(Bst)*numberBuckets);

    for (i = 0; i < fs->numBst; i++){
        fs->hashtable[i] = (Bst*) malloc(sizeof(Bst));
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

    return 0;
}

void delete(tecnicofs* fs, char *name){
    int ix = 0;
    ix = hash(name, fs->numBst);

    if(pthread_rwlock_wrlock(&(fs->hashtable[ix]->rwBstLock))){
        exit(EXIT_FAILURE);
    }

	fs->hashtable[ix]->bstRoot = remove_item(fs->hashtable[ix]->bstRoot, name);
    
    if(pthread_rwlock_unlock(&(fs->hashtable[ix]->rwBstLock))){
        exit(EXIT_FAILURE);
    }
}

int lookup(tecnicofs* fs, char *name){
    int ix = 0;
    ix = hash(name, fs->numBst);

    if(pthread_rwlock_rdlock(&(fs->hashtable[ix]->rwBstLock))){
        exit(EXIT_FAILURE);
    }

	int inumber = 0;
	node* searchNode = search(fs->hashtable[ix]->bstRoot, name);
	if ( searchNode ){ 
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

void renameFile(tecnicofs *fs, char *name, char* nameAux){
    int flagTrylockOne = 0, flagTrylockTwo = 0; /* as flags servem para indicar os locks efetuados */ 
    int iNumberAux = 0;
    int ix1 = 0, ix2 = 0;

    ix1 = hash(name, fs->numBst);
    ix2 = hash(nameAux, fs->numBst);

    struct timespec tim, tim2;
    tim.tv_sec = 0;
    while ((lookup(fs, name) != 0) && (lookup(fs, nameAux) == 0)){
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
    if ((flagTrylockOne != 0) || (flagTrylockTwo != 0)){
        node *nodeAux;

        nodeAux = search(fs->hashtable[ix1]->bstRoot, name);
        iNumberAux = nodeAux->inumber;
        fs->hashtable[ix1]->bstRoot = remove_item(fs->hashtable[ix1]->bstRoot, name);
        fs->hashtable[ix2]->bstRoot = insert(fs->hashtable[ix2]->bstRoot, nameAux, iNumberAux);
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
}

