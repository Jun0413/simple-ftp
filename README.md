# simple-ftp

This is a simplified version of client-server FTP. Authenticated clients can issue commands to server on another host to download/upload files in active (PORT) mode. The server process can handle concurrent users using multi-threading.

#### Installation & Run

```bash
$ make
$ ./server <port> # default directory .
$ ./client <server_ip> <port>
```

#### Login

```bash
username: user
password: pass
```

#### Commands

```bash
>mftp cd <path>            change server directory
>mftp !cd <path>           change local directory
>mftp pwd                  pwd on server
>mftp !pwd                 pwd locally
>mftp ls                   list files under server pwd
>mftp put <filename>       upload <filename> to server
>mftp get <filename>       download <filename> from server
>mftp quit (or ctrl+d)     quit client process
```

