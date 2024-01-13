#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <unordered_set>

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
int handleLogin(void *, sqlite3 *db);
void initializeDatabase(sqlite3 *db);
static int callbackPrintUsers(void *data, int argc, char **argv, char **azColName);
static int callbackUsernameExists(void *data, int argc, char **argv, char **azColName);
char *createConversationTable(sqlite3 *db, char *user1, char *user2);
void logUserIn(char *username);
void logUserOut(char *username);
bool userIsLoggedIn(char *username);
char *getAllUsers(sqlite3 *db);
int callbackGetAllUsers(void *data, int argc, char **argv, char **azColName);
char *getAllLoggedUsers();
int writePlusSize(int id_thread, int sd, const char *message);
int readPlusSize(int id_thread, int sd, char *message, int bufferSize);
int readSize(int id_thread, int sd);
char *itoa(int num, char str[], int base);
void reverse(char str[], int length);
int userExists(sqlite3 *db, char *username);
int isPasswordCorrect(sqlite3 *db, char *username, char *password);
void addMessageToConversation(sqlite3 *db, char *sender, char *receiver, char *message, char *conversationName);
int command4Handling(thData tdL, sqlite3 *db, char *username);
int command3Handling(thData tdL, sqlite3 *db, char *username);
bool tableExists(sqlite3 *db, const char *tableName);
int retrieveConversation(sqlite3 *db, thData tdL, char *conversationName, char *username);

int main()
{
  struct sockaddr_in server;
  struct sockaddr_in from;
  int sd;
  int pid;
  pthread_t th[12];
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

  // const char *sql = "SELECT username FROM users;";
  // rc = sqlite3_exec(db, sql, callbackPrintUsers, 0, &err_msg);

  // if (rc != SQLITE_OK)
  // {
  //   fprintf(stderr, "Failed to select data\n");
  //   fprintf(stderr, "SQL error: %s\n", err_msg);
  //   fflush(stdout);

  //   sqlite3_free(err_msg);
  //   sqlite3_close(db);

  //   return 1;
  // }

  // ------------------ no more database --------------------

  if (listen(sd, 10) == -1)
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
    close(tdL.cl);
    exit(1);
  }
  else
  {
    printf("\n[Thread %d] Opened database successfully\n", tdL.idThread);
    fflush(stdout);
  }

  printf("[Thread %d] Waiting for messages...\n\n", tdL.idThread);
  fflush(stdout);
  pthread_detach(pthread_self());

  if (handleLogin((struct thData *)arg, db) < 0)
  {
    printf("[Thread %d] Error at handleLogin.\n", tdL.idThread);
    fflush(stdout);
    close(tdL.cl);
    sqlite3_close(db);
    return NULL;
  }
  // --------- read username from client --------
  char username[100];
  memset(username, 0, 100);
  if (readPlusSize(tdL.idThread, tdL.cl, username, 100) < 0)
  {
    printf("[Thread %d] Error at read(username) from client.\n", tdL.idThread);
    fflush(stdout);
    close(tdL.cl);
    sqlite3_close(db);
    return NULL;
  }
  // -----------------------------------------------

  while (true)
  {
    char command[100];
    memset(command, 0, 100);
    if (readPlusSize(tdL.idThread, tdL.cl, command, 100) < 0)
    {
      printf("[Thread %d] Error at read(command) from client.\n", tdL.idThread);
      fflush(stdout);
      logUserOut(username);
      close(tdL.cl);
      sqlite3_close(db);
      return NULL;
    }

    /// TODO: refresh / nullify command so it doesn't stay the same throughout the loop

    printf("\n[Thread %d] Received command: %s\n\n", tdL.idThread, command);
    fflush(stdout);

    if (strcmp(command, "quit") == 0)
    {
      logUserOut(username);
      close(tdL.cl);
      sqlite3_close(db);
      printf("[Thread %d] Client disconnected.\n", tdL.idThread);
      fflush(stdout);
      return (NULL);
    }
    else if (strcmp(command, "1") == 0)
    {
      printf("[Thread %d] Received command 1. Getting all users...\n", tdL.idThread);
      fflush(stdout);
      char *userList = getAllUsers(db);
      if (userList == NULL)
      {
        printf("[Thread %d] Failed to receive userList.\n", tdL.idThread);
        fflush(stdout);
        if (writePlusSize(tdL.idThread, tdL.cl, "Failed to receive userList.") < 0)
        {
          close(tdL.cl);
          sqlite3_close(db);
        }
      }
      else
      {
        // printf("[Thread %d] User list received: %s\n", tdL.idThread, userList);
        // fflush(stdout);
        // printf("[Thread %d] length of userList = %ld\n", tdL.idThread, strlen(userList));

        writePlusSize(tdL.idThread, tdL.cl, userList);
      }
    }
    else if (strcmp(command, "2") == 0)
    {
      printf("[Thread %d] Received command 2. Getting all logged users...\n", tdL.idThread);
      fflush(stdout);
      char *loggedUserList = getAllLoggedUsers();
      if (loggedUserList == NULL)
      {
        printf("[Thread %d] Failed to receive loggedUserList.\n", tdL.idThread);
        fflush(stdout);
        if (writePlusSize(tdL.idThread, tdL.cl, "Failed to receive loggedUserList.") < 0)
        {
          logUserOut(username);
          close(tdL.cl);
          sqlite3_close(db);
        }
      }
      else
      {
        writePlusSize(tdL.idThread, tdL.cl, loggedUserList);
      }
    }
    else if (strcmp(command, "3") == 0)
    {
      printf("[Thread %d] Received command 3. Waiting for user to send an username...\n", tdL.idThread);
      fflush(stdout);

      command3Handling(tdL, db, username);
      /// TODO: vezi la ce da return si da return aici la ce trebuie
    }
    else if (strcmp(command, "4") == 0)
    {
      printf("[Thread %d] Received command 4. Getting conversation with an user. Waiting for user...\n", tdL.idThread);

      if (command4Handling(tdL, db, username) < 0)
      {
        printf("[Thread %d] Error at command4Handling().\n", tdL.idThread);
        fflush(stdout);
        logUserOut(username);
        close(tdL.cl);
        sqlite3_close(db);
        return NULL;
      }

      /// TODO: vezi la ce se da return si da return aici la ce trebuie
    }
    else
    {
      printf("[Thread %d] Received an unknown command from this client.\n", tdL.idThread);
      logUserOut(username);
      close(tdL.cl);
      sqlite3_close(db);
      fflush(stdout);
    }
  }
  close((intptr_t)arg);
  return (NULL);
};

