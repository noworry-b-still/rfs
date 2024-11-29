#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 8192

int main(int argc, char *argv[])
{
    int socket_desc;
    struct sockaddr_in server_addr;
    char command[BUFFER_SIZE], buffer[BUFFER_SIZE];
    int local_fd, bytes_read;
    ssize_t total_bytes_sent = 0;

    if (argc != 4 || strcmp(argv[1], "WRITE") != 0)
    {
        printf("Usage: %s WRITE <local-file-path> <remote-file-path>\n", argv[0]);
        return 1;
    }

    // Open the local file
    local_fd = open(argv[2], O_RDONLY);
    if (local_fd < 0)
    {
        perror("Failed to open local file");
        return 1;
    }

    // Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1)
    {
        perror("Could not create socket");
        close(local_fd);
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
        close(local_fd);
        close(socket_desc);
        return 1;
    }

    // Send the WRITE command
    snprintf(command, sizeof(command), "WRITE %s %s", argv[2], argv[3]);
    printf("Sending command: %s\n", command);
    if (send(socket_desc, command, strlen(command), 0) < 0)
    {
        perror("Failed to send command");
        close(local_fd);
        close(socket_desc);
        return 1;
    }

    // Wait for server to be ready
    bytes_read = recv(socket_desc, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
    {
        perror("Failed to receive server ready message");
        close(local_fd);
        close(socket_desc);
        return 1;
    }
    buffer[bytes_read] = '\0';
    printf("Server response: %s", buffer);

    if (strncmp(buffer, "READY\n", 6) != 0)
    {
        printf("Server not ready: %s", buffer);
        close(local_fd);
        close(socket_desc);
        return 1;
    }

    // Send the file data
    while ((bytes_read = read(local_fd, buffer, sizeof(buffer))) > 0)
    {
        if (send(socket_desc, buffer, bytes_read, 0) < 0)
        {
            perror("Failed to send file data");
            close(local_fd);
            close(socket_desc);
            return 1;
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
        close(socket_desc);
        return 1;
    }

    close(local_fd);

    // Receive server response
    bytes_read = recv(socket_desc, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0'; // Null-terminate the response
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

    // Close the socket
    close(socket_desc);
    return 0;
}