#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>

#define RUNDIR "/run/network/"

int verbose = 0;
long timeout = 0;
long nonblocking = 0;
int unlock = 0;
char *iface= NULL;

int lock(const char *filename, int type, int nonblocking)
{
    struct flock lock;
    int result = 0;

    lock.l_type = type;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    FILE *f = fopen(filename, "w");

    if (f)
    {
        if( fcntl(fileno(f), nonblocking ? F_SETLK : F_SETLKW, &lock) )
        {
            if (verbose)
                fprintf(stderr, "Failed to lock %s: %s\n", filename, strerror(errno));
            result = 1;
        }

        if (!result && type == F_WRLCK)
            sleep(10);

        fclose(f);
    }
    else
    {
        result = 1;
        fprintf(stderr, "Failed to access '%s': %s\n", filename, strerror(errno));
    }

    return result;
}

int lock_iface(char *iface, int nonblocking, int unlock)
{
    char filename[100];
    sprintf(filename, RUNDIR "%s.lock", iface);

    return lock(filename, unlock ? F_UNLCK : F_WRLCK, nonblocking);
}

void alarm_handler(int sig)
{
    if (sig == SIGALRM)
    {
        if (verbose)
            fprintf(stderr, "Timeout, stopping...\n");
        abort();
    }
}

void setup_alarm(long timeout)
{
    alarm(timeout);

    struct sigaction sa;
    sa.sa_handler = &alarm_handler;

    sigaction(SIGALRM, &sa, NULL);
}

void usage(const char *execname)
{
    printf("Usage:\n\t%s [options] <interface_name>\n", execname);
    printf("\nwhere [options] are:\n");
    printf("\t-h | --help - print this help message\n");
    printf("\t-t | --timeout= <timeout> - time to wait for a lock to be released\n");
    printf("\t-v | --verbose - more verbose output\n");
    printf("\t-n | --non-blocking - don't block on waiting for lock\n");
    printf("\t-u | --unlock - try to unlock instead of locking\n");
    printf("\n");
}

int main(int argc, char *argv[])
{
    struct option long_opts[] = {
        {"help", no_argument, NULL, 'h'},
        {"verbose", no_argument, NULL, 'v'},
        {"timeout", required_argument, NULL, 't'},
        {"non-blocking", no_argument, NULL, 'n'},
        {"unlock", no_argument, NULL, 'u'},
        {0, 0, 0, 0}
    };
    int result = 0;

    for (;;)
    {
        int c = getopt_long(argc, argv, "hvt:nu", long_opts, NULL);

        if (c == EOF)
            break;

        switch (c) {
            case 'h':
                usage(argv[0]);
                exit(1);
                break;
            case 'v':
                verbose = 1;
                break;
            case 't':
                timeout = strtol(optarg, NULL, 10);
                break;
            case 'n':
                nonblocking = 1;
                break;
            case 'u':
                unlock = 1;
                break;
            default:
                usage(argv[0]);
                break;
        }
    }

    if (optind != argc - 1)
    {
        usage(argv[0]);
        exit(1);
    }

    iface = strdup(argv[optind]);

    if (timeout > 0)
    {
        setup_alarm(timeout);
    }

    result = lock_iface(iface, nonblocking, unlock);

    return result;
}
