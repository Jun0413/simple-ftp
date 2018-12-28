#include "mftputil.h"

void error_exit(char *message)
{
    perror(message);
    exit(1);
}

int create_socket(int port)
{
    int lstnsocket;
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;

    if ((lstnsocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        error_exit("socket() fails");
    }

    // for address reuse
    if (setsockopt(lstnsocket, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) < 0)
    {
        close(lstnsocket);
        error_exit("setsockopt() fails");
    }

    if (bind(lstnsocket, (struct sockaddr *) &address, sizeof(address)) < 0)
    {
        close(lstnsocket);
        error_exit("bind() fails");
    }

    if (listen(lstnsocket, MAX_PENDING) < 0)
    {
        close(lstnsocket);
        error_exit("listen() fails");
    }

    return lstnsocket;
}

void strtocmd(char *str, Command *cmd)
{
    memset(cmd->command, 0, sizeof(cmd->command));
    memset(cmd->arg, 0, sizeof(cmd->arg));

    char *p = strtok(str, " ");
    p = strtok(NULL, " ");

    strcpy(cmd->command, str);
    if (p != NULL)
    {
        strncpy(cmd->arg, p, strlen(p));
    }
}

void read_send_file(char *data, int size, int datasock, FILE *fp)
{
    size_t bytes_read;
    memset(data, 0, size);
    while ((bytes_read = fread(data, 1, size, fp)) > 0)
    {
        if (send(datasock, data, bytes_read, 0) < 0)
        {
            perror("fail to send data");
            break;
        }
        memset(data, 0, size);
    }
}

void recv_save_file(char *data, int size, int datasock, FILE *fp)
{
    memset(data, 0, MAX_BUF_SIZE);
    ssize_t bytes_rcvd;
    while ((bytes_rcvd = recv(datasock, data, size, 0)) > 0)
    {
        fwrite(data, 1, bytes_rcvd, fp);
        memset(data, 0, MAX_BUF_SIZE);
    }

    if (bytes_rcvd < 0)
    {
        perror("fail to receive file");
    }
}