#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define MAXCMDLEN  256
#define MAXPATHLEN 128
#define MAXCMDNUM   16

typedef struct {
    int type;
    char **cmd;
    char *fin;
    char *fout;
} Command;

#define T_EXEC   1
#define T_RDIN   2
#define T_RDOUT  3
#define T_RDBOTH 4

void
panic (char *s)
{
    fprintf(2, "%s", s);
    exit(-1);
}

int
getcmd (char *buf, int nbuf)
{
    char c = 0, *p = buf;
    unsigned char skip = 1;

    fprintf(2, "@ ");
    memset(buf, 0, nbuf);

    while (1) {
        if(!read(0, &c, sizeof(char)))
            break;
        if ((c == ' ' || c == '\t') && (skip))
            continue;
        if (c == '\n') {
            *p = c;
            break;
        }
        if (!c)
            break;
        skip = 0;
        *p++ = c;
        if ((p - buf) >= nbuf)
            panic("nsh: command too long\n");
    }

    if (buf[0] == 0) // EOF
        return -1;
    while ((c = *p) == ' ' || c == '\t' || c == '\n')
        *p-- = 0;
    return 0;
}

void
handle_cd (char *buf) {
    char *p, c;
    char path[MAXPATHLEN], *pp = path;
    p = buf + 3;
    while ((c=*p) == ' ' || c == '\t')
        p++;
    if (!c) {
        fprintf(2, "nsh: cd: missing operand\n");
        return;
    }
    while ((c=*p) != ' ' && c != '\t' && c != 0) {
        *pp++ = *p++;
        if ((pp - path) >= MAXPATHLEN) {
            fprintf(2, "nsh: cd: path too long\n");
            return;
        }
    }
    if (c) {
        while ((c = *++p)) {
            if (c != ' ' && c != '\t') {
                fprintf(2, "nsh: cd: too many arguments\n");
                return;
            }
        }
    }
    if (chdir(path) < 0)
        fprintf(2, "nsh: cd: cannot cd \"%s\"\n", path);
}

int
myfork (void) 
{
    int pid = fork();
    if (pid < 0)
        panic("nsh: failed to fork\n");
    return pid;
}

char *
gettoken (char *bp) {
    char *p = bp, c;

    while ((c = *p) != ' ' && c != '\t' && c != 0)
        p++;
    if (!c)
        return 0;
    *p = 0; p++;
    while ((c = *p) == ' ' || c == '\t') {
        p++;
    }
    return p;
}

void
tokenize (char *raw, char **tokens)
{
    if (!*raw) {
        tokens[0] = 0;
        return;
    }

    int i = 0;
    char *prev = raw, *next;

    while (1) {
        next = gettoken(prev);
    
        tokens[i++] = prev;
        if (next) {
            prev = next;
        }
        else {
            tokens[i] = 0;
            break;
        }
    }
}

void
parsecmd (char **tokens, Command *command)
{
    int i = 0;
    char **bp, **sp;
    bp = sp = tokens;
    unsigned char new = 1;
    
    while (1) {
        if (!*sp) {
            break;
        }
        
        if (new) {         
            command[i].type = T_EXEC;   
            command[i].cmd = bp;
            new = 0;
        }

        if (**sp == '<') {
            //if ((command[i].type != T_EXEC) || new)
            //    panic("nsh: syntax error near unexpected token \"<\"\n");
            command[i].type = T_RDIN;
            command[i].fin = *(sp+1);
            *sp = 0;
            if (!strcmp(*(sp+2) ,">")) {
                command[i].type = T_RDBOTH;
                command[i].fout = *(sp+3);
                *(sp+2) = 0;
            }
        }
        else if (**sp == '>') {
            //if ((command[i].type != T_EXEC) || new)
            //    panic("nsh: syntax error near unexpected token \">\"\n");
            command[i].type = T_RDOUT;
            command[i].fout = *(sp+1);
            *sp = 0;
            if (!strcmp(*(sp+2) ,"<")) {
                command[i].type = T_RDBOTH;
                command[i].fin = *(sp+3);
                *(sp+2) = 0;
            }
        }
        else if (**sp == '|') {
            //if (new)
            //    panic("nsh: syntax error near unexpected token \"|\"\n");
            new = 1;
            i++;
            if (i >= MAXCMDNUM)
                panic("nsh: too many commands\n");
            *sp++ = 0;
            bp = sp;
            continue;
        }
        sp++;
    }
}

void
runcmd (Command c)
{
    int fd1, fd2;
    switch (c.type){
    case T_EXEC:
        exec(c.cmd[0], c.cmd);
        break;
    case T_RDIN:
        fd1 = open(c.fin, O_RDONLY);
        if (fd1 < 0) {
            fprintf(2, "nsh: failed to open \"%s\"\n", c.fin);
            exit(-1);
        }
        close(0);
        dup(fd1);
        exec(c.cmd[0], c.cmd);
        break;
    case T_RDOUT:
        fd1 = open(c.fout, O_WRONLY | O_CREATE);
        if (fd1 < 0) {
            fprintf(2, "nsh: failed to open \"%s\"\n", c.fout);
            exit(-1);
        }            
        close(1);
        dup(fd1);
        exec(c.cmd[0], c.cmd);
        break;
    case T_RDBOTH:
        fd1 = open(c.fin, O_RDONLY);
        if (fd1 < 0) {
            fprintf(2, "nsh: failed to open \"%s\"\n", c.fin);
            exit(-1);
        }
        fd2 = open(c.fout, O_WRONLY | O_CREATE);
        if (fd2 < 0) {
            fprintf(2, "nsh: failed to open \"%s\"\n", c.fout);
            exit(-1);
        }
        close(0);
        dup(fd1);
        close(1);
        dup(fd2);
        exec(c.cmd[0], c.cmd);
        break;        
    }
    fprintf(2, "nsh: failed to exec \"%s\"\n", c.cmd[0]);
    exit(-1);
}

void
runcmds (Command *cmds)
{
    if ((cmds->type) == 0) {
        exit(0);
    }
    if (((cmds+1)->type) == 0) {
        runcmd(*cmds);
    }
    int fds[2];
    pipe(fds);
    if (fork()) {
        close(fds[0]);
        close(1);
        dup(fds[1]);
        runcmd(*cmds);
    }
    else {
        close(fds[1]);
        close(0);
        dup(fds[0]);
        runcmds(cmds+1);
    }
}

int
main (int argc, char *argv[])
{
    if (argc > 1)
        panic("Usage: nsh\n");

    char buf[MAXCMDLEN];
    char *tokens[MAXCMDLEN] = {0};
    Command command[MAXCMDNUM] = {0};
    
    while (getcmd(buf, sizeof(buf)) >= 0) {
        if (buf[0] == 'c' && buf[1] == 'd' && \
           (buf[2] == ' ' || buf[2] == '\t' || buf[2] == 0)) {
            handle_cd(buf);
            continue;
        }

        if (myfork() == 0) {
            tokenize(buf, tokens);
            parsecmd(tokens, command);
            runcmds(command);
        }
        wait(0);
    }

    printf("\n");
    exit(0);
}