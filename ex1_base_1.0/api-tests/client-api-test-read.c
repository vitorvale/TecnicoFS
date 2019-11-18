#include "../tecnicofs-api-constants.h"
#include "../tecnicofs-client-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>


int main(int argc, char** argv) {
     if (argc != 2) {
        printf("Usage: %s sock_path\n", argv[0]);
        exit(0);
    }
    char readBuffer[10] = {0};
    assert(tfsMount(argv[1]) == 0);
    assert(tfsCreate("abc", RW, READ) == 0);
    int fd = -1;
    assert((fd = tfsOpen("abc", RW)) == 0);
    assert(tfsWrite(fd, "12345", 5) == 0);
    
    printf("Test: read full file content");
    assert(tfsRead(fd, readBuffer, 6) == 5);
    printf("Content read: %s\n", readBuffer);
    
    printf("Test: read only first 3 characters of file content");
    memset(readBuffer, 0, 10*sizeof(char));
    assert(tfsRead(fd, readBuffer, 4) == 3);
    printf("Content read: %s\n", readBuffer);
    
    printf("Test: read with buffer bigger than file content");
    memset(readBuffer, 0, 10*sizeof(char));
    assert(tfsRead(fd, readBuffer, 10) == 5);
    printf("Content read: %s\n", readBuffer);

    assert(tfsClose(fd) == 0);

    printf("Test: read closed file");
    assert(tfsRead(fd, readBuffer, 6) == TECNICOFS_ERROR_FILE_NOT_OPEN);

    printf("Test: read file open in write mode");
    assert((fd = tfsOpen("abc", WRITE)) == 0);
    assert(tfsRead(fd, readBuffer, 6) == TECNICOFS_ERROR_INVALID_MODE);


    assert(tfsDelete("abc") == 0);
    assert(tfsUnmount() == 0);

    return 0;
}