/* Projeto SO - 
    grupo 1 : Vitor Vale  e Tomas Saraiva */
   
/* ver a terminacao apos testes */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include "fs.h"
#include "lib/hash.h"

#define SOCK_SIZE 100
#define MAX_COMMANDS 10
#define MAX_INPUT_SIZE 100
#define BUFF_SIZE 1024
#define FD_NULL -1
#define UID_NULL -1
#define BUFF_RESP_SIZE 3

long numberBuckets = 0;
int tidTableSize = 0, tabSessoesSize = 0, numSessoes = 0, 
numThreads = 0, sockfd, signalActivated = 0, numThreadsOnSignal, numOpenFiles = 0;
pthread_t* tid;
struct timeval begin, end;
tecnicofs* fs;
int *tabSessoes;
sem_t clientfd;
pthread_rwlock_t tabSessoesLock;
pthread_rwlock_t numThreadsLock;

static void displayUsage (const char* appName){
    printf("Usage: %s\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs (long argc, char* const argv[]){
    if (argc != 4) {
        fprintf(stderr, "Invalid format:\n");
        displayUsage(argv[0]);
    }
    numberBuckets = strtol(argv[3], NULL, 10);
}

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}

int numberOfDigits(int n){
    int count = 0;

    while(n != 0){
        n /= 10;
        count++;
    }

    return count;
}

int terminateSession(int cli, openfileLink *tabFichAbertos){
    int sessionExistsFlag = 0, ulen, i; 
    struct ucred ucred;

    ulen = sizeof(struct ucred);
    if (getsockopt(cli, SOL_SOCKET, SO_PEERCRED, &ucred, (socklen_t *) &ulen) == -1){
        fprintf(stderr, "Error: Failed to get client credentials. \n");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < TABELA_FA_SIZE; i++){
        if(tabFichAbertos[i] != NULL){
            free(tabFichAbertos[i]->filename);
            free(tabFichAbertos[i]);
        }
    }
    free(tabFichAbertos);

    if (pthread_rwlock_rdlock(&tabSessoesLock) != 0){
        exit(EXIT_FAILURE);
    }    
    for(i = 0; i < tabSessoesSize; i++){
        if(ucred.uid == tabSessoes[i]){
            sessionExistsFlag++;
            break;
        }
    }
    if (pthread_rwlock_unlock(&tabSessoesLock) != 0){
        exit(EXIT_FAILURE);
    }

    if(sessionExistsFlag == 0){
    	return TECNICOFS_ERROR_NO_OPEN_SESSION;
    }
    else{
        if (pthread_rwlock_wrlock(&tabSessoesLock) != 0){
            exit(EXIT_FAILURE);
        }
        tabSessoes[i] = UID_NULL;
        numSessoes--;
        if (pthread_rwlock_unlock(&tabSessoesLock) != 0){
            exit(EXIT_FAILURE);
        }
        return SUCCESS;
    }
}

void closeClientConnection(int cli){
    if(close(cli) == -1){
        fprintf(stderr, "Error: Failed to close the client socket. \n");
        exit(EXIT_FAILURE);
    }
    if (pthread_rwlock_wrlock(&numThreadsLock) != 0){
        exit(EXIT_FAILURE);
    }
    numThreads--;
    if (pthread_rwlock_unlock(&numThreadsLock) != 0){
        exit(EXIT_FAILURE);
    }
    exit(0);
}

