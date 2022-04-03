#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
    int pid = fork();
    while (1)
    {
        if (!pid) {
            write(1, "Child\n", 6);
            yield();
        } else if (pid > 0) {
            write(1, "Parent\n", 7);
            yield();
        } else {
            write(1, "Error\n", 6);
            yield();
        }
    }
    exit();
};