int handleLogin(void *arg, sqlite3 *db)
{
  char *err_msg = 0;
  struct thData tdL;
  tdL = *((struct thData *)arg);
  int sd = tdL.cl;
  int thread_no = tdL.idThread;

  char *username = (char *)calloc(100, sizeof(char));
  if (readPlusSize(thread_no, sd, username, 100) < 0)
  {
    return -1;
  }

  printf("[Thread %d] Username received: %s\n", tdL.idThread, username);
  fflush(stdout);

  int usernameExists = userExists(db, username);

  if (usernameExists < 0)
  {
    printf("[Thread %d] Error at userExists().\n", tdL.idThread);
    fflush(stdout);
    return usernameExists;
  }

  // printf("[Thread %d] sqlite3_step returned: %d\n", tdL.idThread, rc);
  if (usernameExists && userIsLoggedIn(username))
  {
    writePlusSize(thread_no, sd, "User is already logged in.");
    printf("[Thread %d] User is already logged in. Closing connection...\n", tdL.idThread);
    fflush(stdout);
    return -2;
  }
  else if (usernameExists)
  {
    printf("[Thread %d] Username exists.\n", tdL.idThread);
    fflush(stdout);
    if (writePlusSize(thread_no, sd, "Username exists.") < 0)
    {
      return -1;
    }
    else
    {
      printf("[Thread %d] Username confirmation message sent.\n\n", tdL.idThread);
      fflush(stdout);

      //  ---------- password -----------
      char *password = (char *)calloc(100, sizeof(char));
      if (readPlusSize(thread_no, sd, password, 100) < 0)
      {
        return -1;
      }

      printf("[Thread %d] Password received: %s\n", tdL.idThread, password);
      fflush(stdout);

      int passwordCorrect = isPasswordCorrect(db, username, password);

      if (passwordCorrect < 0)
      {
        printf("[Thread %d] Error at isPasswordCorrect().\n", tdL.idThread);
        fflush(stdout);
        return passwordCorrect;
      }

      if (passwordCorrect && !userIsLoggedIn(username))
      {
        printf("[Thread %d] Password is correct. Logging in...\n", tdL.idThread);
        fflush(stdout);

        if (writePlusSize(thread_no, sd, "Password is correct. Logging in...") < 0)
        {
          return -1;
        }
        else
        {
          printf("[Thread %d] Password confirmation message sent.\n\n", tdL.idThread);
          fflush(stdout);
        }
        if (userIsLoggedIn(username))
        {
          printf("[Thread %d] User is already logged in. Closing connection...\n", tdL.idThread);
          fflush(stdout);
          return -2;
        }
        else
        {
          logUserIn(username);
        }
      }
      else
      {
        printf("[Thread %d] Password is incorrect or user is already logged. Closing connection...\n", tdL.idThread);
        fflush(stdout);
        if (writePlusSize(thread_no, sd, "Password is incorrect or user is already logged in.") < 0)
        {
          return -1;
        }
        else
        {
          printf("[Thread %d] Password error message sent.\n", tdL.idThread);
          fflush(stdout);
          return -2;
        }
      }
      // ------------ /password --------------
    }
  }
  else
  {
    printf("[Thread %d] Username does not exist. Closing connection...\n", tdL.idThread);
    fflush(stdout);
    if (writePlusSize(thread_no, sd, "Username does not exist.") < 0)
    {
      return -1;
    }
    else
    {
      printf("[Thread %d] Username error message sent.\n", tdL.idThread);
      fflush(stdout);
      return -2;
    }
  }
  return 0;
}

