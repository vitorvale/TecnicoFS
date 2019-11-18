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


int obtainNewInumber(tecnicofs* fs) {
	int newInumber = ++(fs->nextInumber);
	return newInumber;
}

tecnicofs* new_tecnicofs(int numberBuckets){
    int i = 0;
	tecnicofs*fs = malloc(sizeof(tecnicofs));
	if (!fs) {
		perror("failed to allocate tecnicofs");
		exit(EXIT_FAILURE);
	}

    fs->nextInumber = 0;
    fs->numBst = numberBuckets;
    fs->hashtable = (bstLink) malloc(sizeof(Bst)*numberBuckets);

    for (i = 0; i < fs->numBst; i++){
        fs->hashtable[i] = (Bst*) malloc(sizeof(Bst));
        fs->hashtable[i]->bstRoot = NULL;
        #ifdef MUTEX
            if(pthread_mutex_init(&(fs->hashtable[i]->mutexBstLock), NULL) != 0){
                exit(EXIT_FAILURE);
            }
        #elif RWLOCK
            if(pthread_rwlock_init(&(fs->hashtable[i]->rwBstLock), NULL) != 0){
                exit(EXIT_FAILURE);
            }
        #endif
    }

	return fs;
}

void free_tecnicofs(tecnicofs* fs){
    int i = 0;
    for (i = 0; i < fs->numBst; i++){
        free_tree(fs->hashtable[i]->bstRoot);
        #ifdef MUTEX
            if(pthread_mutex_destroy(&(fs->hashtable[i]->mutexBstLock)) != 0){
                exit(EXIT_FAILURE);
            }
        #elif RWLOCK
            if(pthread_rwlock_destroy(&(fs->hashtable[i]->rwBstLock)) != 0){
                exit(EXIT_FAILURE);
            }
        #endif
    }
	
    for(i = 0; i < fs->numBst; i++){
        free(fs->hashtable[i]);
    }
    free(fs->hashtable);
	free(fs);
}

void create(tecnicofs* fs, char *name, int inumber){
    int ix = 0;
    ix = hash(name, fs->numBst);
    #ifdef MUTEX
        if(pthread_mutex_lock(&(fs->hashtable[ix]->mutexBstLock)))
            exit(EXIT_FAILURE);
    #elif RWLOCK
        if(pthread_rwlock_wrlock(&(fs->hashtable[ix]->rwBstLock)))
            exit(EXIT_FAILURE);
    #endif
	fs->hashtable[ix]->bstRoot = insert(fs->hashtable[ix]->bstRoot, name, inumber);
    #ifdef MUTEX
        if(pthread_mutex_unlock(&(fs->hashtable[ix]->mutexBstLock)))
            exit(EXIT_FAILURE);
    #elif RWLOCK
        if(pthread_rwlock_unlock(&fs->hashtable[ix]->rwBstLock))
            exit(EXIT_FAILURE);
    #endif
}

void delete(tecnicofs* fs, char *name){
    int ix = 0;
    ix = hash(name, fs->numBst);
    #ifdef MUTEX
        if(pthread_mutex_lock(&(fs->hashtable[ix]->mutexBstLock)))
            exit(EXIT_FAILURE);
    #elif RWLOCK
        if(pthread_rwlock_wrlock(&(fs->hashtable[ix]->rwBstLock)))
            exit(EXIT_FAILURE);
    #endif
	fs->hashtable[ix]->bstRoot = remove_item(fs->hashtable[ix]->bstRoot, name);
    #ifdef MUTEX
        if(pthread_mutex_unlock(&(fs->hashtable[ix]->mutexBstLock)))
            exit(EXIT_FAILURE);
    #elif RWLOCK
        if(pthread_rwlock_unlock(&(fs->hashtable[ix]->rwBstLock)))
            exit(EXIT_FAILURE);
    #endif
}

int lookup(tecnicofs* fs, char *name){
    int ix = 0;
    ix = hash(name, fs->numBst);
    #ifdef MUTEX
        if(pthread_mutex_lock(&(fs->hashtable[ix]->mutexBstLock)))
            exit(EXIT_FAILURE);    
    #elif RWLOCK
        if(pthread_rwlock_rdlock(&(fs->hashtable[ix]->rwBstLock)))
            exit(EXIT_FAILURE);
    #endif
	int inumber = 0;
	node* searchNode = search(fs->hashtable[ix]->bstRoot, name);
	if ( searchNode ){ 
		inumber = searchNode->inumber;
	}
    #ifdef MUTEX
        if(pthread_mutex_unlock(&(fs->hashtable[ix]->mutexBstLock)))
            exit(EXIT_FAILURE);   
    #elif RWLOCK
        if(pthread_rwlock_unlock(&(fs->hashtable[ix]->rwBstLock)))
            exit(EXIT_FAILURE);
    #endif
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

    #if defined(MUTEX) || defined(RWLOCK)
        struct timespec tim, tim2;
        tim.tv_sec = 0;
        while ((lookup(fs, name) != 0) && (lookup(fs, nameAux) == 0)){
            int numAttempts = 0;

            #ifdef MUTEX
                if (pthread_mutex_trylock(&(fs->hashtable[ix1]->mutexBstLock)) == 0){
                    if ((ix1 != ix2) && (pthread_mutex_trylock(&(fs->hashtable[ix2]->mutexBstLock)) == 0)){
                        flagTrylockTwo++;
                        break;
                    }
                    else if (ix1 == ix2){
                        flagTrylockOne++;
                        break;
                    }
                    else{
                        numAttempts++; 
                        if(pthread_mutex_unlock(&(fs->hashtable[ix1]->mutexBstLock))){
                            exit(EXIT_FAILURE);
                        } 
                    }
                }
            #elif RWLOCK
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
            #endif        
                else{
                    numAttempts++;
                }
                tim.tv_nsec = rand() % (BASE_DELAY * numAttempts);
                if(nanosleep(&tim, &tim2) < 0){
                    exit(EXIT_FAILURE);
                }    
        }
    #else
        if ((search(fs->hashtable[ix1]->bstRoot, name) != NULL) && (search(fs->hashtable[ix2]->bstRoot, nameAux) == NULL)){
            flagTrylockOne++; /* serve para entrar no bloco que faz o rename, apesar de nao fazermos lock */
        }
    #endif      
    if ((flagTrylockOne != 0) || (flagTrylockTwo != 0)){
        node *nodeAux;

        nodeAux = search(fs->hashtable[ix1]->bstRoot, name);
        iNumberAux = nodeAux->inumber;
        fs->hashtable[ix1]->bstRoot = remove_item(fs->hashtable[ix1]->bstRoot, name);
        fs->hashtable[ix2]->bstRoot = insert(fs->hashtable[ix2]->bstRoot, nameAux, iNumberAux);
        #ifdef MUTEX
            if (flagTrylockOne == 0){
                if(pthread_mutex_unlock(&(fs->hashtable[ix1]->mutexBstLock))){
                    exit(EXIT_FAILURE);
                }
                if(pthread_mutex_unlock(&(fs->hashtable[ix2]->mutexBstLock))){
                    exit(EXIT_FAILURE);
                }
            }
            else if (flagTrylockTwo == 0){
                if(pthread_mutex_unlock(&(fs->hashtable[ix1]->mutexBstLock))){
                    exit(EXIT_FAILURE);
                }
            }
        #elif RWLOCK
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
        #endif   
    }    

}

