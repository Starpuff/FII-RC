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
#include <sqlite3.h>
#include <cstdlib>
#include <map>

using namespace std;

#define PORT 2908

extern int errno;

map<char *, bool> loggedUsers;

typedef struct thData
{
  int idThread;
  int cl;
} thData;

static void *treat(void *);
void handleLogin(void *, sqlite3 *db);
void initializeDatabase(sqlite3 *db);
static int callbackPrintUsers(void *data, int argc, char **argv, char **azColName);
static int callbackUsernameExists(void *data, int argc, char **argv, char **azColName);
void createConversationTable(sqlite3 *db, char *user1, char *user2, char *conversationName);
void logUserIn(char *username, int threadId);
bool userIsLoggedIn(char *username);
char *getAllUsers(sqlite3 *db);
int callbackGetAllUsers(void *data, int argc, char **argv, char **azColName);

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

  // ------------------ DATABASE ------------------

  sqlite3 *db;
  char *err_msg = 0;

  int rc = sqlite3_open("database.db", &db);

  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    fflush(stdout);
    sqlite3_close(db);
    exit(1);
  }
  else
  {
    fprintf(stdout, "Opened database successfully\n");
    fflush(stdout);
  }

  initializeDatabase(db);

  const char *sql = "SELECT username FROM users;";
  rc = sqlite3_exec(db, sql, callbackPrintUsers, 0, &err_msg);

  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "Failed to select data\n");
    fprintf(stderr, "SQL error: %s\n", err_msg);
    fflush(stdout);

    sqlite3_free(err_msg);
    sqlite3_close(db);

    return 1;
  }

  // ------------------ no more database --------------------

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
  sqlite3 *db;
  int rc = sqlite3_open("database.db", &db);

  if (rc != SQLITE_OK)
  {
    printf("[Thread %d] Cannot open database: %s\n", tdL.idThread, sqlite3_errmsg(db));
    fflush(stdout);
    sqlite3_close(db);
    exit(1);
  }
  else
  {
    printf("[Thread %d] Opened database successfully\n", tdL.idThread);
    fflush(stdout);
  }
  printf("[Thread %d] Waiting for messages...\n", tdL.idThread);
  fflush(stdout);
  pthread_detach(pthread_self());
  handleLogin((struct thData *)arg, db);

  while (true)
  {
    char command[100];
    int bytesRead = read(((thData *)arg)->cl, &command, sizeof(int));

    printf("\n[Thread %d] Received command: %s\n\n", tdL.idThread, command);
    if (strcmp(command, "quit") == 0)
    {
      close(((thData *)arg)->cl);
      printf("[Thread %d] Client disconnected.\n", tdL.idThread);
      fflush(stdout);
      return (NULL);
    }
    if (strcmp(command, "1") == 0)
    {
      printf("[Thread %d] Received command 1. Getting all users...\n", tdL.idThread);
      fflush(stdout);
      char * userList = getAllUsers(db);
      if (userList == NULL)
      {
        printf("[Thread %d] Failed to receive userList.\n", tdL.idThread);
        fflush(stdout);
        write(tdL.cl, "Failed to receive userList.", sizeof("Failed to receive userList."));
      }
      else
      {
        printf("[Thread %d] User list received: %s\n", tdL.idThread, userList);
        fflush(stdout);
        printf("[Thread %d] length of userList = %ld\n", tdL.idThread, strlen(userList));

        size_t userListLength = strlen(userList);
        if(write(tdL.cl, &userListLength, sizeof(userListLength))<0)
        {
          perror("[Thread] Error at write(userListLength) to the client.\n");
          free(userList);
          return 0;
        }
        if(write(tdL.cl, userList, userListLength)<0)
        {
          perror("[Thread] Error at write(userList) to the client.\n");
          free(userList);
          return 0;
        }
        free(userList);
      }
    }
    else if (bytesRead <= 0)
    {
      printf("[Thread %d] Client disconnected or encountered an error.\n", tdL.idThread);
      fflush(stdout);
      break;
    }
    else
    {
      printf("[Thread %d] Received an unknown command from this client.\n", tdL.idThread);
      fflush(stdout);
    }
  }
  close((intptr_t)arg);
  return (NULL);
};

