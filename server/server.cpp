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
#include <sqlite3.h>
#include <sys/stat.h>

using namespace std;
using json = nlohmann::json;

#define PORT 2908

extern int errno;

typedef struct thData
{
  int idThread;
  int cl;
} thData;

static void *treat(void *);
void raspunde(void *);
void initializeDatabase(sqlite3 *db);
static int callback(void *data, int argc, char **argv, char **azColName);

json usersData;

int main()
{
  struct sockaddr_in server;
  struct sockaddr_in from;
  int sd;
  int pid;
  pthread_t th[100];
  int i = 0;

  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("[server] Error at creating socket.\n");
    return errno;
  }

  int on = 1;
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  bzero(&server, sizeof(server));
  bzero(&from, sizeof(from));

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(PORT);

  if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
  {
    perror("[server] Error at bind().\n");
    return errno;
  }

  sqlite3 *db;
  char *err_msg = 0;

  int rc = sqlite3_open("database.db", &db);

  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    exit(1);
  }
  else
  {
    fprintf(stdout, "Opened database successfully\n");
  }

  initializeDatabase(db);

  const char *sql = "SELECT username FROM users;";
  rc = sqlite3_exec(db, sql, callback, 0, &err_msg);

  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "Failed to select data\n");
    fprintf(stderr, "SQL error: %s\n", err_msg);

    sqlite3_free(err_msg);
    sqlite3_close(db);

    return 1;
  }

  if (listen(sd, 2) == -1)
  {
    perror("[server] Error at listen().\n");
    return errno;
  }

  printf("[server] Waiting at port %d...\n", PORT);
  fflush(stdout);

  while (1)
  {
    int client;
    thData *td;
    unsigned int length = sizeof(from);

    if ((client = accept(sd, (struct sockaddr *)&from, &length)) < 0)
    {
      perror("[server]Error at accept().\n");
      continue;
    }

    td = (struct thData *)malloc(sizeof(struct thData));
    td->idThread = i++;
    td->cl = client;

    pthread_create(&th[i], NULL, &treat, td);
  }
};

static void *treat(void *arg)
{
  struct thData tdL;
  tdL = *((struct thData *)arg);
  printf("[thread]- %d - Waiting for messages...\n", tdL.idThread);
  fflush(stdout);
  pthread_detach(pthread_self());
  raspunde((struct thData *)arg);

  while (true)
  {
    char command[100];
    int bytesRead = read(((thData *)arg)->cl, &command, sizeof(int));

    if (strcmp(command, "quit") == 0)
    {
      close(((thData *)arg)->cl);
      printf("[Thread %d] Client disconnected.\n", tdL.idThread);
      return (NULL);
    }
    else if (bytesRead <= 0)
    {
      printf("[Thread %d] Client disconnected or encountered an error.\n", tdL.idThread);
      break;
    }
    else
    {
      printf("[Thread %d] Received an unknown command from this client.\n", tdL.idThread);
    }
  }
  close((intptr_t)arg);
  return (NULL);
};

void raspunde(void *arg)
{
  char username[100];
  char password[100];
  struct thData tdL;
  tdL = *((struct thData *)arg);

  if (read(tdL.cl, username, sizeof(username)) <= 0)
  {
    printf("[Thread %d]\n", tdL.idThread);
    perror("Error at read(username) from client.\n");
    return;
  }

  printf("[Thread %d] Username received: %s\n", tdL.idThread, username);

  if (read(tdL.cl, password, sizeof(password)) <= 0)
  {
    printf("[Thread %d]\n", tdL.idThread);
    perror("Error at read(password) from client.\n");
    return;
  }

  printf("[Thread %d] Password received: %s\n", tdL.idThread, password);

  char confirmation[] = "Received username and password.";
  if (write(tdL.cl, confirmation, sizeof(confirmation)) <= 0)
  {
    printf("[Thread %d] ", tdL.idThread);
    perror("[Thread] Error at write(confirmation) to the client.\n");
  }
  else
  {
    printf("[Thread %d] Confirmation message sent.\n", tdL.idThread);
  }
}

void initializeDatabase(sqlite3 *db)
{

  char *err_msg = 0;
  const char *createUsersTableQuery = "BEGIN TRANSACTION;"
                                      "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT NOT NULL UNIQUE, password TEXT NOT NULL);"
                                      "INSERT OR IGNORE INTO users (username, password) VALUES ('a', 'a');"
                                      "INSERT OR IGNORE INTO users (username, password) VALUES ('b', 'b');"
                                      "INSERT OR IGNORE INTO users (username, password) VALUES ('c', 'c');"
                                      "INSERT OR IGNORE INTO users (username, password) VALUES ('d', 'd');"
                                      "INSERT OR IGNORE INTO users (username, password) VALUES ('e', 'e');"
                                      "INSERT OR IGNORE INTO users (username, password) VALUES ('f', 'f');"
                                      "COMMIT;";
  int rc = sqlite3_exec(db, createUsersTableQuery, 0, 0, &err_msg);

  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    exit(1);
  }
}

static int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
  printf("Username = %s\n", argv[0] ? argv[0] : "NULL");
  return 0;
}