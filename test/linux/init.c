/* minimal libc-free init (PID 1) with simple shell */

#include <stdint.h>

/* Linux ARM EABI syscall numbers */
#define SYS_write       4
#define SYS_read        3
#define SYS_nanosleep   162
#define SYS_wait4       114
#define SYS_exit        1

#define STDOUT          1
#define STDIN           0
#define CMD_MAX_LEN     64

struct timespec {
    long tv_sec;
    long tv_nsec;
};

/* raw syscall */
static inline long syscall(
    long nr,
    long a,
    long b,
    long c,
    long d,
    long e,
    long f)
{
    register long r0 asm("r0") = a;
    register long r1 asm("r1") = b;
    register long r2 asm("r2") = c;
    register long r3 asm("r3") = d;
    register long r4 asm("r4") = e;
    register long r5 asm("r5") = f;
    register long r7 asm("r7") = nr;

    asm volatile (
        "svc 0"
        : "+r"(r0)
        : "r"(r1), "r"(r2), "r"(r3),
          "r"(r4), "r"(r5), "r"(r7)
        : "memory");

    return r0;
}

static void write_str(const char *s)
{
    const char *p = s;
    long len = 0;

    while (p[len]) len++;

    syscall(SYS_write, STDOUT, (long)s, len, 0, 0, 0);
}

static long read_stdin(char *buf, long maxlen)
{
    return syscall(SYS_read, STDIN, (long)buf, maxlen, 0, 0, 0);
}

static void sleep_1s(void)
{
    struct timespec ts;
    ts.tv_sec = 1;
    ts.tv_nsec = 0;
    syscall(SYS_nanosleep, (long)&ts, 0, 0, 0, 0, 0);
}

static void reap_zombies(void)
{
    while (syscall(SYS_wait4, -1, 0, 1, 0, 0, 0) > 0) {
        /* reap all */
    }
}

void _start(void)
{
    char cmd[CMD_MAX_LEN];

    write_str("Hello, world!\n");

    for (;;) {
        reap_zombies();

        /* display prompt */
        write_str("$ ");

        /* read one line from stdin */
        long n = read_stdin(cmd, CMD_MAX_LEN-1);
        if (n > 0) {
            /* remove trailing newline */
            if (cmd[n-1] == '\n') n--;
            cmd[n] = 0;

            /* respond "command not found" */
            write_str(cmd);
            write_str(": command not found\n");
        }
    }

    syscall(SYS_exit, 0, 0, 0, 0, 0, 0);
}

