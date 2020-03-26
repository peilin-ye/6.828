#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void
find (char *path, char *name, char *target) {
    char buf[512], *p;

    if ((strlen(path) + 1 + strlen(name) + 1) > sizeof(buf)) {
        fprintf(2, "find: path too long\n");
        return;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    if (strcmp(name, "\0") != 0) {
        *p = '/';
        p++;
        strcpy(p, name);
    }

    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(buf, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", buf);
        return;
    }

    if ((fstat(fd, &st)) < 0) {
        fprintf(2, "find: cannot stat %s\n", buf);
        close(fd);
        return;
    }

    if ((strcmp(name, target)) == 0) {
        printf("%s\n", buf);
    }

    if (st.type == T_DIR) {
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0)
                continue;
            if ((strcmp(de.name, ".") == 0) | (strcmp(de.name, "..") == 0)) {
                continue;
            }
            find(buf, de.name, target);
        }
    }

    close(fd);
    return;
}

int
main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(2, "Usage: %s PATH EXPRESSION\n", argv[0]);
        exit();
    }

    // assuming argv[1] is a directory. "find file1 file1" doesn't work.
    find(argv[1], "\0", argv[2]);
    exit();
}