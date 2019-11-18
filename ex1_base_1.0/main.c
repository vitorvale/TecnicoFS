/* Projeto SO - 
    grupo 1 : Vitor Vale  e Tomas Saraiva */
   
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <semaphore.h>
#include "fs.h"
#include "lib/hash.h"

#define MAX_COMMANDS 10
#define MAX_INPUT_SIZE 100

long numberThreads = 0;
long numberBuckets = 0;
int done = 0; 
pthread_t* tid;
struct timeval begin, end;
char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
tecnicofs* fs;
int prodptr = 0;
int consptr = 0;
pthread_mutex_t mutexInumberArrayLock;
pthread_mutex_t mutexDoneLock;
pthread_rwlock_t rwDoneLock;
sem_t prod;
sem_t cons;

static void displayUsage (const char* appName){
    printf("Usage: %s\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs (long argc, char* const argv[]){
    if (argc != 5) {
        fprintf(stderr, "Invalid format:\n");
        displayUsage(argv[0]);
    }
    numberBuckets = strtol(argv[4], NULL, 10);
    numberThreads = strtol(argv[3], NULL, 10);

}

int insertCommand(char* data) {
    
    strcpy(inputCommands[prodptr], data);
    prodptr = (prodptr + 1) % MAX_COMMANDS;

    return 1;
}

char* removeCommand() {
    char *command;

    command = inputCommands[consptr];
    consptr = (consptr + 1) % MAX_COMMANDS;
    return command;
    
}

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    //exit(EXIT_FAILURE);
}

void *processInput(void *file){
    FILE *fin = (FILE *) file;
    char line[MAX_INPUT_SIZE];

    while (fgets(line, sizeof(line)/sizeof(char), fin)) {
        char token;
        char name[MAX_INPUT_SIZE];
        char nameAux[MAX_INPUT_SIZE];

        int numTokens = sscanf(line, "%c", &token);

        if(token != 'r'){
            numTokens += sscanf(line, "%c %s", &token, name) - 1; 
        }
        else{
            numTokens += sscanf(line, "%c %s %s", &token, name, nameAux) - 1;
        }

        /* perform minimal validation */
        if (numTokens < 1) {
            continue;
        }
        switch (token) {
            case 'c':
            case 'l':
            case 'd':
            case 'r':
                if(((numTokens != 2) && (token != 'r')) || ((numTokens != 3) && (token == 'r'))){
                    errorParse();
                }
                if(sem_wait(&prod) == -1){
                    exit(EXIT_FAILURE);
                }
                if(pthread_mutex_lock(&mutexInumberArrayLock))
                    exit(EXIT_FAILURE);    
                if(!insertCommand(line)){
                    if(pthread_mutex_unlock(&mutexInumberArrayLock))
                        exit(EXIT_FAILURE);
                    if(sem_post(&cons) == -1){
                        exit(EXIT_FAILURE);
                    }     
                    return NULL;
                }
                if(pthread_mutex_unlock(&mutexInumberArrayLock))
                    exit(EXIT_FAILURE);
                if(sem_post(&cons) == -1){
                    exit(EXIT_FAILURE);
                }      
                break;    
            case '#':
                break;
            default: { /* error */
                errorParse();
            }
        }
    }
    if(sem_wait(&prod) == -1){
        exit(EXIT_FAILURE);
    }
    if(pthread_mutex_lock(&mutexInumberArrayLock))
        exit(EXIT_FAILURE);
    insertCommand("x");
    if(pthread_mutex_unlock(&mutexInumberArrayLock))
        exit(EXIT_FAILURE);
    if(sem_post(&cons) == -1){
        exit(EXIT_FAILURE);
    }    
    return NULL;
}

