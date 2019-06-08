#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <wait.h>

#define MAX_NUMBER_OF_CLIENTS 50
#define PORT 3218

FILE *LOG_FILE = NULL;

int SOCKET_FD = -1;

void daemonize();

char *timestamp();

void on_sigchld(int signo);

int main(int argc, char const *argv[])
{
    int result = fork();

    if (result == -1)
    {
        fprintf(stderr, "%s - %d - error: %s.\n", timestamp(), getpid(), strerror(errno));
        fflush(stderr);
        exit(errno);
    }
    else if (result > 0)
    {
        return 0;
    }

    daemonize();

    LOG_FILE = fopen("log.txt", "a");

    if (LOG_FILE == NULL)
        exit(errno);

    fprintf(LOG_FILE, "%s - %d - process succesfully daemonized.\n", timestamp(), getpid());
    fflush(LOG_FILE);

    struct sigaction on_sigchld_act = {.sa_handler = &on_sigchld};
    struct sigaction old_on_sigchld_act;

    if (sigaction(SIGCHLD, &on_sigchld_act, &old_on_sigchld_act) == -1)
    {
        fprintf(LOG_FILE, "%s - %d - error while creating handler of child exit: %s\n", timestamp(), getpid(), strerror(errno));
        fflush(LOG_FILE);
        exit(errno);
    }

    SOCKET_FD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (SOCKET_FD == -1)
    {
        fprintf(LOG_FILE, "%s - %d - error while opening socket: %s\n", timestamp(), getpid(), strerror(errno));
        fflush(LOG_FILE);
        exit(errno);
    }

    const struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr = htonl(INADDR_LOOPBACK)};

    if (bind(SOCKET_FD, (const struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
    {
        fprintf(LOG_FILE, "%s - %d - error while binding socket: %s\n", timestamp(), getpid(), strerror(errno));
        fflush(LOG_FILE);
        exit(errno);
    }

    fprintf(LOG_FILE, "%s - %d - binded socket\n", timestamp(), getpid());
    fflush(LOG_FILE);

    if (listen(SOCKET_FD, MAX_NUMBER_OF_CLIENTS))
    {
        fprintf(LOG_FILE, "%s - %d - error while preparing socket for listening: %s\n", timestamp(), getpid(), strerror(errno));
        fflush(LOG_FILE);
        exit(errno);
    }

    fprintf(LOG_FILE, "%s - %d - started listening for clients\n", timestamp(), getpid());
    fflush(LOG_FILE);

    while (1)
    {
        socklen_t client_addr_size = (socklen_t)sizeof(struct sockaddr_in);
        struct sockaddr_in client_addr;
        int client_fd = accept(SOCKET_FD, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_fd == -1)
        {
            if (errno != EINTR)
            {
                fprintf(LOG_FILE, "%s - %d - error while accepting connection: %s\n", timestamp(), getpid(), strerror(errno));
                fflush(LOG_FILE);
            }
            continue;
        }

        fprintf(LOG_FILE, "%s - %d - accepted new connection\n", timestamp(), getpid());
        fflush(LOG_FILE);

        pid_t child_pid = fork();

        if (child_pid == 0)
        {
            fprintf(LOG_FILE, "%s - %d - started listening for messages\n", timestamp(), getpid());
            fflush(LOG_FILE);

            while (1)
            {
                char request[1024];
                if (recv(client_fd, request, 1024, 0) == -1)
                {
                    fprintf(LOG_FILE, "%s - %d - error while reading from socket: %s\n", timestamp(), getpid(), strerror(errno));
                    fflush(LOG_FILE);
                    exit(errno);
                }

                fprintf(LOG_FILE, "%s - %d - received message: %s\n", timestamp(), getpid(), request);
                fflush(LOG_FILE);

                char response[1024];
                int written = sprintf(response, "%s - %d - %s", timestamp(), getpid(), request);

                if (send(client_fd, response, written + 1, 0) == -1)
                {
                    fprintf(LOG_FILE, "%s - %d - error while writing from socket: %s\n", timestamp(), getpid(), strerror(errno));
                    fflush(LOG_FILE);
                    exit(errno);
                }

                if (strcmp(request, "close") == 0)
                {
                    fprintf(LOG_FILE, "%s - %d - received command to stop\n", timestamp(), getpid());
                    fflush(LOG_FILE);

                    if (close(client_fd) == -1)
                    {
                        fprintf(LOG_FILE, "%s - %d - error while closing socket with fd %d: %s\n", timestamp(), getpid(), client_fd, strerror(errno));
                        fflush(LOG_FILE);
                        exit(errno);
                    }

                    fprintf(LOG_FILE, "%s - %d - closed socket\n", timestamp(), getpid());
                    fprintf(LOG_FILE, "%s - %d - exiting\n", timestamp(), getpid());
                    fflush(LOG_FILE);
                    exit(0);
                }
            }
        }
        else if (child_pid == -1)
        {
            fprintf(LOG_FILE, "%s - %d - error while forking child for new connection: %s\n", timestamp(), getpid(), strerror(errno));
            fflush(LOG_FILE);
        }
        else
        {
            fprintf(LOG_FILE, "%s - %d - forked child with pid %d\n", timestamp(), getpid(), child_pid);
            fflush(LOG_FILE);
        }

        if (close(client_fd) == -1)
        {
            fprintf(LOG_FILE, "%s - %d - error while closing socket with fd %d: %s\n", timestamp(), getpid(), client_fd, strerror(errno));
            fflush(LOG_FILE);
        }
    }

    return 0;
}

void close_all_descriptors()
{
    for (int fd = 0; fd < FOPEN_MAX; fd++)
    {
        close(fd);
    }
}

void redirect_std_streams()
{
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);
}

void daemonize()
{
    if (setsid() == -1)
    {
        fprintf(stderr, "%s - %d - error: %s.\n", timestamp(), getpid(), strerror(errno));
        fflush(stderr);
        exit(errno);
    }

    if (chdir("/") == -1)
    {
        fprintf(stderr, "%s - %d - error: %s.\n", timestamp(), getpid(), strerror(errno));
        fflush(stderr);
        exit(errno);
    }

    close_all_descriptors();

    redirect_std_streams();
}

char *timestamp()
{
    char *buffer = (char *)malloc(20);
    time_t curr_time = time(NULL);
    strftime(buffer, 20, "%F %T", localtime(&curr_time));
    return buffer;
}

void on_sigchld(int signo)
{
    int status;
    pid_t child_pid = wait(&status);
    fprintf(LOG_FILE, "%s - %d - child process %d exited with status: %d\n", timestamp(), getpid(), child_pid, WEXITSTATUS(status));
    fflush(LOG_FILE);
}