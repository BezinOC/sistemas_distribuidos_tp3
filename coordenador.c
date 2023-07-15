#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define MAX_CLIENTS 5

void *handle_client(void *socket_desc) {
    int client_socket = *(int *)socket_desc;
    char buffer[1024] = {0};
    int valread;

    while (1) {
        // Receive the number from the client
        if ((valread = read(client_socket, buffer, sizeof(buffer))) <= 0) {
            if (valread == 0) {
                printf("Client disconnected\n");
            } else {
                perror("read");
            }
            break; // Exit the loop on read error or disconnection
        }

        int number = atoi(buffer);
        printf("Client sent: %d\n", number);

        // Determine if the number is even or odd
        char response[1024];
        if (number % 2 == 0) {
            strcpy(response, "Even");
        } else {
            strcpy(response, "Odd");
        }

        // Send the response back to the client
        write(client_socket, response, strlen(response));

        memset(buffer, 0, sizeof(buffer));
    }

    close(client_socket);
    free(socket_desc);
    pthread_exit(NULL);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    pthread_t thread_id;

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options to reuse address and port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket to the specified address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    while (1) {
        // Accept incoming connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        printf("New client connected\n");

        // Create a new thread to handle the client communication
        int *socket_desc = malloc(sizeof(int));
        *socket_desc = new_socket;

        if (pthread_create(&thread_id, NULL, handle_client, (void *)socket_desc) < 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }

        // Detach the thread so that it cleans up automatically when it exits
        if (pthread_detach(thread_id) != 0) {
            perror("pthread_detach");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}
