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
#include <sys/socket.h>
#include <sys/un.h>
#include "fs.h"
#include "lib/hash.h"

#define MAX_COMMANDS 10
#define MAX_INPUT_SIZE 100

long numberBuckets = 0;
int tidTableSize = 0;
int done = 0; 
pthread_t* tid;
struct timeval begin, end;
tecnicofs* fs;
pthread_mutex_t mutexInumberArrayLock;
pthread_mutex_t mutexDoneLock;
pthread_rwlock_t rwDoneLock;
char *socketName;

static void displayUsage (const char* appName){
    printf("Usage: %s\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs (long argc, char* const argv[]){
    if (argc != 5) {
        fprintf(stderr, "Invalid format:\n");
        displayUsage(argv[0]);
    }
    numberBuckets = strtol(argv[3], NULL, 10);
    strcpy(socketName, argv[1]);

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

void criaThread(int cli){
    int fileDescTable[5];
    tid = (pthread_t *) realloc(tid, sizeof(pthread_t)*(++tidTableSize));

    if(pthread_create(&tid[tidTableSize - 1], 0, NULL /*funcao para tratar do cliente*/, (void *) cli) != 0){
        exit(EXIT_FAILURE);
    }
}



int main(int argc, char* argv[]) {
    time_t t;
    struct sockaddr_un server_addr;
    int sockfd;
    int cli;
    struct sockaddr_un server_addr;


    /* gera uma seed baseada no tempo
    do sistema para usar na funcao rand() */
    srand((unsigned) time(&t));
    
    parseArgs(argc, argv);

    fs = new_tecnicofs(numberBuckets);

    if((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
        fprintf(stderr, "Error: Failed to create the socket. \n");
        exit(EXIT_FAILURE);
    }

    unlink(socketName);
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sun_family = AF_UNIX; 
    strncpy(server_addr.sun_path, socketName, sizeof(server_addr.sun_path) - 1);

    if((bind(sockfd, (const struct sockaddr*) &server_addr, sizeof(server_addr))) == -1){
        fprintf(stderr, "Error: Failed to bind the socket. \n");
        exit(EXIT_FAILURE);
    }

    if((listen(sockfd, 10)) == -1){
        fprintf(stderr, "Error: Failed to listen to socket. \n");
        exit(EXIT_FAILURE);
    }

    gettimeofday(&begin, NULL);

    char buf[14];
    while(1) {
        cli = accept(sockfd, NULL, NULL);
        /* chamar funcao para criar thread e criar tabela de
        ficheiros abertos com descriptor do cliente */
        read(cli, buf, 14);
        puts(buf);
    }

    printf("TecnicoFS completed in %0.4f seconds.\n", (end.tv_sec + (end.tv_usec / 1000000.0)) - (begin.tv_sec + (begin.tv_usec / 1000000.0)));
    print_tecnicofs_trees(fout, fs);
    

    free_tecnicofs(fs);
    exit(EXIT_SUCCESS);
}
