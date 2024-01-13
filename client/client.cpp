#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

using namespace std;

extern int errno;

int port;
int readSize(int sd);
void readPlusSize(int sd, char *message, int bufferSize);
int login(int sd, char *username, char *password);
void getAllUsers(int sd);
void getAllLoggedUsers(int sd);
int readSize(int sd);
void writePlusSize(int sd, const char *message);
void reverse(char str[], int length);
char *itoa(int num, char str[], int base);

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

    login(sd, username, password);

    // ----- send username to server ----
    writePlusSize(sd, username);
    // ----------------------------------

    while (true)
    {
        printf("\n[client] Choose a command: \n");
        printf("[client] 1 - Show all users.  2 - Show all online users.  3 - Send a message to a user.  4 - Show chat history with a user.  5 - Show all unread messages.  6 - Reply to a message.\n");
        printf("[client] 'quit' - close the connection. \n");
        printf("[client] To enter a command, type the corresponding number or phrase: ");
        fflush(stdout);

        char input[10];
        scanf("%s", input);

        if (strcmp(input, "1") == 0)
        {
            getAllUsers(sd);
        }
        else if (strcmp(input, "2") == 0)
        {
            getAllLoggedUsers(sd);
        }
        else if (strcmp(input, "quit") == 0)
        {
            writePlusSize(sd, "quit");
            printf("[client] Closing connection...\n");
            fflush(stdout);
            break;
        }
        else
        {
            printf("[client] Unknown command. Try again.\n");
            fflush(stdout);
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
    writePlusSize(sd, username);

    char usernameStatus[100];
    readPlusSize(sd, usernameStatus, 100);
    if (strcmp(usernameStatus, "Username exists.") == 0)
    {
        printf("[client] Enter your password: ");
        fflush(stdout);
        read(0, password, sizeof(password));
        password[strlen(password) - 1] = '\0';
        writePlusSize(sd, password);

        char passwordStatus[100];
        memset(passwordStatus, 0, 100);
        readPlusSize(sd, passwordStatus, 100);
        if (strcmp(passwordStatus, "Password is correct. Logging in...") == 0)
        {
            printf("[server] %s\n", passwordStatus);
            fflush(stdout);
        }
        else if (strcmp(passwordStatus, "Password is incorrect.") == 0)
        {
            printf("[server] %s\n", passwordStatus);
            printf("[client] Closing connection...\n");
            fflush(stdout);
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
    else if (strcmp(usernameStatus, "Username does not exist.") == 0)
    {
        printf("[server] %s\n", usernameStatus);
        printf("[client] Closing connection...\n");
        fflush(stdout);
        close(sd);
        exit(0);
        return 0;
    }
    else if (strcmp(usernameStatus, "User is already logged in.") == 0)
    {
        printf("[server] %s\n", usernameStatus);
        printf("[client] Closing connection...\n");
        close(sd);
        exit(0);
        return 0;
    }
    else
    {
        printf("[client] Unexpected reply. Closing connection...\n");
        close(sd);
        exit(0);
        return 0;
    }
    return 0;
}

void getAllUsers(int sd)
{
    writePlusSize(sd, "1");

    char allUsers[1000];
    memset(allUsers, 0, sizeof(allUsers));
    readPlusSize(sd, allUsers, 1000);

    printf("[Server] List of all users: %s\n", allUsers);
}

void getAllLoggedUsers(int sd)
{
    writePlusSize(sd, "2");

    char allLoggedUsers[1000];
    memset(allLoggedUsers, 0, sizeof(allLoggedUsers));
    readPlusSize(sd, allLoggedUsers, 1000);

    printf("[Server] List of all logged users: %s\n", allLoggedUsers);
}

int readSize(int sd)
{
    char size[10];
    memset(size, 0, 10);
    if (read(sd, size, 10) < 0)
    {
        perror("[client] Error at reading the size of the message.\n");
        close(sd);
        exit(-1);
    }
    // printf("[client] Message size: %s received\n", size);
    int messageSize = atoi(size);
    return messageSize;
}

void readPlusSize(int sd, char *message, int bufferSize)
{
    int messageSize = readSize(sd);
    if (messageSize > bufferSize)
    {
        perror("[client] Message size at reading is too big. Closing connection...\n");
        fflush(stdout);
        close(sd);
        exit(0);
    }
    if (messageSize <= 0)
    {
        perror("[client] Message size at reading is too small. Closing connection...\n");
        fflush(stdout);
        close(sd);
        exit(0);
    }
    char temp[bufferSize + 1];
    if (read(sd, temp, bufferSize) < 0)
    {
        perror("[client] Error at read(message) from server.\n");
        close(sd);
        exit(-1);
    }

    memset(message, 0, bufferSize);
    strncpy(message, temp, messageSize);

    // printf("[client] Message: %s received\n", message);
}

void writePlusSize(int sd, const char *message)
{
    size_t messageSize = strlen(message);
    char size[10];
    memset(size, 0, sizeof(size));
    itoa(messageSize, size, 10);
    if (write(sd, size, strlen(size)) < 0)
    {
        perror("[client] Error at write(length) to server.\n");
        fflush(stdout);
        close(sd);
        exit(-1);
    }
    // else {
    //     printf("[client] Message size: %ld sent\n", messageSize);
    // }

    sleep(1);

    if (write(sd, message, messageSize) < 0)
    {
        perror("[client] Error at write(message) to server.\n");
        fflush(stdout);
        close(sd);
        exit(-1);
    }
    // else {
    //     printf("[client] Message: %s sent\n", message);
    // }
}

void reverse(char str[], int length)
{
    int start = 0;
    int end = length - 1;
    while (start < end)
    {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

char *itoa(int num, char str[], int base)
{
    int i = 0;
    int isNegative = 0;

    // Handle 0 explicitly, otherwise empty string is printed
    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    // Handle negative numbers for bases other than 10
    if (num < 0 && base != 10)
    {
        isNegative = 1;
        num = -num;
    }

    // Process individual digits
    while (num != 0)
    {
        int remainder = num % base;
        str[i++] = (remainder > 9) ? (remainder - 10) + 'a' : remainder + '0';
        num = num / base;
    }

    // Append negative sign for bases other than 10
    if (isNegative && base != 10)
    {
        str[i++] = '-';
    }

    str[i] = '\0'; // Null-terminate the string

    // Reverse the string
    reverse(str, i);

    return str;
}
// g++ client.cpp -o client
//./client 127.0.0.1 2908