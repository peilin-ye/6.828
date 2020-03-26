#include "kernel/types.h"
#include "user/user.h"

void
recur (int parent_fd[]) {
    int i, j;
    if (read(parent_fd[0], &i, sizeof(int)) == 0) {
        close(parent_fd[0]);
        exit();    
    }
    printf("primes %d\n", i);
    char flag = 0;
    int child_fd[2];
    while ((read(parent_fd[0], &j, sizeof(int))) != 0) {
        if ((j % i) != 0) {
            if (!flag) {
                flag = 1;
                pipe(child_fd);
                if (fork() != 0) {
                    close(child_fd[0]);
                } else {
                    close(child_fd[1]);
                    recur(child_fd);
                }
            }
            write(child_fd[1], &j, sizeof(int));
        }
    }
    if (flag) {
        close(child_fd[1]);
        wait();
    }
    close(parent_fd[0]);
    exit();
}

int
main (int argc, char *argv[]) {
    if (argc != 1) {
        fprintf(2, "Usage: primes\n");
        exit();
    }

    int parent_fd[2];
    pipe(parent_fd);
    if (fork() == 0) {
        close(parent_fd[1]);
        recur(parent_fd);
    } else {
        close(parent_fd[0]);
        for (int i=2; i<36; i++) {
            write(parent_fd[1], &i, sizeof(int));
        }
        close(parent_fd[1]);
        wait();
    }
    exit();
}
