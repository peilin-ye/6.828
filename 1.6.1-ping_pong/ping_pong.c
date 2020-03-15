//ping-pong.c

#include<sys/types.h>
#include<sys/wait.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<time.h>

int main() {
    int p1[2], p2[2];
    pipe(p1);
    pipe(p2);

    if (fork() != 0) {
        //parent
        close(p1[0]);
        close(p2[1]);
        char ping[1] = "B";

        clock_t start, end;
        double rtt;
	

        start = clock();
        for (int i=0; i<100; i++) {
            write(p1[1], ping, 1);
            read(p2[0], ping, 1);
        }
        end = clock();

        wait(0);
        close(p1[1]);
        close(p2[0]);

        rtt = (((double)(end - start)) / CLOCKS_PER_SEC) / 100;
        printf("average RTT: %f ms\n", (rtt * 1000));
	    printf("exchanges per second: %ld times\n", (unsigned long)(1 / rtt));
    }
    else {
        //child
        close(p1[1]);
        close(p2[0]);
        char pong[1];

        for (int j=0; j<100; j++) {
            read(p1[0], pong, 1);
            write(p2[1], pong, 1);
        }

        close(p1[0]);
        close(p2[1]);
        exit(0);
    }

    return 0;
}
