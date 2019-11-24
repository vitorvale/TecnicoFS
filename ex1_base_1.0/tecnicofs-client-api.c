#include "tecnicofs-client-api.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h> 
#include <sys/un.h> 

int sockfd = -1;

int createSocket(){

	if(sockfd != -1){
		return 0;
	}

	if((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
        return -1;
    }

    return 0;
}

int commandResponse(char* command){
	char command_resp[3];

	write(sockfd, command, strlen(command)+1);
	read(sockfd, command_resp, 3);

	/*if(atoi(command_resp[0]) == 0){
		return 0;
	}
	else{
		int d1 = atoi(command_resp[0]), d2, err;

		if(command_resp[1] != '\0'){
			d2 = atoi(command_resp[1]);
		}
		err = d1*10 + d2;
		return err;
	}*/

	return atoi(command_resp);
}

int numberOfDigits(int n){
	int count = 0;

	while(n != 0){
		n /= 10;
		count++;
	}

	return count;
}

int tfsMount(char* address){
	char mount_resp[4];
	struct sockaddr_un server_addr;

	if(createSocket() == -1){
		return TECNICOFS_ERROR_OTHER;
	}

    memset(&server_addr, 0, sizeof(server_addr));
    
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, address, sizeof(server_addr.sun_path) - 1);

    if((connect(sockfd, (struct sockaddr *) &server_addr,sizeof(struct sockaddr_un))) == -1){
        return TECNICOFS_ERROR_CONNECTION_ERROR;
    }

    read(sockfd, mount_resp, 4);
    if(!strcmp("yes", mount_resp)){
    	return TECNICOFS_ERROR_OPEN_SESSION;
    }

    return 0;
}

int tfsUnmount(){
	char unmount_resp[8];

	if(sockfd == -1){
		return TECNICOFS_ERROR_NO_OPEN_SESSION;
	}

	write(sockfd, "s\0", 2);
	read(sockfd, unmount_resp, 8);

	if(!strcmp("failure", unmount_resp)){
		return TECNICOFS_ERROR_OTHER;
	}

	close(sockfd);

	return 0;
}

int tfsCreate(char *filename, permission ownerPermissions, permission othersPermissions){
	char ownperm, otherperm;
	char command[100];
	char *end = *(&command);

	command[0] = 'c';
	command[1] = ' ';
	end += 2;
	strcpy(end, filename);
	end += strlen(filename);
	*end = ' ';
	end++;

	if(ownerPermissions == READ){
		*end = '1';
	} else if(ownerPermissions == WRITE){
		*end = '2';
	} else if(ownerPermissions == RW){
		*end = '3';
	}
	end++;

	if(othersPermissions == NONE){
		*end = '0';
	} else if(othersPermissions == READ){
		*end = '1';
	} else if(othersPermissions == WRITE){
		*end = '2';
	} else if(othersPermissions == RW){
		*end = '3';
	}
	end++;
	*end = '\0';
	
	return commandResponse(command);
}

int tfsDelete(char* filename){
	char command[100];

	command[0] = 'd';
	command[1] = ' ';
	strcpy(&command[2], filename);
	
	return commandResponse(command);
}

int tfsRename(char *filenameOld, char *filenameNew){
	char command[100];
	char *end = *(&command);

	command[0] = 'r';
	command[1] = ' ';
	end += 2;
	strcpy(end, filenameOld);
	end += strlen(filenameOld);
	*end = ' ';
	end++;
	strcpy(end, filenameNew);

	return commandResponse(command);
}

int tfsOpen(char *filename, permission mode){
	char command[100];
	char *end = *(&command);

	command[0] = 'o';
	command[1] = ' ';
	end += 2;
	strcpy(end, filename);
	end += strlen(filename);
	*end = ' ';
	end++;

	if(mode == READ){
		*end = '1';
	} else if(mode == WRITE){
		*end = '2';
	} else if(mode == RW){
		*end = '3';
	}
	end++; 
	*end = '\0';

	return commandResponse(command);
}

int tfsClose(int fd){
	char sfd[2];
	char command[4] = {'x', ' ', 0, '\0'};

	sprintf(sfd, "%d", fd); 
	command[2] = sfd[0];

	return commandResponse(command);
}

int tfsRead(int fd, char *buffer, int len){
	char sfd[2], slen[numberOfDigits(len)+1], respBuffer[len+2];
	char command[100] ={'l', ' ', 0, ' '};

	sprintf(sfd, "%d", fd);
	sprintf(slen, "%d", len);

	command[2] = sfd[0];
	strcpy(&command[4], slen);

	write(sockfd, command, strlen(command));
	read(sockfd, respBuffer, len+2);

	if(atoi(&respBuffer[0]) == 0){
		strcpy(buffer, &respBuffer[2]);
		return strlen(buffer);
	} 
	else{
		int d1 = atoi(&respBuffer[0]), d2, err;

		if(respBuffer[1] != ' '){
			d2 = atoi(&respBuffer[1]);
		}
		err = d1*10 + d2;
		return err;
	}
}

int tfsWrite(int fd, char *buffer, int len){
	char sfd[2], dataInBuffer[len+1], respBuffer[len+3];
	char command[100] ={'w', ' ', 0, ' '};
	int i;

	sprintf(sfd, "%d", fd);
	for(i = 0; i < len; i++){
		dataInBuffer[i] = buffer[i];
		if(buffer[i] == '\0')
			break;
	}
	dataInBuffer[i] = '\0';

	command[2] = sfd[0];
	strcpy(&command[4], dataInBuffer);

	return commandResponse(command);
}