void* applyCommands(){
    while(!done){
        if(sem_wait(&cons) == -1){
            exit(EXIT_FAILURE);
        }
        if(done){
            continue;
        }
        if(pthread_mutex_lock(&mutexInumberArrayLock))
            exit(EXIT_FAILURE);

        const char* command = removeCommand();

        if (*command == 'x'){
            #ifdef MUTEX
                if(pthread_mutex_lock(&mutexDoneLock))
                    exit(EXIT_FAILURE);
            #elif RWLOCK
                if(pthread_rwlock_wrlock(&rwDoneLock))
                    exit(EXIT_FAILURE);
            #endif
            done++;
            #ifdef MUTEX
                if(pthread_mutex_unlock(&mutexDoneLock))
                    exit(EXIT_FAILURE);
            #elif RWLOCK
                if(pthread_rwlock_unlock(&rwDoneLock))
                    exit(EXIT_FAILURE);
            #endif
            if(pthread_mutex_unlock(&mutexInumberArrayLock))
                exit(EXIT_FAILURE);
            for(int i = 0; i < numberThreads - 1; i++) sem_post(&cons);
            continue;
        }

        char token;
        char name[MAX_INPUT_SIZE];
        char nameAux[MAX_INPUT_SIZE];
        /* verificacao do token */
        int numTokens = sscanf(command, "%c", &token);

        /* tratamento do resto do comando em funcao do token,
        subtraimos 1 valor para nao contar o token que ja foi lido */
        if(token != 'r'){
            numTokens += sscanf(command, "%c %s", &token, name) - 1; 
        }
        else{
            numTokens += sscanf(command, "%c %s %s", &token, name, nameAux) - 1;
        }

        if ((numTokens != 2) && (token != 'r')) {
            fprintf(stderr, "Error: invalid command in Queue\n");
            exit(EXIT_FAILURE);
        }
        else if((numTokens != 3) && (token == 'r')){
            fprintf(stderr, "Error: invalid command in Queue\n");
            exit(EXIT_FAILURE);
        }
        /* se o comando nao for create damos unlock do mutex
        que protege o nextInumber e o vetor */
        if (token != 'c'){
            if(pthread_mutex_unlock(&mutexInumberArrayLock))
                exit(EXIT_FAILURE);
            if(sem_post(&prod) == -1){
                exit(EXIT_FAILURE);
            }
        }

        int searchResult;
        int iNumber;
        switch (token) {
            case 'c':
                {
                iNumber = obtainNewInumber(fs);
                /* no caso de ser um create damos unlock depois 
                de atualizar o nextInumber da fs */
                if(pthread_mutex_unlock(&mutexInumberArrayLock))
                    exit(EXIT_FAILURE);
                if(sem_post(&prod) == -1){
                    exit(EXIT_FAILURE);
                }    
                create(fs, name, iNumber);
                }
                break;
            case 'l':
                {
                searchResult = lookup(fs, name);
                if(!searchResult)
                    printf( "%s not found\n", name);
                else
                    printf("%s found with inumber %d\n", name, searchResult);
                }    
                break;
            case 'd':
                {
                delete(fs, name);
                }
                break;
            case 'r':
                {
                renameFile(fs, name, nameAux);   
                }
                break;    
            default: { /* error */
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    return NULL;
}


void Threads(FILE *fin){
    int i = 0;

    gettimeofday(&begin, NULL);

    if(pthread_create(&tid[i++], 0, processInput, (void *) fin) != 0){
        exit(EXIT_FAILURE);
    }

    for(; i <= numberThreads; i++){
        if(pthread_create(&tid[i], 0, applyCommands, NULL) != 0){
            exit(EXIT_FAILURE);
        }
    }

    for(i = 0; i < (numberThreads + 1); i++){
        if(pthread_join(tid[i], NULL) != 0){
            exit(EXIT_FAILURE);
        }
    }
    gettimeofday(&end, NULL);
}


int main(int argc, char* argv[]) {
    FILE *fin, *fout;
    time_t t;

    /* gera uma seed baseada no tempo
    do sistema para usar na funcao rand() */
    srand((unsigned) time(&t));
    
    parseArgs(argc, argv);
    tid = (pthread_t*) malloc(sizeof(pthread_t)*(numberThreads + 1));

    if ((fin = fopen(argv[1], "r")) == NULL){
        exit(EXIT_FAILURE);
    }
    if((fout = fopen(argv[2], "w")) == NULL){
        exit(EXIT_FAILURE);
    }

    fs = new_tecnicofs(numberBuckets);
    if(sem_init(&prod, 0, (int) MAX_COMMANDS) == -1){
        exit(EXIT_FAILURE);
    }
    if(sem_init(&cons, 0, 0) == -1){
        exit(EXIT_FAILURE);
    }
    if(pthread_mutex_init(&mutexInumberArrayLock, NULL) != 0){
        exit(EXIT_FAILURE);
    }

    #ifdef MUTEX
        if(pthread_mutex_init(&mutexDoneLock, NULL) != 0){
            exit(EXIT_FAILURE);
        }
    #elif RWLOCK
        if(pthread_rwlock_init(&rwDoneLock, NULL) != 0){
            exit(EXIT_FAILURE);
        }
    #endif

    Threads(fin);

    printf("TecnicoFS completed in %0.4f seconds.\n", (end.tv_sec + (end.tv_usec / 1000000.0)) - (begin.tv_sec + (begin.tv_usec / 1000000.0)));
    print_tecnicofs_trees(fout, fs);
    
    if(fclose(fin) == EOF){
        exit(EXIT_FAILURE);
    }
    if(fclose(fout) == EOF){
        exit(EXIT_FAILURE);
    }

    #ifdef MUTEX
        if(pthread_mutex_destroy(&mutexDoneLock) != 0){
            exit(EXIT_FAILURE);
        }
    #elif RWLOCK
        if(pthread_rwlock_destroy(&rwDoneLock) != 0){
            exit(EXIT_FAILURE);
        }
    #endif

    if(pthread_mutex_destroy(&mutexInumberArrayLock) != 0){
        exit(EXIT_FAILURE);
    }
    if(sem_destroy(&prod) == -1){
        exit(EXIT_FAILURE);
    }
    if(sem_destroy(&cons) == -1){
        exit(EXIT_FAILURE);
    }

    free_tecnicofs(fs);
    free(tid);
    exit(EXIT_SUCCESS);
}