void applyCommand(uid_t user, char* command, openfileLink *tabFichAbertos, int cli){
    char token;
    char arg1[MAX_INPUT_SIZE];
    char arg2[MAX_INPUT_SIZE];
    char buffer[BUFF_RESP_SIZE];
    /* verificacao do token */
    int numTokens = sscanf(command, "%c", &token);

    /*SE O COMANDO TIVER ARGS A MAIS NAO VERIFICA!!!!!!!!*/

    /* tratamento do resto do comando em funcao do token,
       subtraimos 1 valor para nao contar o token que ja foi lido */
    if(token == 'd' || token == 'x'){
        numTokens += sscanf(command, "%c %s", &token, arg1) - 1; 
    }
    else if(token == 's'){
       	//empty
    }
    else{
        numTokens += sscanf(command, "%c %s %s", &token, arg1, arg2) - 1;
    }
    if ((numTokens != 2) && (token == 'd' || token == 'x')) {
        errorParse();
    }
    else if((numTokens != 3) && (token != 'd' && token != 'x' && token != 's')){
        errorParse();
    }
    else if((numTokens != 1) && (token == 's')){
        errorParse();
    }

    switch (token) {
        case 'c':
        { 
            int cres = create(fs, arg1, arg2, user);               		
            sprintf(buffer, "%d", cres);
            if(write(cli, buffer, BUFF_RESP_SIZE) == -1) {
                terminateSession(cli, tabFichAbertos);
                closeClientConnection(cli);        
            }
        }
        break;
        case 'l':
        {
            char contentBuffer[atoi(arg2)];
            int rdRes = readFromFile(fs, tabFichAbertos, atoi(arg1), contentBuffer, atoi(arg2));

            if (rdRes >= 0){ 
                char respBuffer[numberOfDigits(rdRes) + atoi(arg2) + 1];
                sprintf(respBuffer, "%d", rdRes);
                respBuffer[numberOfDigits(rdRes)] = ' ';
                strcpy(&respBuffer[numberOfDigits(rdRes) + 1], contentBuffer);
                if(write(cli, respBuffer, numberOfDigits(rdRes) + atoi(arg2) + 1) == -1){
                    terminateSession(cli, tabFichAbertos);
                    closeClientConnection(cli);          
                }
            }
            else{
                char respBuffer[1 + numberOfDigits(rdRes) + 1];
                sprintf(respBuffer, "%d", rdRes);
                if(write(cli, respBuffer, numberOfDigits(rdRes) + 2) == -1){
                    terminateSession(cli, tabFichAbertos);
                    closeClientConnection(cli); 
                }
            }    
        }    
        break;
        case 'd':
        {
            int dres = delete(fs, arg1, user, tabFichAbertos);
            sprintf(buffer, "%d", dres);
            if(write(cli, buffer, BUFF_RESP_SIZE) == -1) {
                terminateSession(cli, tabFichAbertos);
                closeClientConnection(cli);        
            }       
        }
        break;
        case 'r':
        {
            int renamRes = renameFile(fs, arg1, arg2, user);
            sprintf(buffer, "%d", renamRes);
            if(write(cli, buffer, BUFF_RESP_SIZE) == -1) {
                terminateSession(cli, tabFichAbertos);
                closeClientConnection(cli);        
            }       
        }
        break;
        case 'o':
        {
            int ores = openFile(fs, tabFichAbertos, arg1, atoi(arg2), user);
            sprintf(buffer, "%d", ores);
            if(write(cli, buffer, BUFF_RESP_SIZE) == -1) {
                terminateSession(cli, tabFichAbertos);
                closeClientConnection(cli);        
            }
        }
        break;
        case 'x':
        {
            int clsres = closeFile(fs, tabFichAbertos, atoi(arg1));
            sprintf(buffer, "%d", clsres);
            if(write(cli, buffer, BUFF_RESP_SIZE) == -1){
                terminateSession(cli, tabFichAbertos);
                closeClientConnection(cli);        
            }
        }      
        break;
        case 'w':
        {
            int wres = writeToFile(fs, tabFichAbertos, atoi(arg1), arg2);
            sprintf(buffer, "%d", wres);
            if(write(cli, buffer, BUFF_RESP_SIZE) == -1){
                terminateSession(cli, tabFichAbertos);
                closeClientConnection(cli);        
            }

        }
        break;    
        case 's':
        {
            int tres = terminateSession(cli, tabFichAbertos);
            sprintf(buffer, "%d", tres);
            write(cli, buffer, BUFF_RESP_SIZE);
            closeClientConnection(cli);
        }
        break;                
        default: { /* error */
            fprintf(stderr, "Error: command to apply\n");
            exit(EXIT_FAILURE);
        }
    }
}

