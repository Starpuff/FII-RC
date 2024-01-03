#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>

extern int errno;

int port;

int login(int sd, char *username, char *password);

int main(int argc, char *argv[])
{
    int sd;
    struct sockaddr_in server;
    int nr = 0;
    char buf[10];

    if (argc != 3)
    {
        printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    port = atoi(argv[2]);

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Eroare la socket().\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    char username[100];
    char password[100];

    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client] Error at connect().\n");
        return errno;
    }

    login(sd, username, password);

    while (true)
    {
        fflush(stdout);
        printf("[client] Enter 'quit' to close the connection: ");
        fflush(stdout);

        char input[10];
        scanf("%s", input);

        if (strcmp(input, "quit") == 0)
        {
            write(sd, input, strlen(input) + 1);
            fflush(stdout);
            printf("[client] Closing connection...\n");
            break;
        }
    }

    close(sd);
}

int login(int sd, char *username, char *password)
{
    printf("[client] Enter your username: ");
    fflush(stdout);
    read(0, username, sizeof(username));
    write(sd, username, strlen(username) + 1);
    fflush(stdout);

    char usernameStatus[100];
    if (read(sd, usernameStatus, sizeof(usernameStatus)) < 0)
    {
        perror("[client] Error at read().\n");
        return errno;
    }
    else
    {
        if (strcmp(usernameStatus, "Username exists.") == 0)
        {
            printf("[client] Enter your password: ");
            fflush(stdout);
            read(0, password, sizeof(password));
            write(sd, password, strlen(password) + 1);
            fflush(stdout);

            char passwordStatus[100];
            if (read(sd, passwordStatus, sizeof(passwordStatus)) < 0)
            {
                perror("[client] Error at read().\n");
                fflush(stdout);
                return errno;
            }
            else
            {
                if (strcmp(passwordStatus, "Password is correct.") == 0)
                {
                    printf("[server] %s\n", passwordStatus);
                    fflush(stdout);
                    char loginStatus[100];
                    if (read(sd, loginStatus, sizeof(loginStatus)) < 0)
                    {
                        perror("[client] Error at read().\n");
                        fflush(stdout);
                        return errno;
                    }
                    fflush(stdout);
                    printf("[server] %s\n", loginStatus);
                    fflush(stdout);
                }
                else if (strcmp(passwordStatus, "Password is incorrect.") == 0)
                {
                    printf("[server] %s\n", passwordStatus);
                    printf("[client] Closing connection...\n");
                    fflush(stdout);
                    close(sd);
                    return 0;
                }
                else
                {
                    printf("[client] Unexpected reply. Closing connection...\n");
                    fflush(stdout);
                    close(sd);
                    return 0;
                }
            }
        }
        else if (strcmp(usernameStatus, "Username does not exist.") == 0)
        {
            printf("[server] %s\n", usernameStatus);
            fflush(stdout);
            printf("[client] Closing connection...\n");
            fflush(stdout);
            close(sd);
            return 0;
        }
        else
        {
            printf("[client] Unexpected reply. Closing connection...\n");
            fflush(stdout);
            close(sd);
            return 0;
        }
        return 0;
    }
}
// g++ client.cpp -o client
//./client 127.0.0.1 2908