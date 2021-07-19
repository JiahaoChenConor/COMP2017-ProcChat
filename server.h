#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#define CONNECT 0
#define SAY 1
#define SAYCONT 2
#define RECEIVE 3
#define RECVCONT 4
#define PING 5
#define PONG 6
#define DISCONNCT 7
#define TYPE_PADDING 0

void set_path(char* folder, char* identifier, char* file_name, char* path, int type);

int deleteFolder(char* domain);

int creatFolder(char *folder_name);

void str_copy_extra_termination(char* destination, char* resource, int len);

void copy_str_without_terminate(char* destination, char* resource, int len);

void send_RECEIVE_to_other_client_handler_in_same_domain(char* buffer_sent, char* domain, char* sender);





#endif
