#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>

#define PORT 3218

int main(int argc, char const *argv[])
{
    int socket_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    const struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr = htonl(INADDR_LOOPBACK)};

    if (connect(socket_fd, (const struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
    {
        printf("Error while connecting to server: %s\n", strerror(errno));
        return errno;
    }

    printf("Connected to server\n");

    int working = 1;
    while (working == 1)
    {
        char request[1024];

        printf("Write your request: ");
        scanf("%s", request);

        if (strcmp(request, "close") == 0)
            working = 0;

        if (send(socket_fd, request, strlen(request) + 1, 0) == -1)
        {
            printf("Error while sending to server: %s\n", strerror(errno));
            return errno;
        }

        char responce[1024];
        if (recv(socket_fd, responce, 1024, 0) == -1)
        {
            printf("Error while receiving from server: %s\n", strerror(errno));
            return errno;
        }

        printf("Responce: %s\n", responce);
    }

    return 0;
}
