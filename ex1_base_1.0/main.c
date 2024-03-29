/* Projeto SO - 
    grupo 1 : Vitor Vale  e Tomas Saraiva */
   
/* ver retorno dos writes e reads (int/string) */
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
#define PID_NULL  0xFFFFFFFF
#define CLOSE_SESSION_CONSTANT -1
#define BUFF_RESP_SIZE 3

long numberBuckets = 0;
int tidTableSize = 0, tabSessoesSize = 0, numSessoes = 0, 
numThreads = 0, sockfd, signalActivated = 0;
pthread_t* tid;
struct timeval begin, end;
tecnicofs* fs;
u_int32_t *tabSessoes;
sem_t clientfd; //indica se a tarefa escrava ja guardou o fd do socket do cliente
pthread_rwlock_t tabSessoesLock;
pthread_mutex_t numThreadsLock;
pthread_cond_t exitCond;    //verifica se todas as tarefas terminaram apos o signal

void clean_exit(int errorCode) {
	// free all resources
    if(close(sockfd) == -1){
        fprintf(stderr, "Error: Failed to close sockfd. \n");
    }

    if(sem_destroy(&clientfd) == -1){
        fprintf(stderr, "Error: Failed to close file. \n");
    }

    if (pthread_rwlock_destroy(&tabSessoesLock) != 0){
        fprintf(stderr, "Error: Failed to destroy rwlock. \n");
    }

    if (pthread_mutex_destroy(&numThreadsLock) != 0){
        fprintf(stderr, "Error: Failed to destroy mutex. \n");
    }

    if(pthread_cond_destroy(&exitCond) != 0){
        fprintf(stderr, "Error: Failed to destroy cond. \n");
    }

    free(tid);
    free_tecnicofs(fs);
    free(tabSessoes);
	
	exit(errorCode);
}

static void displayUsage (const char* appName){
    printf("Usage: %s\n", appName);
    clean_exit(EXIT_FAILURE);
}

static void parseArgs (long argc, char* const argv[]){
    if (argc != 4) {
        fprintf(stderr, "Invalid format:\n");
        displayUsage(argv[0]);
    }
    numberBuckets = strtol(argv[3], NULL, 10);
}

int numberOfDigits(int n){
    int count = 0;

    while(n != 0){
        n /= 10;
        count++;
    }

    return count;
}

//liberta tabela de ficheiros abertos e termina a sessao caso exista
int terminateSession(u_int32_t pid, openfileLink *tabFichAbertos){
    int sessionExistsFlag = 0, i; 

    if (pthread_rwlock_rdlock(&tabSessoesLock) != 0){
        clean_exit(EXIT_FAILURE);
    }    
    for(i = 0; i < tabSessoesSize; i++){
        if(pid == tabSessoes[i]){
            sessionExistsFlag++;
            break;
        }
    }
    if (pthread_rwlock_unlock(&tabSessoesLock) != 0){
        clean_exit(EXIT_FAILURE);
    }

    if(sessionExistsFlag == 0){
    	return TECNICOFS_ERROR_NO_OPEN_SESSION;
    }
    else{
        for(int k = 0; k < TABELA_FA_SIZE; k++){
            if(tabFichAbertos[k] != NULL){
                free(tabFichAbertos[k]->filename);
                free(tabFichAbertos[k]);
            }
        }

        if (pthread_rwlock_wrlock(&tabSessoesLock) != 0){
            clean_exit(EXIT_FAILURE);
        }
        tabSessoes[i] = (u_int32_t)PID_NULL;
        numSessoes--;
        if (pthread_rwlock_unlock(&tabSessoesLock) != 0){
            clean_exit(EXIT_FAILURE);
        }
        return SUCCESS;
    }
}

void closeClientConnection(int cli){
    if(close(cli) == -1){
        fprintf(stderr, "Error: Failed to close the client socket. \n");
        clean_exit(EXIT_FAILURE);
    }
    if (pthread_mutex_lock(&numThreadsLock) != 0){
        clean_exit(EXIT_FAILURE);
    }
    numThreads--;
    if (pthread_mutex_unlock(&numThreadsLock) != 0){
        clean_exit(EXIT_FAILURE);
    }

}

void terminateClientThread(struct ucred ucred, int cli, openfileLink* tabFichAbertos){
    terminateSession(ucred.pid, tabFichAbertos);
    pthread_detach(pthread_self()); //previne que a tarefa fique num estado 'zombie' a espera do join
    closeClientConnection(cli);
    pthread_cond_signal(&exitCond);
    pthread_exit(NULL);
}

void errorParse(struct ucred ucred, int cli, openfileLink* tabFichAbertos){
    fprintf(stderr, "Error: command invalid\n");
    terminateClientThread(ucred, cli, tabFichAbertos);
}