void handleLogin(void *arg, sqlite3 *db)
{
  char username[100];
  memset(username, 0, sizeof(username));
  char password[100];
  memset(password, 0, sizeof(password));
  char *err_msg = 0;
  struct thData tdL;
  tdL = *((struct thData *)arg);

  if (read(tdL.cl, username, sizeof(username)) <= 0)
  {
    printf("[Thread %d]\n", tdL.idThread);
    fflush(stdout);
    perror("Error at read(username) from client.\n");
    return;
  }

  printf("[Thread %d] Username received: %s\n", tdL.idThread, username);
  fflush(stdout);

  sqlite3_stmt *stmt;
  // string modified_username = "'" + string(username) + "'";
  const char *sql = "SELECT username FROM users WHERE username = ?";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
  {
    fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
    return;
    fflush(stdout);
  }

  // printf("strlen(username) = %ld\n", strlen(username));
  if (sqlite3_bind_text(stmt, 1, username, strlen(username) - 1, SQLITE_STATIC) != SQLITE_OK)
  {
    fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
    fflush(stdout);
    return;
  }

  // printf("am ajuns aici\n");
  int rc = sqlite3_step(stmt);
  int usernameExists = rc == SQLITE_ROW;

  // printf("[Thread %d] sqlite3_step returned: %d\n", tdL.idThread, rc);
  if (usernameExists && userIsLoggedIn(username))
  {
    printf("[Thread %d] User is already logged in. Closing connection...\n", tdL.idThread);
    fflush(stdout);
    char usernameConfirmation[] = "User is already logged in.";
    if (write(tdL.cl, usernameConfirmation, sizeof(usernameConfirmation)) <= 0)
    {
      printf("[Thread %d] ", tdL.idThread);
      fflush(stdout);
      perror("[Thread] Error at write(confirmation-username) to the client.\n");
    }
    else
    {
      printf("[Thread %d] Username confirmation message sent.\n", tdL.idThread);
      fflush(stdout);
    }
  }
  if (usernameExists)
  {
    printf("[Thread %d] Username exists.\n", tdL.idThread);
    fflush(stdout);
    char usernameConfirmation[] = "Username exists.";
    if (write(tdL.cl, usernameConfirmation, sizeof(usernameConfirmation)) <= 0)
    {
      printf("[Thread %d] ", tdL.idThread);
      fflush(stdout);
      perror("[Thread] Error at write(confirmation-username) to the client.\n");
    }
    else
    {
      printf("[Thread %d] Username confirmation message sent.\n", tdL.idThread);
      fflush(stdout);

      //  ---------- password -----------
      if (read(tdL.cl, password, sizeof(password)) <= 0)
      {
        printf("[Thread %d]\n", tdL.idThread);
        fflush(stdout);
        perror("Error at read(password) from client.\n");
        return;
      }

      printf("[Thread %d] Password received: %s\n", tdL.idThread, password);
      fflush(stdout);

      sqlite3_stmt *stmtPassword;
      const char *sqlPassword = "SELECT password FROM users WHERE username = ? AND password = ?";

      if (sqlite3_prepare_v2(db, sqlPassword, -1, &stmtPassword, 0) != SQLITE_OK)
      {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        return;
      }

      if (sqlite3_bind_text(stmtPassword, 1, username, strlen(username) - 1, SQLITE_STATIC) != SQLITE_OK)
      {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        return;
      }

      if (sqlite3_bind_text(stmtPassword, 2, password, strlen(password) - 1, SQLITE_STATIC) != SQLITE_OK)
      {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        return;
      }

      int rcPassword = sqlite3_step(stmtPassword);
      int passwordCorrect = rcPassword == SQLITE_ROW;

      if (passwordCorrect)
      {
        printf("[Thread %d] Password is correct. Logging in...\n", tdL.idThread);
        fflush(stdout);

        char passwordConfirmation[] = "Password is correct. Logging in...";
        if (write(tdL.cl, passwordConfirmation, sizeof(passwordConfirmation)) <= 0)
        {
          printf("[Thread %d] ", tdL.idThread);
          fflush(stdout);
          perror("[Thread] Error at write(confirmation-password) to the client.\n");
        }
        else
        {
          printf("[Thread %d] Password confirmation message sent.\n", tdL.idThread);
          fflush(stdout);
        }

        logUserIn(username, tdL.idThread);
      }
      else
      {
        printf("[Thread %d] Password is incorrect. Closing connection...\n", tdL.idThread);
        char passwordError[] = "Password is incorrect.";
        if (write(tdL.cl, passwordError, sizeof(passwordError)) <= 0)
        {
          printf("[Thread %d] ", tdL.idThread);
          perror("[Thread] Error at write(password-error) to the client.\n");
        }
        else
        {
          printf("[Thread %d] Password error message sent.\n", tdL.idThread);
        }
      }
      // ------------ /password --------------
    }
  }
  else
  {
    printf("[Thread %d] Username does not exist. Closing connection...\n", tdL.idThread);
    char usernameError[] = "Username does not exist.";
    if (write(tdL.cl, usernameError, sizeof(usernameError)) <= 0)
    {
      printf("[Thread %d] ", tdL.idThread);
      perror("[Thread] Error at write(username-error) to the client.\n");
    }
    else
    {
      printf("[Thread %d] Username error message sent.\n", tdL.idThread);
    }
  }

  sqlite3_finalize(stmt);
}

