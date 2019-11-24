/* Projeto SO - 
    grupo 1 : Vitor Vale  e Tomas Saraiva */
   
/* proteger tabelaSessoes, numThreads, numSessoes com mutex */

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
#include <signal.h>
#include "fs.h"
#include "lib/hash.h"
#include "tecnicofs-api-constants.h"

#define _GNU_SOURCE
#define _POSIX_SOURCE
#define MAX_COMMANDS 10
#define MAX_INPUT_SIZE 100
#define TABELA_FA_SIZE 5
#define BUFF_SIZE 1024
#define FD_NULL -1
#define UID_NULL -1
#define SUCCESS 0


long numberBuckets = 0;
int tidTableSize = 0, tabSessoesSize = 0, numSessoes = 0, 
numThreads = 0, sockfd, signalActivated = 0;
pthread_t* tid;
struct timeval begin, end;
tecnicofs* fs;
pthread_mutex_t mutexInumberLock;
char *socketName;
int *tabSessoes;

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
    socketName = *(&argv[1]);

}

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}

/* falta fazer um comando para terminar uma sessao */
void applyCommands(char* command, char **tabFichAbertos, int cli){
        char token;
        char arg1[MAX_INPUT_SIZE];
        char arg2[MAX_INPUT_SIZE];
        /* verificacao do token */
        int numTokens = sscanf(command, "%c", &token);

        /* tratamento do resto do comando em funcao do token,
        subtraimos 1 valor para nao contar o token que ja foi lido */
        if(token == 'd' || token == 'x'){
            numTokens += sscanf(command, "%c %s", &token, arg1) - 1; 
        }
        else{
            numTokens += sscanf(command, "%c %s %s", &token, arg1, arg2) - 1;
        }

        if ((numTokens != 2) && (token == 'd' || token == 'x')) {
            errorParse();
        }
        else if((numTokens != 3) && (token != 'd' && token != 'x')){
            errorParse();
        }
        

        int searchResult;
        int iNumber;
        switch (token) {

            case 'c':
                {
                if(pthread_mutex_lock(&mutexInumberLock))
                    exit(EXIT_FAILURE);    
                iNumber = obtainNewInumber(fs);
                if(pthread_mutex_unlock(&mutexInumberLock))
                    exit(EXIT_FAILURE);  
                create(fs, arg1, iNumber);
                }
                break;
            case 'l':
                {
                searchResult = lookup(fs, arg1);
                if(!searchResult)
                    printf( "%s not found\n", arg1);
                else
                    printf("%s found with inumber %d\n", arg1, searchResult);
                }    
                break;
            case 'd':
                {
                delete(fs, arg1);
                }
                break;
            case 'r':
                {
                renameFile(fs, arg1, arg2);   
                }
                break;
            case 'o':
                {
                int i = 0;
                searchResult = lookup(fs, arg1);
                if(!searchResult)
                    write(cli, (char *) TECNICOFS_ERROR_FILE_NOT_FOUND, 2);
                else{
                    while (tabFichAbertos[i] != NULL) i++;
                    strcpy(tabFichAbertos[i], arg1);
                    write(cli,(char *) i, 2);
                }
                }
                break;
            case 'x':
                {
                int i = 0;
                if (tabFichAbertos[atoi(arg1)] != NULL){
                    tabFichAbertos[atoi(arg1)] = NULL;
                    write(cli, (char *) SUCCESS, 2) ;
                }
                else{
                    write(cli, (char *) TECNICOFS_ERROR_FILE_NOT_OPEN, 2);
                }      
                }
                break;
            case 's':
                {
                terminateSession(cli);
                if(close(cli) == -1){
                    fprintf(stderr, "Error: Failed to close the client socket. \n");
                    exit(EXIT_FAILURE);
                }
                numThreads--;
                exit(0);
                }
                break;                
            default: { /* error */
                fprintf(stderr, "Error: command to apply\n");
                exit(EXIT_FAILURE);
            }
        }
}

void terminateSession(int cli){
    int sessionExistsFlag = 0, ulen, i; 
    struct ucred ucred;

    ulen = sizeof(struct ucred);
    if (getsockopt(cli, SOL_SOCKET, SO_PEERCRED, &ucred, &ulen) == -1){
        fprintf(stderr, "Error: Failed to get client credentials. \n");
        exit(EXIT_FAILURE);
    }
        
    for(i = 0; i < tabSessoesSize; i++){
        if(ucred.uid == tabSessoes[i]){
            sessionExistsFlag++;
            break;
        }
    }
    if(sessionExistsFlag == 0){
        write(cli, (char *) TECNICOFS_ERROR_NO_OPEN_SESSION, 2);
    }
    else{
        tabSessoes[i] = UID_NULL;
        numSessoes--;
        write(cli, (char *) SUCCESS, 2);
    }
}

