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
void getConversation(int sd, char *user, char *username);
int command3(int sd, char *sender, char *receiver);
void printMessage(char *message, char *username);
int command5(int sd, char *sender, char *receiver);
int getUnreadMessages(int sd, char *username);

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

    if (getUnreadMessages(sd, username) < 0)
    {
        printf("[client] Error at getUnreadMessages(). Closing connection...\n");
        fflush(stdout);
        close(sd);
        exit(0);
    }
    else
    {
        printf("[client] getUnreadMessages() handled!\n");
        fflush(stdout);
    }

    while (true)
    {
        printf("\n[client] Choose a command: \n");
        printf("[client] 1 - Show all users.  2 - Show all online users.  3 - Send a message to an user.  4 - Show chat history with an user.  5 - Reply to a message.\n");
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
        else if (strcmp(input, "3") == 0)
        {
            writePlusSize(sd, "3");

            char user[100];
            memset(user, 0, 100);
            printf("\n[client] Enter the username of the user you want to send a message to: ");
            fflush(stdout);
            scanf("%s", user);

            int status = command3(sd, username, user);
            if (status < 0)
            {
                printf("[client] User doens't exist. Try again.\n");
                fflush(stdout);
                break;
            }
        }
        else if (strcmp(input, "4") == 0)
        {
            writePlusSize(sd, "4");

            char user[100];
            memset(user, 0, 100);
            printf("[client] Enter the username of the user you want to see the chat history with: ");
            fflush(stdout);
            scanf("%s", user);

            if (strcmp(user, username) == 0)
            {
                printf("[client] You cannot see the chat history with yourself.\n");
                fflush(stdout);
            }
            else
            {
                getConversation(sd, user, username);
            }
        }
        else if (strcmp(input, "5") == 0)
        {
            writePlusSize(sd, "5");

            char user[100];
            memset(user, 0, 100);
            printf("\n[client] Enter the username of the user you want to send a message to: ");
            fflush(stdout);
            scanf("%s", user);

            if (command5(sd, username, user) < 0)
            {
                printf("[client] User or message doesn't exist. Try again.\n");
                fflush(stdout);
                break;
            }
            else
            {
                printf("[client] Command 5 handled!\n");
                fflush(stdout);
            }
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
        else if (strcmp(passwordStatus, "Password is incorrect or user is already logged in.") == 0)
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

    printf("\n\n[Server] List of all users: %s\n", allUsers);
}

void getAllLoggedUsers(int sd)
{
    writePlusSize(sd, "2");

    char allLoggedUsers[1000];
    memset(allLoggedUsers, 0, sizeof(allLoggedUsers));
    readPlusSize(sd, allLoggedUsers, 1000);

    printf("\n\n[Server] List of all logged users: %s\n", allLoggedUsers);
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

    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    if (num < 0 && base != 10)
    {
        isNegative = 1;
        num = -num;
    }

    while (num != 0)
    {
        int remainder = num % base;
        str[i++] = (remainder > 9) ? (remainder - 10) + 'a' : remainder + '0';
        num = num / base;
    }

    if (isNegative && base != 10)
    {
        str[i++] = '-';
    }

    str[i] = '\0';

    reverse(str, i);

    return str;
}

void getConversation(int sd, char *user, char *username)
{
    writePlusSize(sd, user);

    char *userStatus = (char *)calloc(100, sizeof(char));
    readPlusSize(sd, userStatus, 100);

    if (strcmp(userStatus, "Other user does not exist.") == 0)
    {
        printf("[server] %s Please retry with another username.\n", userStatus);
        fflush(stdout);
        return;
    }
    else if (strcmp(userStatus, "Other user exists. Fetching conversation...") == 0)
    {
        printf("[server] %s\n", userStatus);
        fflush(stdout);
    }
    else
    {
        printf("[client] Unexpected reply. Closing connection...\n");
        fflush(stdout);
        close(sd);
        exit(0);
        return;
    }

    char *conversationStatus = (char *)calloc(100, sizeof(char));
    readPlusSize(sd, conversationStatus, 100);

    if (strcmp(conversationStatus, "Conversation exists.") == 0)
    {
        printf("[server] %s\n", conversationStatus);
        fflush(stdout);
    }
    else if (strcmp(conversationStatus, "No conversation found.") == 0)
    {
        printf("[server] %s\n", conversationStatus);
        fflush(stdout);
        return;
    }
    else
    {
        printf("[client] Unexpected reply. Closing connection...\n");
        fflush(stdout);
        close(sd);
        exit(0);
        return;
    }

    char *message = (char *)calloc(1200, sizeof(char));
    readPlusSize(sd, message, 1200);
    while (strcmp(message, "end") != 0)
    {
        printMessage(message, username);
        readPlusSize(sd, message, 1200);
    }
}

void printMessage(char *message, char *username)
{
    char *id = (char *)calloc(10, sizeof(char));
    char *sender = (char *)calloc(100, sizeof(char));
    char *messageContent = (char *)calloc(1000, sizeof(char));
    char *replyTo = (char *)calloc(10, sizeof(char));
    char *readByReceiver = (char *)calloc(10, sizeof(char));

    char *p = strtok(message, "|");
    strcpy(id, p);
    p = strtok(NULL, "|");
    strcpy(sender, p);
    p = strtok(NULL, "|");
    strcpy(messageContent, p);
    p = strtok(NULL, "|");
    strcpy(replyTo, p);
    p = strtok(NULL, "|");
    strcpy(readByReceiver, p);

    if (strcmp(readByReceiver, "0") == 0 && strcmp(sender, username) != 0)
    {
        if (strcmp(replyTo, "0") != 0)
        {
            printf("[Unread!] id: %s -- [%s] %s (reply to %s)\n", id, sender, messageContent, replyTo);
        }
        else
        {
            printf("[Unread!] id: %s -- [%s] %s\n", id, sender, messageContent);
        }
    }
    else
    {
        if (strcmp(replyTo, "0") != 0)
        {
            printf("id: %s -- [%s] %s (reply to %s)\n", id, sender, messageContent, replyTo);
        }
        else
        {
            printf("id: %s -- [%s] %s\n", id, sender, messageContent);
        }
    }

    free(id);
    free(sender);
    free(messageContent);
    free(replyTo);
    free(readByReceiver);
}

int command3(int sd, char *sender, char *receiver)
{
    writePlusSize(sd, receiver);

    char *status = (char *)calloc(100, sizeof(char));
    readPlusSize(sd, status, 100);

    if (strcmp(status, "Other user does not exist.") == 0)
    {
        printf("[server] %s Please retry with another username.\n", status);
        fflush(stdout);
        return -1;
    }
    else if (strcmp(status, "Other user exists. Send message!") == 0)
    {
        printf("[server] %s\n", status);
        fflush(stdout);
    }
    else
    {
        printf("[client] Unexpected reply. Closing connection...\n");
        fflush(stdout);
        close(sd);
        exit(0);
        return -1;
    }

    char *message = (char *)calloc(1000, sizeof(char));
    printf("[client] Enter your message to %s: ", receiver);
    fflush(stdout);
    read(0, message, 1000);
    message[strlen(message) - 1] = '\0';

    writePlusSize(sd, message);

    char *confirmation = (char *)calloc(100, sizeof(char));
    readPlusSize(sd, confirmation, 100);
    printf("[server] %s\n", confirmation);
    fflush(stdout);
    return 1;
}

int command5(int sd, char *sender, char *receiver)
{
    writePlusSize(sd, receiver);

    char *status = (char *)calloc(100, sizeof(char));
    readPlusSize(sd, status, 100);

    if (strcmp(status, "Other user does not exist.") == 0)
    {
        printf("[server] %s Please retry with another username.\n", status);
        fflush(stdout);
        return 0;
    }
    else if (strcmp(status, "Other user exists. Send the id of the message you want to reply to!") == 0)
    {
        printf("[server] %s\n", status);
        fflush(stdout);
    }
    else
    {
        printf("[client] Unexpected reply. Closing connection...\n");
        fflush(stdout);
        return -1;
    }

    printf("[client] Enter the id of the message you want to reply to: ");
    fflush(stdout);
    char *id = (char *)calloc(10, sizeof(char));
    read(0, id, 10);
    id[strlen(id) - 1] = '\0';

    writePlusSize(sd, id);

    char *idStatus = (char *)calloc(100, sizeof(char));
    readPlusSize(sd, idStatus, 100);
    if (strcmp(idStatus, "No message with that id found.") == 0)
    {
        printf("[server] %s Please try with another id.\n", idStatus);
        fflush(stdout);
        return 0;
    }
    else if (strcmp(idStatus, "Message found. Send the reply!") == 0)
    {
        printf("[server] %s\n", idStatus);
        fflush(stdout);
    }
    else
    {
        printf("[client] Unexpected reply. Closing connection...\n");
        fflush(stdout);
        return -1;
    }

    char *message = (char *)calloc(1000, sizeof(char));
    printf("[client] Enter your reply to %s: ", receiver);
    fflush(stdout);
    read(0, message, 1000);
    message[strlen(message) - 1] = '\0';

    writePlusSize(sd, message);

    char *confirmation = (char *)calloc(100, sizeof(char));
    readPlusSize(sd, confirmation, 100);
    if (strcmp(confirmation, "Reply sent!") == 0)
    {
        printf("[server] %s\n", confirmation);
        fflush(stdout);
        return 1;
    }
    else if (strcmp(confirmation, "Error at insertReplyQuery.") == 0)
    {
        printf("[server] %s\n", confirmation);
        fflush(stdout);
        return -1;
    }
    else
    {
        printf("[client] Unexpected reply. Closing connection...\n");
        fflush(stdout);
        return -1;
    }
}

int getUnreadMessages(int sd, char *username)
{
    char *unreadMessageData = (char *)calloc(1200, sizeof(char));
    readPlusSize(sd, unreadMessageData, 1200);
    while (strcmp(unreadMessageData, "end") != 0)
    {
        printMessage(unreadMessageData, username);
        readPlusSize(sd, unreadMessageData, 1200);
    }

    return 1;
}

// g++ client.cpp -o client
//./client 127.0.0.1 2908