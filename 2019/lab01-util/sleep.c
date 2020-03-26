#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if(argc != 2){
        fprintf(2, "Usage: sleep NUMBER...\n");
        exit();
    }

    int n = atoi(argv[1]);
    
    sleep(n);
    exit();
}