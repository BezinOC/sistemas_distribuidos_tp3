#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define MAX_CLIENTS 5
#define MESSAGE_SIZE 10
#define SEPARATOR '|'
#define REQUEST_MESSAGE_TYPE '1'
#define GRANT_MESSAGE_TYPE '2'
#define RELEASE_MESSAGE_TYPE '3'

// Estrutura de mensagem
typedef struct {
    char type;
    char process_id;
    char padding[MESSAGE_SIZE - 3]; // Tamanho fixo da mensagem
} Message;

// Estrutura de fila de pedidos
typedef struct {
    Message requests[MAX_CLIENTS];
    int front;
    int rear;
    int count;
} RequestQueue;

// Estrutura de estatísticas de acesso
typedef struct {
    int process_counts[MAX_CLIENTS];
} AccessStats;

pthread_mutex_t queue_mutex; // Mutex para acessar a fila de pedidos
pthread_mutex_t stats_mutex; // Mutex para acessar as estatísticas de acesso
RequestQueue request_queue;   // Fila de pedidos
AccessStats access_stats;     // Estatísticas de acesso

void initialize_request_queue(RequestQueue *queue) {
    queue->front = 0;
    queue->rear = -1;
    queue->count = 0;
}

int is_request_queue_empty(RequestQueue *queue) {
    return (queue->count == 0);
}

int is_request_queue_full(RequestQueue *queue) {
    return (queue->count == MAX_CLIENTS);
}

void enqueue_request(RequestQueue *queue, Message request) {
    if (!is_request_queue_full(queue)) {
        queue->rear = (queue->rear + 1) % MAX_CLIENTS;
        queue->requests[queue->rear] = request;
        queue->count++;
    }
}

Message dequeue_request(RequestQueue *queue) {
    Message request;
    if (!is_request_queue_empty(queue)) {
        request = queue->requests[queue->front];
        queue->front = (queue->front + 1) % MAX_CLIENTS;
        queue->count--;
    }
    return request;
}

void initialize_access_stats(AccessStats *stats) {
    memset(stats->process_counts, 0, sizeof(stats->process_counts));
}

void increment_access_count(AccessStats *stats, char process_id) {
    stats->process_counts[process_id - '0']++;
}

void *handle_client(void *socket_desc) {
    int client_socket = *(int *)socket_desc;
    char buffer[MESSAGE_SIZE];
    int valread;

    while (1) {
        // Receive the message from the client
        if ((valread = read(client_socket, buffer, sizeof(buffer))) <= 0) {
            if (valread == 0) {
                printf("Client disconnected\n");
            } else {
                perror("read");
            }
            break; // Exit the loop on read error or disconnection
        }

        Message message;
        memcpy(&message, buffer, sizeof(message));

        printf("Received message from Process %c: Type: %c\n", message.process_id, message.type);

        // Process the message based on its type
        switch (message.type) {
            case REQUEST_MESSAGE_TYPE:
                pthread_mutex_lock(&queue_mutex);
                enqueue_request(&request_queue, message);
                pthread_mutex_unlock(&queue_mutex);
                break;

            case RELEASE_MESSAGE_TYPE:
                pthread_mutex_lock(&queue_mutex);
                pthread_mutex_lock(&stats_mutex);
                increment_access_count(&access_stats, message.process_id);
                pthread_mutex_unlock(&stats_mutex);
                // Process the next request in the queue, if any
                if (!is_request_queue_empty(&request_queue)) {
                    Message next_request = dequeue_request(&request_queue);
                    // Send a GRANT message to the next process in the queue
                    write(client_socket, &next_request, sizeof(next_request));
                    printf("Sent GRANT message to Process %c\n", next_request.process_id);
                }
                pthread_mutex_unlock(&queue_mutex);
                break;
        }

        memset(buffer, 0, sizeof(buffer));
    }

    close(client_socket);
    free(socket_desc);
    pthread_exit(NULL);
}

void *interface_thread(void *arg) {
    while (1) {
        printf("Options:\n");
        printf("1. Print current request queue\n");
        printf("2. Print access statistics\n");
        printf("3. Exit\n");

        int choice;
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                pthread_mutex_lock(&queue_mutex);
                printf("Current request queue:\n");
                for (int i = 0; i < request_queue.count; i++) {
                    int index = (request_queue.front + i) % MAX_CLIENTS;
                    printf("Process %c\n", request_queue.requests[index].process_id);
                }
                pthread_mutex_unlock(&queue_mutex);
                break;

            case 2:
                pthread_mutex_lock(&stats_mutex);
                printf("Access statistics:\n");
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    printf("Process %d: %d\n", i, access_stats.process_counts[i]);
                }
                pthread_mutex_unlock(&stats_mutex);
                break;

            case 3:
                printf("Exiting...\n");
                pthread_exit(NULL);
        }
    }
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
    #ifdef SO_REUSEPORT
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    #else
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    #endif
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

    // Initialize request queue and access stats
    initialize_request_queue(&request_queue);
    initialize_access_stats(&access_stats);

    // Initialize mutexes
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_init(&stats_mutex, NULL);

    // Create interface thread
    pthread_create(&thread_id, NULL, interface_thread, NULL);

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

    // Destroy mutexes
    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&stats_mutex);

    return 0;
}