void initializeDatabase(sqlite3 *db)
{
  char *err_msg = 0;
  const char *createUsersTableQuery =
      "BEGIN TRANSACTION;"
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

char *createConversationTable(sqlite3 *db, char *user1, char *user2)
{
  char *err_msg = 0;
  char *conversationName = (char *)calloc(200, sizeof(char));
  if (strcmp(user1, user2) < 0)
  {
    strcpy(conversationName, user1);
    strcat(conversationName, "_");
    strcat(conversationName, user2);
  }
  else
  {
    strcpy(conversationName, user2);
    strcat(conversationName, "_");
    strcat(conversationName, user1);
  }
  string createConversationTableQuery = string("CREATE TABLE IF NOT EXISTS ") + conversationName + " (id INTEGER PRIMARY KEY AUTOINCREMENT, message TEXT NOT NULL, sender VARCHAR(100) NOT NULL, readByReceiver INTEGER DEFAULT 0, replyTo INTEGER, Timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, FOREIGN KEY (sender) REFERENCES users(username), FOREIGN KEY (replyTo) REFERENCES " + conversationName + "(id));";
  int rc = sqlite3_exec(db, createConversationTableQuery.c_str(), 0, 0, &err_msg);

  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(db);
    exit(1);
  }
  return conversationName;
}

static int callbackPrintUsers(void *NotUsed, int argc, char **argv, char **azColName)
{
  printf("Username = %s\n", argv[0] ? argv[0] : "NULL");
  return 0;
}

void logUserIn(char *username)
{
  printf("[Server] Logging user %s in...\n", username);
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

void logUserOut(char *username)
{
  printf("[Server] Logging user %s out...\n", username);
  for (auto usr = loggedUsers.begin(); usr != loggedUsers.end(); usr++)
  {
    if (strcmp(usr->first, username) == 0 && usr->second == true)
    {
      usr->second = false;
      return;
    }
  }
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

int userExists(sqlite3 *db, char *username)
{
  sqlite3_stmt *stmt;
  // string modified_username = "'" + string(username) + "'";
  const char *sql = "SELECT username FROM users WHERE username = ?";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
  {
    fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
    fflush(stdout);
    return -1;
  }

  // printf("strlen(username) = %ld\n", strlen(username));
  if (sqlite3_bind_text(stmt, 1, username, strlen(username), SQLITE_STATIC) != SQLITE_OK)
  {
    fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
    fflush(stdout);
    return -1;
  }

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_ROW;
}

int isPasswordCorrect(sqlite3 *db, char *username, char *password)
{
  sqlite3_stmt *stmtPassword;
  const char *sqlPassword = "SELECT password FROM users WHERE username = ? AND password = ?";

  if (sqlite3_prepare_v2(db, sqlPassword, -1, &stmtPassword, 0) != SQLITE_OK)
  {
    fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
    return -3;
  }

  if (sqlite3_bind_text(stmtPassword, 1, username, strlen(username), SQLITE_STATIC) != SQLITE_OK)
  {
    fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
    return -3;
  }

  if (sqlite3_bind_text(stmtPassword, 2, password, strlen(password), SQLITE_STATIC) != SQLITE_OK)
  {
    fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
    return -3;
  }

  int rcPassword = sqlite3_step(stmtPassword);
  sqlite3_finalize(stmtPassword);
  return rcPassword == SQLITE_ROW;
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
      *users = static_cast<char *>(realloc(*users, strlen(*users) + strlen(argv[i]) + 2));
      strcat(*users, " ");
      strcat(*users, argv[i]);
    }
  }
  return 0;
}