void applyCommand(struct ucred ucred, char* command, openfileLink *tabFichAbertos, int cli){
    char token = '\0';
    char arg1[MAX_INPUT_SIZE];
    char arg2[MAX_INPUT_SIZE];
    char buffer[BUFF_RESP_SIZE];
    memset(arg1, '\0', sizeof(char) * MAX_INPUT_SIZE);
    memset(arg2, '\0', sizeof(char) * MAX_INPUT_SIZE);
    memset(buffer, '\0', sizeof(char) * BUFF_RESP_SIZE);

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
    if ((token == 'd' || token == 'x') &&  (numTokens != 2)) {
        errorParse(ucred, cli, tabFichAbertos);
    }
    else if((token != 'd' && token != 'x' && token != 's') && (numTokens != 3)){
        errorParse(ucred, cli, tabFichAbertos);
    }
    else if((token == 's') && (numTokens != 1)){
        errorParse(ucred, cli, tabFichAbertos);
    }

    switch (token) {
        case 'c':
        { 
            int cres = create(fs, arg1, arg2, ucred.uid);               		
            sprintf(buffer, "%d", cres);
            if(write(cli, buffer, BUFF_RESP_SIZE) == -1) {
                terminateClientThread(ucred, cli, tabFichAbertos);        
            }
        }
        break;
        case 'l':
        {
            char contentBuffer[atoi(arg2)];
            int rdRes = readFromFile(fs, tabFichAbertos, atoi(arg1), contentBuffer, atoi(arg2));

            if (rdRes >= 0){    //separa o caso de sucesso do caso de erro
                char respBuffer[numberOfDigits(rdRes) + atoi(arg2) + 1];
                memset(respBuffer, '\0', sizeof(char) * (numberOfDigits(rdRes) + atoi(arg2) + 1));
                sprintf(respBuffer, "%d", rdRes);
                respBuffer[numberOfDigits(rdRes)] = ' ';
                strcpy(&respBuffer[numberOfDigits(rdRes) + 1], contentBuffer);
                if(write(cli, respBuffer, numberOfDigits(rdRes) + atoi(arg2) + 1) == -1){
                    terminateClientThread(ucred, cli, tabFichAbertos);          
                }
            }
            else{
                char respBuffer[1 + numberOfDigits(rdRes) + 1];
                sprintf(respBuffer, "%d", rdRes);
                if(write(cli, respBuffer, numberOfDigits(rdRes) + 2) == -1){
                    terminateClientThread(ucred, cli, tabFichAbertos); 
                }
            }    
        }    
        break;
        case 'd':
        {
            int dres = delete(fs, arg1, ucred.uid, tabFichAbertos);
            sprintf(buffer, "%d", dres);
            if(write(cli, buffer, BUFF_RESP_SIZE) == -1) {
                terminateClientThread(ucred, cli, tabFichAbertos);        
            }       
        }
        break;
        case 'r':
        {
            int renamRes = renameFile(fs, arg1, arg2, ucred.uid, tabFichAbertos);
            sprintf(buffer, "%d", renamRes);
            if(write(cli, buffer, BUFF_RESP_SIZE) == -1) {
                terminateClientThread(ucred, cli, tabFichAbertos);        
            }       
        }
        break;
        case 'o':
        {
            int ores = openFile(fs, tabFichAbertos, arg1, atoi(arg2), ucred.uid);
            sprintf(buffer, "%d", ores);
            if(write(cli, buffer, BUFF_RESP_SIZE) == -1) {
                terminateClientThread(ucred, cli, tabFichAbertos);        
            }
        }
        break;
        case 'x':
        {
            int clsres = closeFile(fs, tabFichAbertos, atoi(arg1));
            sprintf(buffer, "%d", clsres);
            if(write(cli, buffer, BUFF_RESP_SIZE) == -1){
                terminateClientThread(ucred, cli, tabFichAbertos);        
            }
        }      
        break;
        case 'w':
        {
            int wres = writeToFile(fs, tabFichAbertos, atoi(arg1), arg2);
            sprintf(buffer, "%d", wres);
            if(write(cli, buffer, BUFF_RESP_SIZE) == -1){
                terminateClientThread(ucred, cli, tabFichAbertos);        
            }

        }
        break;    
        case 's':
        {
            int tres = terminateSession(ucred.pid, tabFichAbertos);
            sprintf(buffer, "%d", tres);
            write(cli, buffer, BUFF_RESP_SIZE);     //neste caso nao e necessario testar retorno do write
            pthread_detach(pthread_self());         //porque vai fechar a coneccao de qualquer forma
            closeClientConnection(cli);
            pthread_cond_signal(&exitCond);
            pthread_exit(NULL);
        }
        break;                
        default: { /* error */
            terminateClientThread(ucred, cli, tabFichAbertos);
        }
    }
}