void *trataCliente(void *arg){
    int *cli = (int*) arg;
    int clifd;
    char buff[BUFF_SIZE];
    struct ucred ucred;
    int ulen;
    openfileLink *tabFichAbertos = (openfileLink*) malloc(sizeof(openfileLink) * TABELA_FA_SIZE);
    
    if(!tabFichAbertos){
        perror("failed to allocate tabFichAbertos\n");
		exit(EXIT_FAILURE);
    }
    memset(buff, 0, sizeof(char));

    clifd = *cli;
    if (sem_post(&clientfd) == -1){
        exit(EXIT_FAILURE);
    }
    
    ulen = sizeof(struct ucred);
    if (getsockopt(clifd, SOL_SOCKET, SO_PEERCRED, &ucred, (socklen_t *) &ulen) == -1){
        fprintf(stderr, "Error: Failed to get client credentials. \n");
        exit(EXIT_FAILURE);
    }

    while(1){
        memset(buff, 0, sizeof(char));
        if(read(clifd, buff, BUFF_SIZE) == -1){
            terminateSession(clifd, tabFichAbertos);
            closeClientConnection(clifd);
        }
        applyCommand(ucred.uid, buff, tabFichAbertos, clifd);
    }
}

void criaThread(int cli){
    
    if(pthread_rwlock_rdlock(&numThreadsLock) != 0){
        exit(EXIT_FAILURE);
    }
    if(numThreads == tidTableSize){
        tid = (pthread_t *) realloc(tid, sizeof(pthread_t)*(++tidTableSize));
    }
    if(pthread_rwlock_unlock(&numThreadsLock) != 0){
        exit(EXIT_FAILURE);
    }
    
    if(pthread_rwlock_wrlock(&numThreadsLock) != 0){
        exit(EXIT_FAILURE);
    }
    if(pthread_create(&tid[numThreads++], 0, trataCliente, (void *) &cli) != 0){
        exit(EXIT_FAILURE);
    }
    if (pthread_rwlock_unlock(&numThreadsLock) != 0){
        exit(EXIT_FAILURE);
    }
    if(sem_wait(&clientfd) == -1){
        exit(EXIT_FAILURE);
    }
}

void rotinaTratamentoSignal(){
    numThreadsOnSignal = numThreads;
    signalActivated++;
}

