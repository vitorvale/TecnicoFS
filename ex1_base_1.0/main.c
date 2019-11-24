/* Projeto SO - 
    grupo 1 : Vitor Vale  e Tomas Saraiva */
   
/* proteger tabelaSessoes, numThreads, numSessoes com mutex */
#define _GNU_SOURCE

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
char *socketName;
int *tabSessoes;
sem_t clientfd;

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
    socketName = *(&argv[1]);

}

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}

void terminateSession(int cli){
    int sessionExistsFlag = 0, ulen, i; 
    struct ucred ucred;
    char buffer[3];

    ulen = sizeof(struct ucred);
    if (getsockopt(cli, SOL_SOCKET, SO_PEERCRED, &ucred, (socklen_t *) &ulen) == -1){
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
    	sprintf(buffer, "%d", TECNICOFS_ERROR_NO_OPEN_SESSION);
        write(cli, buffer, 2);
    }
    else{
        tabSessoes[i] = UID_NULL;
        numSessoes--;
        sprintf(buffer, "%d", SUCCESS);
        write(cli, buffer, 2);
    }
}

/* falta fazer um comando para terminar uma sessao */
void applyCommands(uid_t owner, char* command, char **tabFichAbertos, int cli){
        char token;
        char arg1[MAX_INPUT_SIZE];
        char arg2[MAX_INPUT_SIZE];
        char buffer[3];
        /* verificacao do token */
        int numTokens = sscanf(command, "%c", &token);

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
        
        int searchResult;
        switch (token) {
            case 'c':
                { 
				int cres = create(fs, arg1, arg2, owner);               		
				sprintf(buffer, "%d", cres);
                write(cli, buffer, 3);
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
                if(!searchResult){
                	sprintf(buffer, "%d", TECNICOFS_ERROR_FILE_NOT_FOUND);
                    write(cli, buffer, 3);
                }
                else{
                    while (tabFichAbertos[i] != NULL) i++;
                    strcpy(tabFichAbertos[i], arg1);
                    sprintf(buffer, "%d", i);
                    write(cli, buffer, 3);
                }
                }
                break;
            case 'x':
                {
                if (tabFichAbertos[atoi(arg1)] != NULL){
                    tabFichAbertos[atoi(arg1)] = NULL;
					sprintf(buffer, "%d", SUCCESS);
        			write(cli, buffer, 3);                
        		}
                else{
					sprintf(buffer, "%d", TECNICOFS_ERROR_FILE_NOT_OPEN);
        			write(cli, buffer, 3);                
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

void *trataCliente(void *arg){
    int *cli = (int*) arg;
    int clifd;
    char buff[BUFF_SIZE];
    char *tabFichAbertos[TABELA_FA_SIZE];
    struct ucred ucred;
    int ulen;
    
    memset(buff, 0, sizeof(char));

    clifd = *cli;
    sem_post(&clientfd);
    
    ulen = sizeof(struct ucred);
    if (getsockopt(clifd, SOL_SOCKET, SO_PEERCRED, &ucred, (socklen_t *) &ulen) == -1){
        fprintf(stderr, "Error: Failed to get client credentials. \n");
        exit(EXIT_FAILURE);
    }

    while(1){
        read(clifd, buff, BUFF_SIZE);
        applyCommands(ucred.uid, buff, tabFichAbertos, clifd);
    }
}

void criaThread(int cli){
    
    if (numThreads == tidTableSize){
        tid = (pthread_t *) realloc(tid, sizeof(pthread_t)*(++tidTableSize));
    }
    
    if(pthread_create(&tid[numThreads++], 0, trataCliente, (void *) &cli) != 0){
        exit(EXIT_FAILURE);
    }
    sem_wait(&clientfd);
}

void rotinaTratamentoSignal(){
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


    /* gera uma seed baseada no tempo
    do sistema para usar na funcao rand() */
    srand((unsigned) time(&t));
    
    parseArgs(argc, argv);

    if(sem_init(&clientfd, 0, 0) == -1){
    	exit(EXIT_FAILURE);
    }

    fs = new_tecnicofs(numberBuckets);
    fout = fopen(argv[2], "w");
    if (fout == NULL){
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

    terminateAction.sa_mask = signal_mask;
    terminateAction.sa_flags = 0;
    terminateAction.sa_handler = rotinaTratamentoSignal;

    sigaction(SIGINT, &terminateAction, NULL);

    gettimeofday(&begin, NULL);

    while(signalActivated == 0) {
        int sessionAlreadyExistsFlag = 0;
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
        pthread_join(tid[i], NULL);
    }

    gettimeofday(&end, NULL);

    printf("TecnicoFS completed in %0.4f seconds.\n", (end.tv_sec + (end.tv_usec / 1000000.0)) - (begin.tv_sec + (begin.tv_usec / 1000000.0)));
    print_tecnicofs_trees(fout, fs);

    if(fclose(fout) == EOF){
        fprintf(stderr, "Error: Failed to get client credentials. \n");
        exit(EXIT_FAILURE);
    }

    unlink(socketName);
    if(close(sockfd) == -1){
        fprintf(stderr, "Error: Failed to get client credentials. \n");
        exit(EXIT_FAILURE);
    }

    if(sem_destroy(&clientfd) == -1){
    	exit(EXIT_FAILURE);
    }

    free(tid);
    free_tecnicofs(fs);
    free(tabSessoes);

    exit(EXIT_SUCCESS);
}