char *getAllLoggedUsers()
{
  char *users = NULL;
  for (auto usr = loggedUsers.begin(); usr != loggedUsers.end(); usr++)
  {
    if (users == nullptr && usr->second == true)
      users = strdup(usr->first);
    else if (usr->second == true)
    {
      users = static_cast<char *>(realloc(users, strlen(users) + strlen(usr->first) + 2));
      strcat(users, " ");
      strcat(users, usr->first);
    }
  }
  return users;
}

int readSize(int id_thread, int sd)
{
  char size[10];
  memset(size, 0, 10);
  if (read(sd, size, 10) < 0)
  {
    fprintf(stderr, "[Thread %d] Error at read(length) from client.\n", id_thread);
    fflush(stdout);
    return -1;
  }
  // printf("[Thread %d] Message size: %s received\n", id_thread, size);
  int messageSize = atoi(size);
  return messageSize;
}

int readPlusSize(int id_thread, int sd, char *message, int bufferSize)
{
  int messageSize = readSize(id_thread, sd);
  if (messageSize <= 0)
  {
    return -1;
  }
  if (messageSize > bufferSize)
  {
    fprintf(stderr, "[Thread %d] Error: message size is bigger than buffer size at reading.\n", id_thread);
    fflush(stdout);
    return -1;
  }
  char temp[bufferSize + 1];
  if (read(sd, temp, bufferSize) < 0)
  {
    fprintf(stderr, "[Thread %d] Error at read(message) from client.\n", id_thread);
    fflush(stdout);
    return -1;
  }

  memset(message, 0, bufferSize);
  strncpy(message, temp, messageSize);

  // printf("[Thread %d] Message: %s received\n", id_thread, message);

  return 0;
}

int writePlusSize(int id_thread, int sd, const char *message)
{
  size_t messageSize = strlen(message);
  char size[10];
  memset(size, 0, sizeof(size));
  itoa(messageSize, size, 10);
  if (write(sd, size, strlen(size)) < 0)
  {
    fprintf(stderr, "[Thread %d] Error at write(length) to client.\n", id_thread);
    fflush(stdout);
    return -1;
  }

  sleep(1);

  if (write(sd, message, messageSize) < 0)
  {
    fprintf(stderr, "[Thread %d] Error at write(message) to client.\n", id_thread);
    fflush(stdout);
    return -1;
  }
  else
  {
    // printf("[Thread %d] Message: %s sent\n", id_thread, message);
    // fflush(stdout);
    return 0;
  }
  return 0;
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

void addMessageToConversation(sqlite3 *db, char *sender, char *receiver, char *message, char *conversationName)
{
  char *err_msg = 0;
  string insertMessageQuery = "INSERT INTO " + string(conversationName) + " (message, sender) VALUES (?, ?);";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, insertMessageQuery.c_str(), -1, &stmt, 0);

  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
    fflush(stdout);
    sqlite3_close(db);
    exit(1);
  }

  sqlite3_bind_text(stmt, 1, message, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, sender, -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);

  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE)
  {
    fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
    fflush(stdout);
    sqlite3_close(db);
    exit(1);
  }
}

