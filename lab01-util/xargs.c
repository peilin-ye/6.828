#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

char arg[512], *p;
char *args[MAXARG], **ps;

void
finish() {
    *p = 0;
    *ps = malloc(strlen(arg) + 1);
    strcpy(*ps, arg);
    *(*ps + strlen(arg)) = 0;
    ps++;
    if(ps >= (args + MAXARG)) {
        fprintf(2, "xargs: too many arguments\n");
        exit();
    }
}

int
main(int argc, char *argv[]) {
    if(argc < 2) {
        fprintf(2, "Usage: xargs COMMAND...\n");
        exit();
    }
    if((argc - 1) > MAXARG) {
        fprintf(2, "xargs: too many arguments\n");
    }

    for(int i = 0; i < (argc - 1); i++)
        args[i] = argv[i + 1];
    ps = args + (argc - 1);

    p = arg;
    while(1) {
        if(read(0, p, sizeof(char)) == 0) {
            finish();
            *ps = 0;
            break;
        }
        if(*p == '\n') {
            finish();
            p = arg;
            continue;
        }
        p++;
        if(p >= arg + 512) {
            fprintf(2, "xargs: argument too long\n");
            exit();
        }
    }

    if(fork()) {
        wait();
    }
    else {
        exec(argv[1], args);
        exit();
    }
    exit();
}