int main(int argc, char* argv[]) {
    time_t t;
    struct sockaddr_un server_addr;
    int cli, ulen, i;
    struct ucred ucred;
    sigset_t signal_mask;
    FILE *fout;
    struct sigaction terminateAction;
    char socketName[SOCK_SIZE];

    /* gera uma seed baseada no tempo
    do sistema para usar na funcao rand() */
    srand((unsigned) time(&t));
    
    parseArgs(argc, argv);

    strcpy(socketName, argv[1]);

    if(sem_init(&clientfd, 0, 0) == -1){
    	printf("sem init\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_rwlock_init(&tabSessoesLock ,NULL) != 0){
        printf("sessoes lock init\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_rwlock_init(&numThreadsLock ,NULL) != 0){
        printf("threads lock init\n");
        exit(EXIT_FAILURE);
    }

    fs = new_tecnicofs(numberBuckets);
    fout = fopen(argv[2], "w");
    if (fout == NULL){
        fprintf(stderr, "Error: Failed to get client credentials. \n");
        exit(EXIT_FAILURE);
    }

    tid = (pthread_t *) malloc(sizeof(pthread_t));
    if (!tid){
        perror("failed to allocate tid\n");
		exit(EXIT_FAILURE);
    }
    tabSessoes = (int *) malloc(sizeof(int));
    if (!tabSessoes){
        perror("failed to allocate tabSessoes\n");
		exit(EXIT_FAILURE);
    }

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

    if(sigemptyset(&signal_mask) == -1){
        fprintf(stderr, "Error: sigemptyset. \n");
        exit(EXIT_FAILURE);
    }

    if(sigaddset(&signal_mask, SIGINT) == -1){
        fprintf(stderr, "Error: sigaddset. \n");
        exit(EXIT_FAILURE);
    }

    terminateAction.sa_mask = signal_mask;
    terminateAction.sa_flags = 0;
    terminateAction.sa_handler = rotinaTratamentoSignal;

    if(sigaction(SIGINT, &terminateAction, NULL) == -1){
        fprintf(stderr, "Error: sigaction. \n");
        exit(EXIT_FAILURE);
    }

    gettimeofday(&begin, NULL);

    while(signalActivated == 0) {
        int sessionAlreadyExistsFlag = 0;
        char buffer[BUFF_RESP_SIZE];
        
        cli = accept(sockfd, NULL, NULL);
        if (cli == -1){
            if (signalActivated == 0){
                fprintf(stderr, "Error: Failed to accept connection. \n");
                exit(EXIT_FAILURE);
            }
            else{
                continue;
            }
        }
        ulen = sizeof(struct ucred);
        if (getsockopt(cli, SOL_SOCKET, SO_PEERCRED, &ucred, (socklen_t *) &ulen) == -1){
            fprintf(stderr, "Error: Failed to get client credentials. \n");
            exit(EXIT_FAILURE);
        }

        if (pthread_rwlock_rdlock(&tabSessoesLock) != 0){
            exit(EXIT_FAILURE);
        }
        for(i = 0; i < tabSessoesSize; i++){
            if(ucred.uid == tabSessoes[i]){
                sessionAlreadyExistsFlag++;
                break;
            }
        }
        if (pthread_rwlock_unlock(&tabSessoesLock) != 0){
            exit(EXIT_FAILURE);
        }

        if(sessionAlreadyExistsFlag == 0){
            if (pthread_rwlock_wrlock(&tabSessoesLock) != 0){
                exit(EXIT_FAILURE);
            }
            if(numSessoes == tabSessoesSize)
                tabSessoes = (int *) realloc(tabSessoes, sizeof(int*)*(++tabSessoesSize));
            tabSessoes[numSessoes++] = ucred.uid;
            sprintf(buffer, "%d", SUCCESS);
            if(write(cli, buffer, BUFF_RESP_SIZE) == -1)
                closeClientConnection(cli);
            if (pthread_rwlock_unlock(&tabSessoesLock) != 0){
                exit(EXIT_FAILURE);
            }
        }
        else{
            sprintf(buffer, "%d", TECNICOFS_ERROR_OPEN_SESSION);
            if(write(cli, buffer, BUFF_RESP_SIZE) == -1)
                closeClientConnection(cli);       
        }

        criaThread(cli);
    }

    for (i = 0; i < numThreadsOnSignal; i++){
        if(pthread_join(tid[i], NULL) != 0){
            exit(EXIT_FAILURE);
        }
    }

    gettimeofday(&end, NULL);

    printf("TecnicoFS completed in %0.4f seconds.\n", (end.tv_sec + (end.tv_usec / 1000000.0)) - (begin.tv_sec + (begin.tv_usec / 1000000.0)));
    print_tecnicofs_trees(fout, fs);

    if(fclose(fout) == EOF){
        fprintf(stderr, "Error: Failed to get client credentials. \n");
        exit(EXIT_FAILURE);
    }

    if(unlink(socketName) == -1){
        exit(EXIT_FAILURE);
    }
    if(close(sockfd) == -1){
        fprintf(stderr, "Error: Failed to get client credentials. \n");
        exit(EXIT_FAILURE);
    }

    if(sem_destroy(&clientfd) == -1){
    	exit(EXIT_FAILURE);
    }

    if (pthread_rwlock_destroy(&tabSessoesLock) != 0){
        exit(EXIT_FAILURE);
    }

    if (pthread_rwlock_destroy(&numThreadsLock) != 0){
        exit(EXIT_FAILURE);
    }

    free(tid);
    free_tecnicofs(fs);
    free(tabSessoes);

    exit(EXIT_SUCCESS);
}

