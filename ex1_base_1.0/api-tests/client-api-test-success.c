#include "../tecnicofs-client-api.h"
#include "../tecnicofs-api-constants.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s sock_path\n", argv[0]);
        exit(0);
    }
    
    char readBuffer[4] = {0};

    assert(tfsMount(argv[1]) == 0);

    assert(tfsCreate("abc", RW, READ) == 0 );

    assert(tfsRename("abc", "bcd") == 0);

    int fd = -1;
    assert((fd = tfsOpen("bcd", RW)) == 0);

    assert(tfsWrite(fd, "hmm", 3) == 0);

    assert(tfsRead(fd, readBuffer, 4) == 3);

    puts(readBuffer);

    assert(tfsClose(fd) == 0);

    assert(tfsDelete("bcd") == 0);

    assert(tfsUnmount() == 0);

    return 0;
}