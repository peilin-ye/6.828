#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if(argc != 1){
        fprintf(2, "Usage: %s\n", argv[0]);
        exit();
    }

    int parent_fd[2], child_fd[2];
    pipe(parent_fd);
    pipe(child_fd);

    char ball[1] = "O";

    if(fork() != 0){
        close(parent_fd[0]);
        close(child_fd[1]);
        
        write(parent_fd[1], ball, sizeof(ball));
        if(read(child_fd[0], ball, sizeof(ball)) == sizeof(ball)){
            printf("%d: received pong\n", getpid());
        }
        wait();
        exit();
    } else {
        close(parent_fd[1]);
        close(child_fd[0]);
        
        if(read(parent_fd[0], ball, sizeof(ball)) == sizeof(ball)){
            printf("%d: received ping\n", getpid());
            write(child_fd[1], ball, sizeof(ball));
        }
        exit();
    }
}