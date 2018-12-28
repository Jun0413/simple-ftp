#ifndef MFTPUTIL_H
#define MFTPUTIL_H

/* response code */
#define CODE_SERVICE_READY 220
#define CODE_NEED_PASS 331
#define CODE_USR_LOGGED_IN 230
#define CODE_INVALID_USR 430
#define CODE_SERVICE_CLOSE_CTRL 221
#define CODE_VALID_CMD 250
#define CODE_CMD_NOT_IMPL 502
#define CODE_OPEN_DATA_CONN 150
#define CODE_FILE_UNAVAIL 550
#define CODE_CLOSE_DATA_CONN 226
#define CODE_CMD_BAD_SEQ 503

#define MAX_BUF_SIZE 512
#define MAX_PENDING 5
#define CLIENT_DATA_PORT 10240

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct Command
{
    char command[5];
    char arg[MAX_BUF_SIZE];
} Command;

/**
 * Die with error
 * @param message Error message
 */
void error_exit(char *message);

/**
 * Create a listening socket
 * @param port Port
 * @return socket
 */
int create_socket(int port);

/**
 * Convert string to struct command
 * @param str String
 * @param cmd Pointer to struct command
 */ 
void strtocmd(char *str, Command *cmd);

/**
 * Read from file and send via data socket
 * @param data Buffer
 * @param size Buffer size
 * @param datasock Socket for data
 * @param fp Pointer to file to read
 */ 
void read_send_file(char *data, int size, int datasock, FILE *fp);

/**
 * Receive to buffer and save to file via data socket
 * @param data Buffer
 * @param size Buffer size
 * @param datasock Socket for data
 * @param fp Pointer to file to save
 */ 
void recv_save_file(char *data, int size, int datasock, FILE *fp);

#endif