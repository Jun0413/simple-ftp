#include "mftputil.h"

int get_response_code(int ctrlsock);
void print_response(int res_code);

void ftp_client_login(int ctrlsock);
int ftp_client_give_command(int ctrlsock, Command *cmd);
int ftp_client_get_command(char *buffer, Command *cmd);
int ftp_client_data_conn(int ctrlsock);

void ftp_client_get_file(int ctrlsock, Command *cmd);
void ftp_client_put_file(int ctrlsock, Command *cmd);
void ftp_client_dir(int ctrlsock, Command *cmd);
void ftp_client_chdir(int ctrlsock, Command *cmd);

int main(int argc, char const *argv[])
{
    if (argc != 3)
    {   
        fprintf(stderr, "Usage: %s <server ip> <port>\n", argv[0]);
        exit(1);
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    
    // create socket for commands and connect to server
    int ctrlsock;
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_aton(server_ip, (struct in_addr *) &server_addr.sin_addr.s_addr);

    if ((ctrlsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        close(ctrlsock);
        error_exit("fail to create controlling socket");
    }

    if (connect(ctrlsock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
    {
        close(ctrlsock);
        error_exit("fail to connect");
    }

    printf("%s connected\n", server_ip);
    print_response(get_response_code(ctrlsock));

    // try logining to server
    ftp_client_login(ctrlsock);
    print_response(get_response_code(ctrlsock));

    char input_buffer[MAX_BUF_SIZE];
    char output_buffer[MAX_BUF_SIZE];
    FILE *output_stream;
    Command cmd;
    char *rm1ch_ptr;

    while (1)
    {
        // get command
        if (ftp_client_get_command(input_buffer, &cmd) < 0)
        {
            printf("Invalid command\n");
            continue;
        }

        // 1. process locally run commands without functions
        // 2. send server commands with wrapped functions
        if (strcmp(cmd.command, "put") == 0)
            ftp_client_put_file(ctrlsock, &cmd);

        else if (strcmp(cmd.command, "get") == 0)
            ftp_client_get_file(ctrlsock, &cmd);

        else if (strcmp(cmd.command, "ls") == 0 || strcmp(cmd.command, "pwd") == 0)
            ftp_client_dir(ctrlsock, &cmd);

        else if (strcmp(cmd.command, "cd") == 0)
            ftp_client_chdir(ctrlsock, &cmd);

        else if (strcmp(cmd.command, "!ls") == 0 || strcmp(cmd.command, "!pwd") == 0)
        {
            // to remove 1st char '!' of cmd
            rm1ch_ptr = cmd.command;
            if ((output_stream = popen(++rm1ch_ptr, "r")) == NULL)
            {
                perror("fail to execute command");
                pclose(output_stream);
                continue;
            }
            
            while (fgets(output_buffer, MAX_BUF_SIZE, output_stream) != NULL)
            {
                printf("%s", output_buffer);
                memset(output_buffer, 0, MAX_BUF_SIZE);
            }

            pclose(output_stream);
            memset(output_buffer, 0, MAX_BUF_SIZE);
        }
        else if (strcmp(cmd.command, "!cd") == 0)
        {
            if (chdir(cmd.arg) < 0)
                perror("fail to change directory");
        }
        else if (strcmp(cmd.command, "quit") == 0)
        {
            ftp_client_give_command(ctrlsock, &cmd);
            int res_code = get_response_code(ctrlsock);
            if (res_code != CODE_SERVICE_CLOSE_CTRL)
            {
                printf("fail to get quitting response code\n");
                continue;
            }

            close(ctrlsock);
            print_response(res_code);
            printf("quitted\n");
            break;
        }
    }

    return 0;
}

/**
 * Receive response code from server
 * @param ctrlsock Socket for commands
 * @return host response code, -1 if failed
 */ 
int get_response_code(int ctrlsock)
{
    int res_code = -1;
    recv(ctrlsock, &res_code, sizeof(res_code), 0);
    return res_code == -1 ? res_code : ntohl(res_code);
}

/**
 * Interpret response code
 * @param res_code host response code
 */ 
void print_response(int res_code)
{
    switch(res_code)
    {
        case CODE_SERVICE_READY:
            printf("Service ready [%d]\n", CODE_SERVICE_READY);
            break;
        case CODE_USR_LOGGED_IN:
            printf("User logged in [%d]\n", CODE_USR_LOGGED_IN);
            break;
        case CODE_INVALID_USR:
            printf("Invalid credentials [%d]\n", CODE_INVALID_USR);
            exit(1);
        case CODE_CMD_NOT_IMPL:
            printf("Command cannot be executed [%d]\n", CODE_CMD_NOT_IMPL);
            break;
        case CODE_VALID_CMD:
            printf("Command OK [%d]\n", CODE_VALID_CMD);
            break;
        case CODE_FILE_UNAVAIL:
            printf("No such file on server [%d]\n", CODE_FILE_UNAVAIL);
            break;
        case CODE_CLOSE_DATA_CONN:
            printf("Finish data transmission [%d]\n", CODE_CLOSE_DATA_CONN);
            break;
        case CODE_CMD_BAD_SEQ:
            printf("Command has bad sequence [%d]\n", CODE_CMD_BAD_SEQ);
            break;
        case CODE_SERVICE_CLOSE_CTRL:
            printf("Close connection [%d]\n", CODE_SERVICE_CLOSE_CTRL);
            break;
        default:
            printf("Internal service error\n");
            exit(1);
    }
}

/**
 * Gets username & password and sends for validation
 * @param ctrlsock Socket for commands
 */ 
void ftp_client_login(int ctrlsock)
{
    Command cmd;
    char usrname[MAX_BUF_SIZE];
    memset(usrname, 0, MAX_BUF_SIZE);
    int res_code;

    printf("username: ");
    fflush(stdout);
    if (fgets(usrname, MAX_BUF_SIZE, stdin))
    {
        // replace new line with 0
        usrname[strcspn(usrname, "\n")] = '\0';
    }

    strcpy(cmd.command, "user");
    strcpy(cmd.arg, usrname);
    ftp_client_give_command(ctrlsock, &cmd);

    if (recv(ctrlsock, &res_code, sizeof(res_code), 0) < 0)
        error_exit("fail to receive response");

    if (ntohl(res_code) != CODE_NEED_PASS)
    {
        printf("internal service error");
        exit(1);
    }

    fflush(stdout);
    char *password = getpass("password: ");
    strcpy(cmd.command, "pass");
    strcpy(cmd.arg, password);
    ftp_client_give_command(ctrlsock, &cmd);
}

/**
 * Sends commands to server
 * @param ctrlsock Socket for commands
 * @param cmd Pointer to struct command
 * @return success or not
 */ 
int ftp_client_give_command(int ctrlsock, Command *cmd)
{
    char buffer[MAX_BUF_SIZE];
    memset(buffer, 0, MAX_BUF_SIZE);
    sprintf(buffer, "%s %s", cmd->command, cmd->arg);
    if (send(ctrlsock, buffer, sizeof(buffer), 0) < 0)
    {
        perror("fail to command");
        return -1;
    }
    return 0;
}

/**
 * Gets user input and validates command
 * @param buffer String to save valid command
 * @param cmd Pointer to struct command to hold valid one
 * @return valid or not
 */
int ftp_client_get_command(char *buffer, Command *cmd)
{
    memset(buffer, 0, MAX_BUF_SIZE);
    memset(cmd->command, 0, sizeof(cmd->command));
    memset(cmd->arg, 0, sizeof(cmd->arg));
    
    printf("mftp> ");
    fflush(stdout);

    if (fgets(buffer, MAX_BUF_SIZE, stdin) == NULL)
    {
        // stdin is EOF and closed
        // quit the program
        strcpy(cmd->command, "quit");
        return 0;       
    }
    else
        buffer[strcspn(buffer, "\n")] = '\0';

    // TODO: not ideal user input
    char *p = NULL;
    p = strtok(buffer, " ");
    p = strtok(NULL, " ");
    if (strcmp(buffer, "put") == 0 || strcmp(buffer, "get") == 0
        || strcmp(buffer, "cd") == 0 || strcmp(buffer, "!cd") == 0)
    {
        // must have arg
        if (p == NULL) return -1;
    }
    else if (strcmp(buffer, "pwd") == 0 || strcmp(buffer, "!pwd") == 0
        || strcmp(buffer, "quit") == 0)
    {
        // must not have arg
        if (p != NULL) return -1;
    }
    else if (strcmp(buffer, "ls") != 0 && strcmp(buffer, "!ls") != 0)
    {
        // invalid command
        return -1;
    }

    strcpy(cmd->command, buffer);
    if (p != NULL) // argument exists
        strncpy(cmd->arg, p, strlen(p));

    return 0;
}

/**
 * Creates socket for data and accept connection
 * @param ctrlsock Socket for commands
 * @return socket for data, exit if failed
 */ 
int ftp_client_data_conn(int ctrlsock)
{
    int lstnsock, datasock;
    if ((lstnsock = create_socket(CLIENT_DATA_PORT)) < 0)
        error_exit("fail to create socket");

    // inform server to connect
    if (send(ctrlsock, &(int) {1}, sizeof(int), 0) < 0)
        error_exit("fail to ack server");

    if ((datasock = accept(lstnsock, NULL, NULL)) < 0)
        error_exit("fail to accept socket");

    close(lstnsock);
    return datasock;
}

/**
 * Downloads file from server
 * @param ctrlsock Socket for commands
 * @param cmd Pointer to struct command
 */ 
void ftp_client_get_file(int ctrlsock, Command *cmd)
{
    // send commands and get response
    ftp_client_give_command(ctrlsock, cmd);
    int res_code = get_response_code(ctrlsock);
    if (res_code != CODE_OPEN_DATA_CONN)
    {
        print_response(res_code); // unavailable file
        return;
    }
    
    // start downloading if permitted
    int datasock = ftp_client_data_conn(ctrlsock);
    char data[MAX_BUF_SIZE];

    FILE *fp = fopen(cmd->arg, "w"); // TODO: check
    recv_save_file(data, MAX_BUF_SIZE, datasock, fp);
    close(datasock);
    fclose(fp);

    // done message
    printf("%s is retrieved\n", cmd->arg);
    print_response(get_response_code(ctrlsock));
}

/**
 * Uploads file to server
 * @param ctrlsock Socket for commands
 * @param cmd Pointer to struct command
 */ 
void ftp_client_put_file(int ctrlsock, Command *cmd)
{ 
    // check file exists locally
    FILE *fp = fopen(cmd->arg, "r");
    if (!fp)
    {
        fclose(fp);
        fprintf(stderr, "%s: no such file\n", cmd->arg);
        return;
    }

    // send commands and get response
    ftp_client_give_command(ctrlsock, cmd);
    int res_code = get_response_code(ctrlsock);
    if (res_code == CODE_CMD_BAD_SEQ)
    {
        printf("Operation not allowed: file with same name exists on server\n");
        print_response(res_code);
        fclose(fp);
        return;
    }

    if (res_code != CODE_OPEN_DATA_CONN)
    {
        print_response(res_code);
        fclose(fp);
        return;
    }

    // start uploading if permitted
    char data[MAX_BUF_SIZE];
    memset(data, 0, MAX_BUF_SIZE);
    
    int datasock = ftp_client_data_conn(ctrlsock);
    read_send_file(data, MAX_BUF_SIZE, datasock, fp);
    close(datasock);
    fclose(fp);

    // done message
    printf("%s is uploaded\n", cmd->arg);
    print_response(get_response_code(ctrlsock));
}

/**
 * Sends commands (ls, pwd) and gets output
 * @param ctrlsock Socket for commands
 * @param cmd Pointer to struct command
 */ 
void ftp_client_dir(int ctrlsock, Command *cmd)
{
    // send commands and get response
    ftp_client_give_command(ctrlsock, cmd);
    int res_code = get_response_code(ctrlsock);
    if (res_code != CODE_OPEN_DATA_CONN)
        print_response(res_code); // error res_code
    
    // use data port to get stdout
    char dirbuf[MAX_BUF_SIZE];
    memset(dirbuf, 0, MAX_BUF_SIZE);

    int datasock = ftp_client_data_conn(ctrlsock);
    while (recv(datasock, dirbuf, MAX_BUF_SIZE, 0) > 0)
    {
        printf("%s", dirbuf);
        memset(dirbuf, 0, MAX_BUF_SIZE);
    }

    close(datasock);
}

/**
 * Commands server to cd
 * @param ctrlsock Socket for commands
 * @param cmd Pointer to struct command
 */ 
void ftp_client_chdir(int ctrlsock, Command *cmd)
{
    // does not use data port
    ftp_client_give_command(ctrlsock, cmd);
    print_response(get_response_code(ctrlsock));
}