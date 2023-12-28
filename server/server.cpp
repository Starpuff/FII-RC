#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <unordered_set>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

#define PORT 2908

extern int errno;

typedef struct thData
{
  int idThread; // id-ul thread-ului tinut in evidenta de acest program
  int cl;       // descriptorul intors de accept
} thData;

static void *treat(void *);
void raspunde(void *);

json usersData;

int main()
{
  struct sockaddr_in server; // structura folosita de server
  struct sockaddr_in from;
  int nr; // mesajul primit de trimis la client
  int sd; // descriptorul de socket
  int pid;
  pthread_t th[100]; // Identificatorii thread-urilor care se vor crea
  int i = 0;

  /* crearea unui socket */
  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("[server]Eroare la socket().\n");
    return errno;
  }
  /* utilizarea optiunii SO_REUSEADDR */
  int on = 1;
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  /* pregatirea structurilor de date */
  bzero(&server, sizeof(server));
  bzero(&from, sizeof(from));

  /* umplem structura folosita de server */
  /* stabilirea familiei de socket-uri */
  server.sin_family = AF_INET;
  /* acceptam orice adresa */
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  /* utilizam un port utilizator */
  server.sin_port = htons(PORT);

  /* atasam socketul */
  if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
  {
    perror("[server]Eroare la bind().\n");
    return errno;
  }

  /* punem serverul sa asculte daca vin clienti sa se conecteze */
  if (listen(sd, 2) == -1)
  {
    perror("[server]Eroare la listen().\n");
    return errno;
  }

  printf("[server]Asteptam la portul %d...\n", PORT);
  fflush(stdout);

  /* servim in mod concurent clientii...folosind thread-uri */
  while (1)
  {
    int client;
    thData *td; // parametru functia executata de thread
    unsigned int length = sizeof(from);

    // client= malloc(sizeof(int));
    /* acceptam un client (stare blocanta pina la realizarea conexiunii) */
    if ((client = accept(sd, (struct sockaddr *)&from, &length)) < 0)
    {
      perror("[server]Eroare la accept().\n");
      continue;
    }

    /* s-a realizat conexiunea, se astepta mesajul */

    // int idThread; //id-ul threadului
    // int cl; //descriptorul intors de accept

    td = (struct thData *)malloc(sizeof(struct thData));
    td->idThread = i++;
    td->cl = client;

    pthread_create(&th[i], NULL, &treat, td);

  } // while
};

void loadUserData()
{
  ifstream usersFile("users.json");
  usersFile >> usersData;
}

void saveUserData()
{
  ofstream usersFile("users.json");
  usersFile << usersData.dump(4); // Writing formatted JSON to file
}

// Function to handle client login
void handleLogin(int clientSocket)
{
  char username[100];
  char password[100];

  // Receive username and password from the client
  read(clientSocket, username, sizeof(username));
  read(clientSocket, password, sizeof(password));

  string uname = string(username);
  string pass = string(password);

  bool userExists = false;
  bool alreadyOnline = false;
  bool passwordMatch = false;

  for (auto &user : usersData["users"])
  {
    if (user["username"] == uname)
    {
      userExists = true;
      if (user["status"] == "online")
      {
        alreadyOnline = true;
        break;
      }
      if (user["password"] == pass)
      {
        passwordMatch = true;
        user["status"] = "online"; // Set user as online in JSON
        saveUserData();            // Save updated JSON data
        break;
      }
    }
  }

  string loginStatus;
  if (!userExists)
  {
    loginStatus = "User doesn't exist!";
  }
  else if (alreadyOnline)
  {
    loginStatus = "User is already logged in!";
  }
  else if (passwordMatch)
  {
    loginStatus = "Login successful!";
  }
  else
  {
    loginStatus = "Invalid password!";
  }

  // Send login status to the client
  write(clientSocket, loginStatus.c_str(), loginStatus.size() + 1);
}

static void *treat(void *arg)
{
  struct thData tdL;
  tdL = *((struct thData *)arg);
  printf("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
  fflush(stdout);
  pthread_detach(pthread_self());
  raspunde((struct thData *)arg);
  handleLogin(((thData *)arg)->cl);

  while (true)
  {
    char command[100];
    int bytesRead = read(((thData *)arg)->cl, &command, sizeof(int));

    if (bytesRead <= 0)
    {
      printf("Client disconnected or encountered an error.\n");
      break;
    }

    if (strcmp(command, "show users") == 0)
    {
      printf("Function to show all users\n");
    }
    else if (strcmp(command, "quit") == 0)
    {
      close(((thData *)arg)->cl);
      return (NULL);
    }
    else
    {
      printf("Received unknown command from the client.\n");
    }
  }

  /* am terminat cu acest client, inchidem conexiunea */
  close((intptr_t)arg);
  return (NULL);
};

void raspunde(void *arg)
{
  int nr, i = 0;
  struct thData tdL;
  tdL = *((struct thData *)arg);
  if (read(tdL.cl, &nr, sizeof(int)) <= 0)
  {
    printf("[Thread %d]\n", tdL.idThread);
    perror("Eroare la read() de la client.\n");
  }

  printf("[Thread %d]Mesajul a fost receptionat...%d\n", tdL.idThread, nr);

  /*pregatim mesajul de raspuns */
  nr++;
  printf("[Thread %d]Trimitem mesajul inapoi...%d\n", tdL.idThread, nr);

  /* returnam mesajul clientului */
  if (write(tdL.cl, &nr, sizeof(int)) <= 0)
  {
    printf("[Thread %d] ", tdL.idThread);
    perror("[Thread]Eroare la write() catre client.\n");
  }
  else
    printf("[Thread %d]Mesajul a fost trasmis cu succes.\n", tdL.idThread);
}