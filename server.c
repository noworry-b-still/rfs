#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUFFER_SIZE 8192
#define SERVER_ROOT "./server_root"

void handle_client(int client_sock);

int main(void)
{
    int socket_desc, client_sock;
    socklen_t client_size;
    struct sockaddr_in server_addr, client_addr;

    // Create root directory if it doesn't exist
    mkdir(SERVER_ROOT, 0755);

    // Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc < 0)
    {
        perror("Error creating socket");
        return -1;
    }
    printf("Socket created\n");

    // Set port and IP
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2000);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind to the set port and IP
    if (bind(socket_desc, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(socket_desc);
        return -1;
    }
    printf("Bind successful\n");

    // Listen for incoming connections
    if (listen(socket_desc, 5) < 0)
    {
        perror("Error while listening");
        close(socket_desc);
        return -1;
    }
    printf("Listening for incoming connections...\n");

    while (1)
    {
        // Accept an incoming connection
        client_size = sizeof(client_addr);
        client_sock = accept(socket_desc, (struct sockaddr *)&client_addr, &client_size);

        if (client_sock < 0)
        {
            perror("Accept failed");
            continue;
        }
        printf("Client connected\n");

        // Handle the client request
        handle_client(client_sock);

        // Close the client socket
        close(client_sock);
        printf("Client disconnected\n");
    }

    // Close the listening socket
    close(socket_desc);
    return 0;
}

void handle_client(int client_sock)
{
    char command[BUFFER_SIZE], buffer[BUFFER_SIZE];
    char local_file[256], remote_file[256];
    ssize_t bytes_read, file_fd;
    char full_path[BUFFER_SIZE];
    ssize_t total_bytes_transferred = 0;

    // Receive the command from the client
    if ((bytes_read = recv(client_sock, command, sizeof(command), 0)) <= 0)
    {
        perror("Failed to receive command");
        return;
    }
    command[bytes_read] = '\0';
    printf("Received command: %s\n", command);

    if (strncmp(command, "WRITE", 5) == 0)
    {
        if (sscanf(command, "WRITE %255s %255s", local_file, remote_file) != 2)
        {
            const char *error_msg = "Invalid command format. Use: WRITE <local-file> <remote-file>\n";
            send(client_sock, error_msg, strlen(error_msg), 0);
            return;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", SERVER_ROOT, remote_file);
        printf("Full path: %s\n", full_path);

        char *dir_end = strrchr(full_path, '/');
        if (dir_end)
        {
            *dir_end = '\0';
            mkdir(full_path, 0755);
            *dir_end = '/';
        }

        file_fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (file_fd < 0)
        {
            perror("Failed to open file");
            return;
        }

        const char *ready_msg = "READY\n";
        if (send(client_sock, ready_msg, strlen(ready_msg), 0) < 0)
        {
            perror("Failed to send ready message");
            close(file_fd);
            return;
        }

        while (1)
        {
            bytes_read = recv(client_sock, buffer, sizeof(buffer), 0);
            if (bytes_read <= 0)
            {
                perror("Failed to receive data");
                close(file_fd);
                return;
            }

            // Check for DONE marker
            if (bytes_read == 5 && strncmp(buffer, "DONE\n", 5) == 0)
            {
                printf("Received DONE marker. Transferred %zd bytes\n", total_bytes_transferred);
                break;
            }

            if (write(file_fd, buffer, bytes_read) < 0)
            {
                perror("Failed to write to file");
                close(file_fd);
                return;
            }
            total_bytes_transferred += bytes_read;
        }

        close(file_fd);

        // Send acknowledgment
        const char *ack_msg = "File transfer complete\n";
        send(client_sock, ack_msg, strlen(ack_msg), 0);
    }
    else if (strncmp(command, "GET", 3) == 0)
    {
        if (sscanf(command, "GET %255s %255s", remote_file, local_file) != 2)
        {
            const char *error_msg = "Invalid command format. Use: GET <remote-file> <local-file>\n";
            send(client_sock, error_msg, strlen(error_msg), 0);
            return;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", SERVER_ROOT, remote_file);
        printf("Full path: %s\n", full_path);

        file_fd = open(full_path, O_RDONLY);
        if (file_fd < 0)
        {
            perror("Failed to open file");
            const char *error_msg = "ERROR: File not found\n";
            send(client_sock, error_msg, strlen(error_msg), 0);
            return;
        }

        const char *ready_msg = "READY\n";
        if (send(client_sock, ready_msg, strlen(ready_msg), 0) < 0)
        {
            perror("Failed to send ready message");
            close(file_fd);
            return;
        }

        while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0)
        {
            if (send(client_sock, buffer, bytes_read, 0) < 0)
            {
                perror("Failed to send file data");
                close(file_fd);
                return;
            }
        }

        // Send DONE marker as an end-of-transmission signal
        if (send(client_sock, "DONE\n", 5, 0) < 0)
        {
            perror("Failed to send DONE marker");
        }

        close(file_fd);
    }
    else if (strncmp(command, "RM", 2) == 0)
    {
        // Parse the RM command
        if (sscanf(command, "RM %255s", remote_file) != 1)
        {
            const char *error_msg = "Invalid command format. Use: RM <remote-file-or-folder>\n";
            send(client_sock, error_msg, strlen(error_msg), 0);
            return;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", SERVER_ROOT, remote_file);
        printf("Full path to delete: %s\n", full_path);

        // Attempt to remove the file or directory
        if (remove(full_path) == 0)
        {
            const char *success_msg = "SUCCESS: File or folder deleted\n";
            send(client_sock, success_msg, strlen(success_msg), 0);
        }
        else
        {
            perror("Failed to delete file or folder");
            const char *error_msg = "ERROR: Failed to delete file or folder\n";
            send(client_sock, error_msg, strlen(error_msg), 0);
        }
    }
    else
    {
        const char *error_msg = "ERROR: Unsupported command\n";
        send(client_sock, error_msg, strlen(error_msg), 0);
    }
}