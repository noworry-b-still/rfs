# Remote File System Implemented In C
------------------------------------

## Features:

1. Upload a file 
2. Download a file
3. Delete a file
4. Permissions (read-only and read-write(default if permissions are not provided))

## General Flow:

1. Run `make` to compile and build.
2. Run `./server` in one terminal
3. Run `./client WRITE local-file-path remote-file-path` (to upload a file to remote server)
4. Run `./client GET remote-file-path local-file-path` (to download a file from remote server)
5. Run `./client RM remote-file-path` (to remove a file from remote server)

## Permissions:

1. When a file is created, its permissions defaults to read and write, unless read-only flag is specified.
2. Proper error logs will be printed to the terminal if wrong permissions are given.

## Client/Server:

1. Client disconnects after making/handling a command request.
2. Server stays alive until an interrupt (Ctrl-C) is pressed, basically it listens for clients requests until killed.






