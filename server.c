#include <pthread.h>

#include "mftputil.h"

int ftp_server_response(int ctrlsock, int res_code);
int authenticate_ftp_client(int ctrlsock);
int ftp_server_data_conn(int ctrlsock);

void ftp_server_dir(int ctrlsock, char *cmd);
void ftp_server_chdir(int ctrlsock, char *dir);
void ftp_server_get_file(int ctrlsock, char *fname);
void ftp_server_put_file(int ctrlsock, char *fname);

void *handle_ftp_client(void *ctrlsock); /* server runs in multi-thread */
// void handle_ftp_client(int ctrlsock); /* server runs in multi-proc */

// define access control
const char USER[MAX_BUF_SIZE] = "user";
const char PASS[MAX_BUF_SIZE] = "pass";

int main(int argc, char const *argv[])
{   
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    // int pid, ctrlsock; /* decomment for multi-proc mode */
    int lstnsock = create_socket(port);
    if (lstnsock < 0)
    {
        close(lstnsock);
        error_exit("fail to create listening socket");
    }

    // run server
    while (1)
    {
        /*** multi-thread mode ***/
        int *ctrlsock = (int *) malloc(sizeof(int));
        if ((*ctrlsock = accept(lstnsock, NULL, NULL)) < 0)
            break;
        pthread_t pid;
        pthread_create(&pid, NULL, handle_ftp_client, (void *) ctrlsock);

        /*** ALTERNATIVE: multi-process mode ***/
        /*
        if ((ctrlsock = accept(lstnsock, NULL, NULL)) < 0)
        {
            break;
        }
        if ((pid = fork()) < 0)
        {
            close(ctrlsock);
            error_exit("fail to fork child process");
        }
        else if (pid == 0)
        {
            close(lstnsock); //TODO: check
            handle_ftp_client(ctrlsock);
            close(ctrlsock);
            exit(0);
        }
        close(ctrlsock);
        */
    }

    close(lstnsock);
    return 0;
}

/**
 * Provides services to a client
 * @param _ctrlsock Pointer to socket for commands
 */
void *handle_ftp_client(void *_ctrlsock)
{
    int ctrlsock = *(int *)_ctrlsock;
    
    // inform client that service is ready
    ftp_server_response(ctrlsock, CODE_SERVICE_READY);

    // recv usr & pwd and authenticate
    if (authenticate_ftp_client(ctrlsock))
        ftp_server_response(ctrlsock, CODE_USR_LOGGED_IN);
    else
    {
        ftp_server_response(ctrlsock, CODE_INVALID_USR);
        close(ctrlsock);
        return NULL;
    }

    char cmd_buffer[MAX_BUF_SIZE];
    Command cmd;
    
    while (1)
    {
        memset(cmd_buffer, 0, MAX_BUF_SIZE);    
        if (recv(ctrlsock, cmd_buffer, sizeof(cmd_buffer), 0) < 0)
        {
            perror("fail to receive command");
            continue;
        }

        strtocmd(cmd_buffer, &cmd); // note: cmd_buffer tokenized
        printf("Command received: %s %s\n", cmd.command, cmd.arg);

        // server operates & responds as per command
        if (strcmp(cmd.command, "put") == 0)
            ftp_server_put_file(ctrlsock, cmd.arg);

        else if (strcmp(cmd.command, "get") == 0)
            ftp_server_get_file(ctrlsock, cmd.arg);

        else if (strcmp(cmd.command, "ls") == 0 || strcmp(cmd.command, "pwd") == 0)
            ftp_server_dir(ctrlsock, cmd.command);

        else if (strcmp(cmd.command, "cd") == 0)
            ftp_server_chdir(ctrlsock, cmd.arg);          

        else if (strcmp(cmd.command, "quit") == 0)
        {
            if (ftp_server_response(ctrlsock, CODE_SERVICE_CLOSE_CTRL) < 0)
            {
                close(ctrlsock);
                return NULL;
            }

            break;
        }
        // invalid commands were intercepted at client side
        // except for SIGINT (ctrl + C)
        else
            break;
    }

    close(ctrlsock);
    printf("Client disconnected\n");
    return NULL;
}

/**
 * Sends response code to client via socket for commands
 * @param ctrlsock Socket for commands
 * @param res_code Response code
 * @return success or not
 */ 
int ftp_server_response(int ctrlsock, int res_code)
{
    int _res_code = htonl(res_code);
    if (send(ctrlsock, &_res_code, sizeof(_res_code), 0) < 0)
    {
        perror("fail to send response code\n");
        return -1;
    }
    return 0;
}

/**
 * Runs authentication process
 * @param ctrlsock Socket for commands
 * @return whether user is authenticated
 */ 
int authenticate_ftp_client(int ctrlsock)
{
    char buffer[MAX_BUF_SIZE];
    Command login_cmd;
    memset(buffer, 0, MAX_BUF_SIZE);
    int vald_usrname, vald_password;

    // recv username command to buffer
    if (recv(ctrlsock, buffer, sizeof(buffer), 0) < 0)
        error_exit("fail to receive username");

    // get username into struct cmd and validate
    strtocmd(buffer, &login_cmd);
    vald_usrname = strcmp(login_cmd.arg, USER);

    // ask client for password
    if (ftp_server_response(ctrlsock, CODE_NEED_PASS) < 0)
        error_exit("fail to ask for password");
    
    // repeat same procedure
    memset(buffer, 0, MAX_BUF_SIZE);
    if (recv(ctrlsock, buffer, sizeof(buffer), 0) < 0)
        error_exit("fail to receive username");
   
    strtocmd(buffer, &login_cmd);
    vald_password = strcmp(login_cmd.arg, PASS);

    return (vald_usrname == 0) && (vald_password == 0);
}

