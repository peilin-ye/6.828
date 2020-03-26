#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char* argv[])
{
    if(argc != 1){
        fprintf(2, "Usage: uptime\n");
        exit();
    }

    printf("%d\n", uptime());
    exit();
}
