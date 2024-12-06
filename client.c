#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUFFER_SIZE 8192

void handle_write(int socket_desc, const char *local_file_path, const char *remote_file_path, const char *permission);
void handle_get(int socket_desc, const char *remote_file_path, const char *local_file_path);
void handle_rm(int socket_desc, const char *remote_file_path);
void create_local_dir(const char *local_file_path);

int main(int argc, char *argv[])
{
    int socket_desc;
    struct sockaddr_in server_addr;

    if (argc < 3 || argc > 6)
    {
        printf("Usage: %s <COMMAND> [local-path] <remote-path> [-r|-rw]\n", argv[0]);
        printf("COMMAND can be:\n");
        printf("  WRITE: Upload a local file to the server (requires both local and remote paths)\n");
        printf("        Optional flags: -r (read-only), -rw (read-write, default)\n");
        printf("  GET: Download a file from the server (requires both remote and local paths)\n");
        printf("  RM: Delete a file from the server (requires only remote path)\n");
        return 1;
    }

    // Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1)
    {
        perror("Could not create socket");
        return 1;
    }

    // Initialize server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Connect to the server
    if (connect(socket_desc, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        close(socket_desc);
        return 1;
    }

    if (strcmp(argv[1], "WRITE") == 0)
    {
        char *permission = "-rw"; // Default to read-write

        // Check for permission flag
        if (argc == 5)
        {
            if (strcmp(argv[4], "-r") == 0 || strcmp(argv[4], "-rw") == 0)
            {
                permission = argv[4];
            }
            else
            {
                printf("Invalid permission flag. Use -r or -rw.\n");
                close(socket_desc);
                return 1;
            }
        }
        else if (argc != 4)
        {
            printf("WRITE requires a local path and a remote path.\n");
            close(socket_desc);
            return 1;
        }

        handle_write(socket_desc, argv[2], argv[3], permission);
    }
    else if (strcmp(argv[1], "GET") == 0)
    {
        if (argc != 4)
        {
            printf("GET requires a remote path and a local path.\n");
            close(socket_desc);
            return 1;
        }
        handle_get(socket_desc, argv[2], argv[3]);
    }
    else if (strcmp(argv[1], "RM") == 0)
    {
        if (argc != 3)
        {
            printf("RM requires only a remote path.\n");
            close(socket_desc);
            return 1;
        }
        handle_rm(socket_desc, argv[2]);
    }
    else
    {
        printf("Invalid command. Use WRITE, GET, or RM.\n");
    }

    close(socket_desc);
    exit(0); // Explicitly exit after command
}

void handle_write(int socket_desc, const char *local_file_path, const char *remote_file_path, const char *permission)
{
    char command[BUFFER_SIZE], buffer[BUFFER_SIZE];
    int local_fd;
    ssize_t total_bytes_sent = 0;
    ssize_t bytes_read;

    // Open the local file
    local_fd = open(local_file_path, O_RDONLY);
    if (local_fd < 0)
    {
        perror("Failed to open local file");
        return;
    }

    // Send the WRITE command with permission
    snprintf(command, sizeof(command), "WRITE %s %s %s", local_file_path, remote_file_path, permission);
    printf("Sending command: %s\n", command);
    if (send(socket_desc, command, strlen(command), 0) < 0)
    {
        perror("Failed to send command");
        close(local_fd);
        return;
    }

    // Wait for server to be ready
    bytes_read = recv(socket_desc, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
    {
        perror("Failed to receive server ready message");
        close(local_fd);
        return;
    }
    buffer[bytes_read] = '\0';
    printf("Server response: %s", buffer);

    if (strncmp(buffer, "READY\n", 6) != 0)
    {
        printf("Server not ready: %s", buffer);
        close(local_fd);
        return;
    }

    // Send the file data
    while ((bytes_read = read(local_fd, buffer, sizeof(buffer))) > 0)
    {
        if (send(socket_desc, buffer, bytes_read, 0) < 0)
        {
            perror("Failed to send file data");
            close(local_fd);
            return;
        }
        total_bytes_sent += bytes_read;
        printf("Sent %zd bytes\n", total_bytes_sent);
    }

    // Send end-of-transmission marker
    printf("Sending DONE marker\n");
    if (send(socket_desc, "DONE\n", 5, 0) < 0)
    {
        perror("Failed to send end marker");
        close(local_fd);
        return;
    }

    close(local_fd);

    // Receive server response
    bytes_read = recv(socket_desc, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';
        printf("Server response: %s", buffer);
    }
    else if (bytes_read == 0)
    {
        printf("Server closed the connection\n");
    }
    else
    {
        perror("Failed to receive acknowledgment");
    }
}

void create_local_dir(const char *local_file_path)
{
    char dir[BUFFER_SIZE];
    char *dir_end;

    // Copy the local file path to dir to modify only the directory part
    strncpy(dir, local_file_path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';

    // Find the last '/' in the path (before the file name)
    dir_end = strrchr(dir, '/');
    if (dir_end)
    {
        *dir_end = '\0'; // null-terminate at the last '/' to separate directory from file

        // Check if the directory exists
        struct stat st = {0};
        if (stat(dir, &st) == -1)
        {
            // Directory doesn't exist, create it
            if (mkdir(dir, 0700) != 0)
            {
                perror("Failed to create directory");
                exit(1); // Exit on failure to create directory
            }
            printf("[INFO] Directory created: %s\n", dir);
        }
        else
        {
            printf("[INFO] Directory already exists: %s\n", dir);
        }
    }
}

void handle_get(int socket_desc, const char *remote_file_path, const char *local_file_path)
{
    char command[BUFFER_SIZE], buffer[BUFFER_SIZE];
    int local_fd;
    ssize_t bytes_read;
    ssize_t total_bytes_received = 0;
    int done_received = 0;

    printf("[INFO] Preparing to download remote file '%s' to local path '%s'.\n", remote_file_path, local_file_path);

    // Create the necessary local directory if it doesn't exist
    create_local_dir(local_file_path);

    // Open the local file for writing
    local_fd = open(local_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (local_fd < 0)
    {
        perror("[ERROR] Failed to open local file");
        return;
    }

    // Send the GET command with both remote and local file paths
    snprintf(command, sizeof(command), "GET %s %s", remote_file_path, local_file_path);
    printf("[INFO] Sending command: %s\n", command);
    if (send(socket_desc, command, strlen(command), 0) < 0)
    {
        perror("[ERROR] Failed to send GET command");
        close(local_fd);
        return;
    }

    // Receive the server response
    bytes_read = recv(socket_desc, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
    {
        perror("[ERROR] Failed to receive server response");
        close(local_fd);
        return;
    }
    buffer[bytes_read] = '\0';
    printf("[INFO] Server response: %s", buffer);

    if (strncmp(buffer, "READY\n", 6) != 0)
    {
        printf("[ERROR] Server response indicates an error: %s", buffer);
        close(local_fd);
        return;
    }

    // Now receive the file data from the server
    while (!done_received)
    {
        bytes_read = recv(socket_desc, buffer, sizeof(buffer), 0);
        if (bytes_read <= 0)
        {
            perror("[ERROR] Failed to receive file data");
            break;
        }

        // Check for 'DONE' marker
        if (bytes_read == 5 && strncmp(buffer, "DONE\n", 5) == 0)
        {
            printf("[INFO] Received DONE marker. Total bytes received: %zd.\n", total_bytes_received);
            done_received = 1;
            break;
        }

        if (write(local_fd, buffer, bytes_read) < 0)
        {
            perror("[ERROR] Failed to write to local file");
            close(local_fd);
            return;
        }
        total_bytes_received += bytes_read;
    }

    close(local_fd);
    printf("[INFO] File downloaded successfully.\n");
}

void handle_rm(int socket_desc, const char *remote_file_path)
{
    char command[BUFFER_SIZE], buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Send the RM command
    snprintf(command, sizeof(command), "RM %s", remote_file_path);
    if (send(socket_desc, command, strlen(command), 0) < 0)
    {
        perror("Failed to send RM command");
        return;
    }

    // Wait for server response
    bytes_read = recv(socket_desc, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';
        printf("Server response: %s\n", buffer);
    }
    else
    {
        perror("Failed to receive server response");
    }
}