/**
 * Connects to client's data port using address in command socket
 * @param ctrlsock Socket for commands
 * @return sockets for data, -1 if failed
 */ 
int ftp_server_data_conn(int ctrlsock)
{
    char buffer[1024];
    int datasock;

    // retrieve address info from ctrlsock
    struct sockaddr_in clntaddr;
    socklen_t clntaddrlen = sizeof(clntaddr);
    getpeername(ctrlsock, (struct sockaddr *) &clntaddr, &clntaddrlen);
    
    // convert network addr to string
    inet_ntop(AF_INET, &clntaddr.sin_addr.s_addr, buffer, sizeof(buffer));

    // create socket for data
    memset(&clntaddr, 0, sizeof(clntaddr));
    clntaddr.sin_family = AF_INET;
    clntaddr.sin_port = htons(CLIENT_DATA_PORT);
    clntaddr.sin_addr.s_addr = inet_addr(buffer);
    if ((datasock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("fail to create data socket");
        return -1;
    }

    // wait for ack from client who is opening data port
    if (recv(ctrlsock, &(int) {1}, sizeof(int), 0) < 0)
    {
        perror("fail to receive ack from client");
        return -1;
    }

    // only initiate connection after ack received
    if (connect(datasock, (struct sockaddr *) &clntaddr, sizeof(clntaddr)) < 0)
    {
        perror("fail to connect to data port");
        return -1;
    }

    return datasock;
}

/**
 * Runs commands: ls, pwd
 * @param ctrlsock Socket for commands
 * @param cmd String command
 */ 
void ftp_server_dir(int ctrlsock, char *cmd)
{
    int datasock;
    FILE *output_stream;
    char output_buffer[MAX_BUF_SIZE];
    memset(output_buffer, 0, MAX_BUF_SIZE);

    // check if command can be executed
    if ((output_stream = popen(cmd, "r")) == NULL)
    {
        perror("fail to execute command");
        ftp_server_response(ctrlsock, CODE_CMD_NOT_IMPL);
        return;
    }

    // tells client to open data port
    ftp_server_response(ctrlsock, CODE_OPEN_DATA_CONN);

    // connects to data port
    if ((datasock = ftp_server_data_conn(ctrlsock)) < 0)
    {
        close(datasock);
        pclose(output_stream);
        return;
    }

    // load stdout to buffer and send
    while (fgets(output_buffer, MAX_BUF_SIZE, output_stream) != NULL)
    {
        if (send(datasock, output_buffer, strlen(output_buffer), 0) < 0)
        {
            perror("fail to send data");
            break;
        }
        memset(output_buffer, 0, MAX_BUF_SIZE);
    }

    pclose(output_stream);
    close(datasock);
}

/**
 * Runs command "cd <directory>"
 * @param ctrlsock Socket for commands
 * @param dir String directory
 */ 
void ftp_server_chdir(int ctrlsock, char *dir)
{
    if (chdir(dir) < 0)
    {
        perror("fail to execute command");
        ftp_server_response(ctrlsock, CODE_CMD_NOT_IMPL);
    }
    else
        ftp_server_response(ctrlsock, CODE_VALID_CMD);

}

/**
 * Sends file to client
 * @param ctrlsock Socket for commands
 * @param fname String file name
 */ 
void ftp_server_get_file(int ctrlsock, char *fname)
{
    FILE *fp;
    char data[MAX_BUF_SIZE];
    memset(data, 0, MAX_BUF_SIZE);

    // check whether file exists
    fp = fopen(fname, "r");
    if (!fp)
    {
        ftp_server_response(ctrlsock, CODE_FILE_UNAVAIL);
        return;
    }

    // open data connection
    ftp_server_response(ctrlsock, CODE_OPEN_DATA_CONN);
    int datasock;
    if ((datasock = ftp_server_data_conn(ctrlsock)) < 0)
    {
        close(datasock);
        return;
    }

    // read file and send
    read_send_file(data, MAX_BUF_SIZE, datasock, fp);

    // close
    close(datasock);
    fclose(fp);
    ftp_server_response(ctrlsock, CODE_CLOSE_DATA_CONN);
}

/**
 * Saves file from client
 * @param ctrlsock Socket for commands
 * @param fname String file name
 */ 
void ftp_server_put_file(int ctrlsock, char *fname)
{
    // check whether fname exists
    FILE *fp = fopen(fname, "r");
    if (fp != NULL)
    {
        // does not allow put existing file
        // TODO: update policy
        ftp_server_response(ctrlsock, CODE_CMD_BAD_SEQ);
        fclose(fp);
        return;
    }

    // open data connection
    ftp_server_response(ctrlsock, CODE_OPEN_DATA_CONN);
    int datasock;
    if ((datasock = ftp_server_data_conn(ctrlsock)) < 0)
    {
        close(datasock);
        return;
    }

    // receive and write to file
    fclose(fp);
    fp = fopen(fname, "w");
    char data[MAX_BUF_SIZE];
    recv_save_file(data, MAX_BUF_SIZE, datasock, fp);

    // close
    close(datasock);
    fclose(fp);
    ftp_server_response(ctrlsock, CODE_CLOSE_DATA_CONN);
}