#include "tecnicofs-api-constants.h"
#include "tecnicofs-client-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s sock_path\n", argv[0]);
        exit(0);
    }
    assert(tfsMount(argv[1]) == 0);
    printf("Test: mount success!\n");
    assert(tfsCreate("a", RW, READ) == 0);
    printf("Test: create file sucess\n");
    assert(tfsCreate("a", RW, READ) == TECNICOFS_ERROR_FILE_ALREADY_EXISTS);
    printf("Test: create file with name that already exists\n");
    assert(tfsUnmount() == 0);
    printf("Test: unmount success!\n");

    return 0;
}