void initializeDatabase(sqlite3 *db)
{

  char *err_msg = 0;
  const char *createUsersTableQuery = "BEGIN TRANSACTION;"
                                      "PRAGMA foreign_keys = ON;"
                                      "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT NOT NULL UNIQUE, password TEXT NOT NULL);"
                                      "INSERT OR IGNORE INTO users (username, password) VALUES ('a', 'a');"
                                      "INSERT OR IGNORE INTO users (username, password) VALUES ('b', 'b');"
                                      "INSERT OR IGNORE INTO users (username, password) VALUES ('c', 'c');"
                                      "INSERT OR IGNORE INTO users (username, password) VALUES ('d', 'd');"
                                      "INSERT OR IGNORE INTO users (username, password) VALUES ('e', 'e');"
                                      "INSERT OR IGNORE INTO users (username, password) VALUES ('f', 'f');"
                                      "INSERT OR IGNORE INTO users (username, password) VALUES ('ana', 'ana');"
                                      "COMMIT;";
  int rc = sqlite3_exec(db, createUsersTableQuery, 0, 0, &err_msg);

  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    exit(1);
  }

  const char *createConversationsTableQuery = "CREATE TABLE IF NOT EXISTS conversations (id INTEGER PRIMARY KEY AUTOINCREMENT, user1 TEXT NOT NULL, user2 TEXT NOT NULL, conversationName TEXT NOT NULL);";

  rc = sqlite3_exec(db, createConversationsTableQuery, 0, 0, &err_msg);

  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    exit(1);
  }
}

void createConversationTable(sqlite3 *db, char *user1, char *user2, char *conversationName)
{
  char *err_msg = 0;
  string createConversationTableQuery = string("CREATE TABLE IF NOT EXISTS ") + conversationName + " (id INTEGER PRIMARY KEY AUTOINCREMENT, message TEXT NOT NULL, sender TEXT NOT NULL, readByReceiver INTEGER DEFAULT 0, replyTo INTEGER, Timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, FOREIGN KEY (Sender) REFERENCES users(username), FOREIGN KEY (replyTo) REFERENCES " + conversationName + "(id));";
  int rc = sqlite3_exec(db, createConversationTableQuery.c_str(), 0, 0, &err_msg);

  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    exit(1);
  }
}

static int callbackPrintUsers(void *NotUsed, int argc, char **argv, char **azColName)
{
  printf("Username = %s\n", argv[0] ? argv[0] : "NULL");
  return 0;
}

void logUserIn(char *username, int threadId)
{
  for (auto usr = loggedUsers.begin(); usr != loggedUsers.end(); usr++)
  {
    if (strcmp(usr->first, username) == 0 && usr->second == false)
    {
      usr->second = true;
      return;
    }
  }
  loggedUsers.insert(pair<char *, bool>(username, true));
}

bool userIsLoggedIn(char *username)
{
  for (auto usr = loggedUsers.begin(); usr != loggedUsers.end(); usr++)
  {
    if (strcmp(usr->first, username) == 0 && usr->second == true)
    {
      return true;
    }
  }
  return false;
}

char *getAllUsers(sqlite3 *db)
{
  char *err_msg = 0;
  char *users = NULL;
  const char *sql = "SELECT username FROM users;";
  int rc = sqlite3_exec(db, sql, callbackGetAllUsers, &users, &err_msg);

  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "Failed to select data\n");
    fprintf(stderr, "SQL error: %s\n", err_msg);
    fflush(stdout);

    sqlite3_free(err_msg);
    sqlite3_close(db);

    return NULL;
  }
  return users;
}

int callbackGetAllUsers(void *data, int argc, char **argv, char **azColName)
{
  char **users = static_cast<char **>(data);
  for (int i = 0; i < argc; i++)
  {
    if (*users == nullptr)
      *users = strdup(argv[i]);
    else
    {
      *users = static_cast<char*>(realloc(*users, strlen(*users) + strlen(argv[i]) + 2));
      strcat(*users, " ");
      strcat(*users, argv[i]);
    }
  }
  return 0;
}
// g++ server.cpp -o server -pthread -std=c++11 -lstdc++ -lsqlite3