void criaThread(int cli){
    
    if (numThreads = tidTableSize){
        tid = (pthread_t *) realloc(tid, sizeof(pthread_t)*(++tidTableSize));
    }
    
    if(pthread_create(&tid[numThreads++], 0, trataCliente, (void *) cli) != 0){
        exit(EXIT_FAILURE);
    }
}

void *trataCliente(void *arg){
    int *cli = arg;
    int clifd = *(cli);
    char buff[BUFF_SIZE];
    char *tabFichAbertos[TABELA_FA_SIZE];
    memset(buff, 0, sizeof(char));
    memset(buff, 0, sizeof(char));

    while(1){
        read(clifd, buff, BUFF_SIZE);
        applyCommands(buff, tabFichAbertos, clifd);
    }
}



int main(int argc, char* argv[]) {
    time_t t;
    struct sockaddr_un server_addr, cli_addr;
    int cli, clilen, ulen, i;
    long uid;
    struct ucred ucred;
    sigset_t signal_mask;
    FILE *fout;


    /* gera uma seed baseada no tempo
    do sistema para usar na funcao rand() */
    srand((unsigned) time(&t));
    
    parseArgs(argc, argv);

    fs = new_tecnicofs(numberBuckets);
    fout = open(argv[2], 'w');
    if (fout == -1){
        fprintf(stderr, "Error: Failed to get client credentials. \n");
        exit(EXIT_FAILURE);
    }

    tid = (pthread_t *) malloc(sizeof(pthread_t));
    tabSessoes = (int *) malloc(sizeof(int));

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

    sigemptyset (&signal_mask);
    sigaddset(&signal_mask, SIGINT);
    if(pthread_sigmask(SIG_BLOCK, &signal_mask, NULL) != 0){
        fprintf(stderr, "Error: Failed to execute function pthread_sigmask. \n");
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, rotinaTratamentoSignal);
    gettimeofday(&begin, NULL);

    while(signalActivated == 0) {
        int sessionAlreadyExistsFlag = 0;
        cli = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (cli == -1){
            fprintf(stderr, "Error: Failed to accept connection. \n");
            exit(EXIT_FAILURE);
        }
        ulen = sizeof(struct ucred);
        if (getsockopt(cli, SOL_SOCKET, SO_PEERCRED, &ucred, &ulen) == -1){
            fprintf(stderr, "Error: Failed to get client credentials. \n");
            exit(EXIT_FAILURE);
        }
        for(i = 0; i < tabSessoesSize; i++){
            if(ucred.uid == tabSessoes[i]){
                sessionAlreadyExistsFlag++;
                break;
            }
        }
        if(sessionAlreadyExistsFlag == 0){
            if(numSessoes == tabSessoesSize)
                tabSessoes = (int *) realloc(tabSessoes, sizeof(int*)*(++tabSessoesSize));
            tabSessoes[numSessoes++] = ucred.uid;
            write(cli, "no\0", 3);
        }
        else{
            write(cli, "yes\0", 4);
        }

        criaThread(cli);
    }

    for (i = 0; i < (numThreads - 1); i++){
        pthread_join(&tid[i], NULL);
    }

    gettimeofday(&end, NULL);

    printf("TecnicoFS completed in %0.4f seconds.\n", (end.tv_sec + (end.tv_usec / 1000000.0)) - (begin.tv_sec + (begin.tv_usec / 1000000.0)));
    print_tecnicofs_trees(fout, fs);

    if(close(fout) == -1){
        fprintf(stderr, "Error: Failed to get client credentials. \n");
        exit(EXIT_FAILURE);
    }

    unlink(socketName);
    if(close(sockfd) == -1){
        fprintf(stderr, "Error: Failed to get client credentials. \n");
        exit(EXIT_FAILURE);
    }

    free(tid);
    free_tecnicofs(fs);
    free(tabSessoes);

    exit(EXIT_SUCCESS);
}

void rotinaTratamentoSignal(){
    signalActivated++;
    signal(SIGINT, rotinaTratamentoSignal);
}