int command4Handling(thData tdL, sqlite3 *db, char *username)
{
  char *otherUser = (char *)calloc(100, sizeof(char));
  if (readPlusSize(tdL.idThread, tdL.cl, otherUser, 100) < 0)
  {
    printf("[Thread %d] Error at read(otherUser) from client.\n", tdL.idThread);
    fflush(stdout);
    logUserOut(username);
    close(tdL.cl);
    sqlite3_close(db);
    return -1;
  }

  printf("[Thread %d] Received otherUser: %s\n", tdL.idThread, otherUser);
  fflush(stdout);

  int otherUsernameExists = userExists(db, otherUser);

  if (otherUsernameExists < 0)
  {
    printf("[Thread %d] Error at userExists().\n", tdL.idThread);
    fflush(stdout);
    logUserOut(username);
    close(tdL.cl);
    sqlite3_close(db);
    return otherUsernameExists;
  }
  else if (otherUsernameExists == 0)
  {
    printf("[Thread %d] Other user does not exist.\n", tdL.idThread);
    fflush(stdout);
    if (writePlusSize(tdL.idThread, tdL.cl, "Other user does not exist.") < 0)
    {
      logUserOut(username);
      close(tdL.cl);
      sqlite3_close(db);
      return -1;
    }
  }
  else
  {
    printf("[Thread %d] Other user exists.\n", tdL.idThread);
    fflush(stdout);
    if (writePlusSize(tdL.idThread, tdL.cl, "Other user exists. Fetching conversation...") < 0)
    {
      printf("[Thread %d] Error at writePlusSize().\n", tdL.idThread);
      fflush(stdout);
      logUserOut(username);
      close(tdL.cl);
      sqlite3_close(db);
      return -1;
    }
    else
    {
      printf("[Thread %d] Confirmation message sent. Conversation with %s is being fetched...\n", tdL.idThread, otherUser);
      fflush(stdout);

      char *conversationName = (char *)calloc(200, sizeof(char));
      if (strcmp(username, otherUser) < 0)
      {
        strcpy(conversationName, username);
        strcat(conversationName, "_");
        strcat(conversationName, otherUser);
      }
      else
      {
        strcpy(conversationName, otherUser);
        strcat(conversationName, "_");
        strcat(conversationName, username);
      }

      printf("[TEST] conversationName = %s\n", conversationName);

      // -------------- test --------------
      char *err_msg = 0;
      string selectMessagesQuery = "SELECT id, sender, message FROM " + string(conversationName) + " WHERE id = 2;";
      sqlite3_stmt *stmt;
      int rc = sqlite3_prepare_v2(db, selectMessagesQuery.c_str(), -1, &stmt, 0);
      if (rc != SQLITE_OK)
      {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        fflush(stdout);
        sqlite3_close(db);
        exit(1);
      }
      rc = sqlite3_step(stmt);
      if (rc == SQLITE_ROW)
      {
        int id = sqlite3_column_int(stmt, 0);
        char *sender = (char *)calloc(100, sizeof(char));
        strcpy(sender, (char *)sqlite3_column_text(stmt, 1));
        char *message = (char *)calloc(1000, sizeof(char));
        strcpy(message, (char *)sqlite3_column_text(stmt, 2));
        printf("[TEST] id = %d, sender = %s, message = %s\n", id, sender, message);
      }
      else
      {
        printf("[TEST] No row selected.\n");
        fflush(stdout);
      }
      // -------------- /test --------------

      if (tableExists(db, conversationName) == 1)
      {
        printf("[Thread %d] Conversation exists.\n", tdL.idThread);
        fflush(stdout);
        if (writePlusSize(tdL.idThread, tdL.cl, "Conversation exists.") < 0)
        {
          printf("[Thread %d] Error at writePlusSize().\n", tdL.idThread);
          fflush(stdout);
          return -1;
        }
        else
        {
          printf("[Thread %d] Confirmation message sent.\n", tdL.idThread);
          fflush(stdout);
          if (retrieveConversation(db, tdL, conversationName, username) < 0)
          {
            printf("[Thread %d] Error at retrieveConversation().\n", tdL.idThread);
            fflush(stdout);
            return -1;
          }
          else
          {
            printf("[Thread %d] Conversation retrieved.\n", tdL.idThread);
            fflush(stdout);
          }
          return 1;
        }
      }
      else
      {
        printf("[Thread %d] No conversation found.\n", tdL.idThread);
        fflush(stdout);
        if (writePlusSize(tdL.idThread, tdL.cl, "No conversation found.") < 0)
        {
          printf("[Thread %d] Error at writePlusSize().\n", tdL.idThread);
          fflush(stdout);
          logUserOut(username);
          close(tdL.cl);
          sqlite3_close(db);
          return -1;
        }
        return 1;
      }
    }
    return 1;
  }
  return 1;
}

