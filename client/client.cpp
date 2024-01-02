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

/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;

int main(int argc, char *argv[])
{
    int sd;                    // descriptorul de socket
    struct sockaddr_in server; // structura folosita pentru conectare
                               // mesajul trimis
    int nr = 0;
    char buf[10];

    /* exista toate argumentele in linia de comanda? */
    if (argc != 3)
    {
        printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    /* stabilim portul */
    port = atoi(argv[2]);

    /* cream socketul */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Eroare la socket().\n");
        return errno;
    }

    /* umplem structura folosita pentru realizarea conexiunii cu serverul */
    /* familia socket-ului */
    server.sin_family = AF_INET;
    /* adresa IP a serverului */
    server.sin_addr.s_addr = inet_addr(argv[1]);
    /* portul de conectare */
    server.sin_port = htons(port);

    char username[100];
    char password[100];

    /* ne conectam la server */
    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client] Error at connect().\n");
        return errno;
    }

    printf("[client] Enter your username: ");
    fflush(stdout);
    read(0, username, sizeof(username));
    write(sd, username, strlen(username) + 1);
    fflush(stdout);

    printf("[client] Enter your password: ");
    fflush(stdout);
    read(0, password, sizeof(password));
    write(sd, password, strlen(password) + 1);
    fflush(stdout);

    char loginStatus[100];
    read(sd, loginStatus, sizeof(loginStatus));

    printf("[server] %s\n", loginStatus);

    while (true)
    {
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
// g++ client.cpp -o client
//./client 127.0.0.1 2908