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
#include <iostream>

using namespace std;

extern int errno;

int port;

int login(int sd, char *username, char *password);
void getAllUsers(int sd, char *username);
void readSize(int sd, size_t *size);
void writePlusSize(int sd, char *message);

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

    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client] Error at connect().\n");
        return errno;
    }

    char username[100];
    char password[100];
    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));
    login(sd, username, password);

    while (true)
    {
        printf("[client] Choose a command: \n");
        printf("[client] 1 - Show all users. \n");
        printf("[client] 2 - Show all online users. \n");
        printf("[client] 3 - Send a message to a user. \n");
        printf("[client] 4 - Show chat history with a user. \n");
        printf("[client] 5 - Show all unread messages. \n");
        printf("[client] 6 - Reply to a message. \n");
        printf("[client] 'quit' - close the connection. \n");
        printf("[client] To enter a command, type the corresponding number or phrase: ");
        fflush(stdout);

        char input[10];
        scanf("%s", input);

        if (strcmp(input, "1") == 0)
        {
            getAllUsers(sd, username);
        }

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
    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));

    printf("[client] Enter your username: ");
    fflush(stdout);
    read(0, username, sizeof(username));
    username[strlen(username) - 1] = '\0';

    printf("aici0");
    writePlusSize(sd, username);
    printf("aici4");
    fflush(stdout);

    /// TODO: aici am ajuns

    char usernameStatus[100];
    memset(usernameStatus, 0, sizeof(usernameStatus));
    if (read(sd, usernameStatus, sizeof(usernameStatus)) < 0)
    {
        perror("[client] Error at read().\n");
        return errno;
    }
    else
    {
        if (strcmp(usernameStatus, "Username exists.") == 0)
        {
            fflush(stdout);
            printf("[client] Enter your password: ");
            fflush(stdout);
            read(0, password, sizeof(password));
            password[strlen(password) - 1] = '\0';
            fflush(stdout);
            write(sd, password, strlen(password) + 1);
            fflush(stdout);

            char passwordStatus[100];
            memset(passwordStatus, 0, sizeof(passwordStatus));
            if (read(sd, passwordStatus, sizeof(passwordStatus)) < 0)
            {
                perror("[client] Error at read().\n");
                fflush(stdout);
                return errno;
            }
            else
            {
                if (strcmp(passwordStatus, "Password is correct. Logging in...") == 0)
                {
                    fflush(stdout);
                    printf("[server] %s\n", passwordStatus);
                    fflush(stdout);
                }
                else if (strcmp(passwordStatus, "Password is incorrect.") == 0)
                {
                    fflush(stdout);
                    printf("[server] %s\n", passwordStatus);
                    fflush(stdout);
                    printf("[client] Closing connection...\n");
                    close(sd);
                    exit(0);
                    return 0;
                }
                else
                {
                    printf("[client] Unexpected reply. Closing connection...\n");
                    fflush(stdout);
                    close(sd);
                    exit(0);
                    return 0;
                }
            }
        }
        else if (strcmp(usernameStatus, "Username does not exist.") == 0)
        {
            fflush(stdout);
            printf("[server] %s\n", usernameStatus);
            fflush(stdout);
            printf("[client] Closing connection...\n");
            fflush(stdout);
            close(sd);
            exit(0);
            return 0;
        }
        else if (strcmp(usernameStatus, "User is already logged in.") == 0)
        {
            printf("[server] %s\n", usernameStatus);
            fflush(stdout);
            printf("[client] Closing connection...\n");
            fflush(stdout);
            close(sd);
            exit(0);
            return 0;
        }
        else
        {
            fflush(stdout);
            printf("[client] Unexpected reply. Closing connection...\n");
            fflush(stdout);
            close(sd);
            exit(0);
            return 0;
        }
        return 0;
    }
}

void getAllUsers(int sd, char *username)
{
    write(sd, "1", strlen("1") + 1);

    size_t usernamesSize;
    readSize(sd, &usernamesSize);

    char *allUsers = (char *)malloc(usernamesSize + 1);
    if (allUsers == NULL)
    {
        perror("[client] Error at malloc().\n");
        return;
    }
    if (read(sd, allUsers, usernamesSize) < 0)
    {
        perror("[client] Error at read().\n");
        fflush(stdout);
        free(allUsers);
        return;
    }

    allUsers[usernamesSize] = '\0';
    printf("[Server] List of all users: %s\n", allUsers);
}

void readSize(int sd, size_t *size)
{
    if (read(sd, size, sizeof(size)) < 0)
    {
        perror("[client] Error at read().\n");
        return;
    }
}

void writePlusSize(int sd, char *message)
{
    size_t messageSize = strlen(message);
    printf("aici");
    if (write(sd, &messageSize, sizeof(messageSize)) < 0)
    {
        perror("[client] Error at write(length) to server.\n");
        free(message);
        return;
    }
    printf("aici2");
    if (write(sd, message, messageSize) < 0)
    {
        perror("[client] Error at write(message) to server.\n");
        free(message);
        return;
    }
    printf("aici3");
    free(message);
}
// g++ client.cpp -o client
//./client 127.0.0.1 2908