int command3Handling(thData tdL, sqlite3 *db, char *username)
{
  char *otherUser = (char *)calloc(100, sizeof(char));
  if (readPlusSize(tdL.idThread, tdL.cl, otherUser, 100) < 0)
  {
    printf("[Thread %d] Error at read(otherUser) from client.\n", tdL.idThread);
    fflush(stdout);
    logUserOut(username);
    close(tdL.cl);
    sqlite3_close(db);
    return -1;
  }

  if (!userExists(db, otherUser))
  {
    printf("[Thread %d] Other user does not exist.\n", tdL.idThread);
    fflush(stdout);
    if (writePlusSize(tdL.idThread, tdL.cl, "Other user does not exist.") < 0)
    {
      logUserOut(username);
      close(tdL.cl);
      sqlite3_close(db);
      return -2;
    }
  }
  else
  {
    printf("[Thread %d] Other user exists.\n", tdL.idThread);
    fflush(stdout);
    if (writePlusSize(tdL.idThread, tdL.cl, "Other user exists. Send message!") < 0)
    {
      printf("[Thread %d] Error at writePlusSize().\n", tdL.idThread);
      fflush(stdout);
      logUserOut(username);
      close(tdL.cl);
      sqlite3_close(db);
      return -1;
    }
    else
    {
      printf("[Thread %d] Confirmation message sent.  Waiting for message from %s to %s...\n", tdL.idThread, username, otherUser);
      fflush(stdout);

      char *message = (char *)calloc(1000, sizeof(char));
      if (readPlusSize(tdL.idThread, tdL.cl, message, 1000) < 0)
      {
        printf("[Thread %d] Error at read(message) from client.\n", tdL.idThread);
        fflush(stdout);
        logUserOut(username);
        close(tdL.cl);
        sqlite3_close(db);
        return -1;
      }

      printf("[Thread %d] Message received: %s\n", tdL.idThread, message);
      fflush(stdout);

      char *conversationName = (char *)calloc(200, sizeof(char));

      printf("[Thread %d] Creating conversation table between %s and %s...\n", tdL.idThread, username, otherUser);

      conversationName = createConversationTable(db, username, otherUser);

      printf("[Thread %d] Conversation table created: %s\n", tdL.idThread, conversationName);
      fflush(stdout);

      addMessageToConversation(db, username, otherUser, message, conversationName);

      printf("[Thread %d] Message added to conversation.\n", tdL.idThread);
      fflush(stdout);

      if (writePlusSize(tdL.idThread, tdL.cl, "Message sent!") < 0)
      {
        printf("[Thread %d] Error at writePlusSize().\n", tdL.idThread);
        fflush(stdout);
        logUserOut(username);
        close(tdL.cl);
        sqlite3_close(db);
        return -1;
      }
      else
      {
        printf("[Thread %d] Confirmation message sent.\n", tdL.idThread);
        fflush(stdout);
      }
    }
  }
  return 1;
}

bool tableExists(sqlite3 *db, const char *tableName)
{
  std::string query = "SELECT name FROM sqlite_master WHERE type='table' AND name = ?;";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, 0);

  if (rc != SQLITE_OK)
  {
    std::cerr << "SQL error: " << sqlite3_errmsg(db) << std::endl;
    return false;
  }

  sqlite3_bind_text(stmt, 1, tableName, -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);

  sqlite3_finalize(stmt);

  return (rc == SQLITE_ROW);
}

int retrieveConversation(sqlite3 *db, thData tdL, char *conversationName, char *username)
{
  char *err_msg = 0;
  string selectMessagesQuery = "SELECT id, sender, message, replyTo FROM " + string(conversationName) + ";";

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, selectMessagesQuery.c_str(), -1, &stmt, 0);

  if (rc != SQLITE_OK)
  {
    fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
    fflush(stdout);
    sqlite3_close(db);
    return -1;
  }

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
  {
    int id = sqlite3_column_int(stmt, 0);
    const char *sender = (const char *)sqlite3_column_text(stmt, 1);
    const char *message = (const char *)sqlite3_column_text(stmt, 2);
    int replyTo = sqlite3_column_int(stmt, 3);

    char idSenderMessage[1200] = {0};

    snprintf(idSenderMessage, 1200, "%d %s %s %d", id, sender, message, replyTo);
    printf("[Thread %d] idSenderMessage = %s\n", tdL.idThread, idSenderMessage);

    if (writePlusSize(tdL.idThread, tdL.cl, idSenderMessage) < 0)
    {
      fprintf(stderr, "[Thread %d] Error at writePlusSize().\n", tdL.idThread);
      fflush(stdout);
      logUserOut(username);
      close(tdL.cl);
      sqlite3_finalize(stmt);
      sqlite3_close(db);
      return -1;
    }
    sleep(1);
  }

  if (writePlusSize(tdL.idThread, tdL.cl, "end") < 0)
  {
    printf("[Thread %d] Error at writePlusSize().\n", tdL.idThread);
    fflush(stdout);
    logUserOut(username);
    close(tdL.cl);
    sqlite3_close(db);
    return -1;
  }

  printf("[Thread %d] end message sent.\n", tdL.idThread);

  sqlite3_finalize(stmt);
  return 1;
}

// g++ server.cpp -o server -pthread -std=c++11 -lstdc++ -lsqlite3