//funcao excutada pela tarefa escrava para comunicar com o cliente
void *trataCliente(void *arg){
    int *cli = (int*) arg;
    int clifd;
    char buff[BUFF_SIZE];
    struct ucred ucred;
    int ulen;
    openfileLink tabFichAbertos[TABELA_FA_SIZE] = {NULL};
    sigset_t signal_mask;

    
    if(sigemptyset(&signal_mask) == -1){
        fprintf(stderr, "Error: sigemptyset. \n");
        clean_exit(EXIT_FAILURE);
    }

    if(sigaddset(&signal_mask, SIGINT) == -1){
        fprintf(stderr, "Error: sigaddset. \n");
        clean_exit(EXIT_FAILURE);
    }

    if(pthread_sigmask(SIG_BLOCK, &signal_mask, NULL) != 0){    //bloqueia o signal SIGINT para a terefa escrava
        fprintf(stderr, "Error: pthread_sigmask. \n");
        clean_exit(EXIT_FAILURE);
    }
    
    clifd = *cli;
    if (sem_post(&clientfd) == -1){ //avisa que ja guardou o fd do socket do cliente
        clean_exit(EXIT_FAILURE);
    }
    
    ulen = sizeof(struct ucred);
    if (getsockopt(clifd, SOL_SOCKET, SO_PEERCRED, &ucred, (socklen_t *) &ulen) == -1){
        fprintf(stderr, "Error: Failed to get client credentials. \n");
        clean_exit(EXIT_FAILURE);
    }

    while(1){
        memset(buff, '\0', sizeof(char) * BUFF_SIZE);
        if(read(clifd, buff, BUFF_SIZE) == -1){
            terminateClientThread(ucred, clifd, tabFichAbertos);
        }
        applyCommand(ucred, buff, tabFichAbertos, clifd);  
    }
    return NULL;
}

void criaThread(int cli){
    
    if(pthread_mutex_lock(&numThreadsLock) != 0){
        clean_exit(EXIT_FAILURE);
    }

    tid = (pthread_t *) realloc(tid, sizeof(pthread_t)*(++tidTableSize));
    if(tid == NULL){
        clean_exit(EXIT_FAILURE);
    }
    
    if(pthread_create(&tid[tidTableSize - 1], 0, trataCliente, (void *) &cli) != 0){
        clean_exit(EXIT_FAILURE);
    }
    numThreads++;
    if (pthread_mutex_unlock(&numThreadsLock) != 0){
        clean_exit(EXIT_FAILURE);
    }
    if(sem_wait(&clientfd) == -1){      //espera que a tarefa escrava guarde o fd do socket do cliente
        clean_exit(EXIT_FAILURE);
    }
}

void rotinaTratamentoSignal(){
    signalActivated++;
}

int main(int argc, char* argv[]) {
    time_t t;
    struct sockaddr_un server_addr = { .sun_family = AF_UNIX };
    int ulen, i;
    struct ucred ucred;
    FILE *fout = NULL;
    struct sigaction terminateAction = {0};
    char socketName[SOCK_SIZE];

    memset(socketName, '\0', sizeof(char) * SOCK_SIZE);

    /* gera uma seed baseada no tempo
    do sistema para usar na funcao rand() */
    srand((unsigned) time(&t));
    
    parseArgs(argc, argv);

    strcpy(socketName, argv[1]);

    if(sem_init(&clientfd, 0, 0) == -1){
    	fprintf(stderr ,"sem init\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_rwlock_init(&tabSessoesLock, NULL) != 0){
        fprintf( stderr ,"sessoes lock init\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_init(&numThreadsLock, NULL) != 0){
        fprintf(stderr,"threads lock init\n");
        exit(EXIT_FAILURE);
    }

    if(pthread_cond_init(&exitCond, NULL) != 0){
        exit(EXIT_FAILURE);
    }

    fs = new_tecnicofs(numberBuckets);
    fout = fopen(argv[2], "w");
    if (fout == NULL){
        fprintf(stderr, "Error: Failed to open file. \n");
        exit(EXIT_FAILURE);
    }

    tid = (pthread_t *) malloc(sizeof(pthread_t));
    tidTableSize++;
    if (!tid){
        perror("failed to allocate tid\n");
		exit(EXIT_FAILURE);
    }

	if (pthread_rwlock_wrlock(&tabSessoesLock) != 0){
        exit(EXIT_FAILURE);
    }
    tabSessoesSize = 1;
    tabSessoes = (u_int32_t *) malloc(sizeof(u_int32_t) * tabSessoesSize);   //guarda os PID's dos clientes com 
    if (!tabSessoes){                                       //sessao ativa
        perror("failed to allocate tabSessoes\n");
		exit(EXIT_FAILURE);
    }
	memset(tabSessoes, PID_NULL, sizeof(u_int32_t)*tabSessoesSize);
	if (pthread_rwlock_unlock(&tabSessoesLock) != 0){
        exit(EXIT_FAILURE);
    }

    if((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
        fprintf(stderr, "Error: Failed to create the socket. \n");
        exit(EXIT_FAILURE);
    }

    unlink(socketName);     //ignorar retorno pois pode nao existir socket com este nome

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

	memset(&terminateAction, 0, sizeof(terminateAction));
    if(sigemptyset(&terminateAction.sa_mask) == -1){
        fprintf(stderr, "Error: sigemptyset. \n");
        exit(EXIT_FAILURE);
    }

    terminateAction.sa_flags = 0;
    terminateAction.sa_handler = rotinaTratamentoSignal;

    if(sigaction(SIGINT, &terminateAction, NULL) == -1){    //associa rotina de tratamento ao signal
        fprintf(stderr, "Error: sigaction. \n");
        exit(EXIT_FAILURE);
    }

    gettimeofday(&begin, NULL);

    while(signalActivated == 0) {
        int sessionAlreadyExistsFlag = 0;
		int cli = -1, k = 0;
        char buffer[BUFF_RESP_SIZE];
        memset(buffer, '\0', sizeof(char) * BUFF_RESP_SIZE);
        
        cli = accept(sockfd, NULL, NULL);
        if (cli == -1){
            if (signalActivated == 0){  //test if accept error was due to signal activation
                fprintf(stderr, "Error: Failed to accept connection. \n");
                clean_exit(EXIT_FAILURE);
            }
            else{
                continue;
            }
        }
        ulen = sizeof(struct ucred);
        if (getsockopt(cli, SOL_SOCKET, SO_PEERCRED, &ucred, (socklen_t *) &ulen) == -1){
            fprintf(stderr, "Error: Failed to get client credentials. \n");
            clean_exit(EXIT_FAILURE);
        }


        if (pthread_rwlock_rdlock(&tabSessoesLock) != 0){
            clean_exit(EXIT_FAILURE);
        }
        for(i = 0; i < tabSessoesSize; i++){
            if((u_int32_t)ucred.pid == (u_int32_t) tabSessoes[i]){
                sessionAlreadyExistsFlag++;
                break;
            }
        }
        if (pthread_rwlock_unlock(&tabSessoesLock) != 0){
            clean_exit(EXIT_FAILURE);
        }

        if(sessionAlreadyExistsFlag == 0){
            if (pthread_rwlock_wrlock(&tabSessoesLock) != 0){
                clean_exit(EXIT_FAILURE);
            }
            if(numSessoes == tabSessoesSize){
                tabSessoes = (u_int32_t *) realloc(tabSessoes, sizeof(u_int32_t)*(++tabSessoesSize));
                tabSessoes[tabSessoesSize - 1] = PID_NULL;
            }        
            for(k = 0; k < tabSessoesSize; k++){
                if(tabSessoes[k] == (u_int32_t) PID_NULL){
                    break;
                }
            }
            tabSessoes[k] = ucred.pid;
            numSessoes++;
            sprintf(buffer, "%d", SUCCESS);
            if(write(cli, buffer, BUFF_RESP_SIZE) == -1){
                if(close(cli) == -1){
                    clean_exit(EXIT_FAILURE);
                }
            }
            if (pthread_rwlock_unlock(&tabSessoesLock) != 0){
                clean_exit(EXIT_FAILURE);
            }

			criaThread(cli);
        }
        else{
            sprintf(buffer, "%d", TECNICOFS_ERROR_OPEN_SESSION);
            if(write(cli, buffer, BUFF_RESP_SIZE) == -1){
                if(close(cli) == -1){
                    clean_exit(EXIT_FAILURE);
                }       
            }
        }
    }

    if(pthread_mutex_lock(&numThreadsLock) != 0){
        clean_exit(EXIT_FAILURE);
    }
    while (numThreads != 0){
        pthread_cond_wait(&exitCond, &numThreadsLock);  //espera que todas as tarefas escravas terminem
    }                                                   //apos receber o signal
    if(pthread_mutex_unlock(&numThreadsLock) != 0){
        clean_exit(EXIT_FAILURE);
    }

    gettimeofday(&end, NULL);

    printf("TecnicoFS completed in %0.4f seconds.\n", (end.tv_sec + (end.tv_usec / 1000000.0)) - (begin.tv_sec + (begin.tv_usec / 1000000.0)));
    print_tecnicofs_trees(fout, fs);

	if(fclose(fout) == EOF){
        fprintf(stderr, "Error: Failed to close file. \n");
    }

    if(unlink(socketName) == -1){
        fprintf(stderr, "Error: Failed to unlink socket. \n");
    }

    clean_exit(EXIT_SUCCESS);
}

