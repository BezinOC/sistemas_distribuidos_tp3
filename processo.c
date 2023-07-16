#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MESSAGE_SIZE 10
#define SEPARATOR '|'
#define REQUEST_MESSAGE_TYPE '1'
#define GRANT_MESSAGE_TYPE '2'
#define RELEASE_MESSAGE_TYPE '3'

typedef struct {
    int thread_id;
    int r;
    int k;
} ThreadArgs;

typedef struct {
    char type;
    char process_id;
    char padding[MESSAGE_SIZE - 3];
} Message;

void write_to_file(int pid, int number, const char* response) {
    FILE* file = fopen("client_log.txt", "a");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);

    // Format the time as HH:MM:SS:MS
    char timeStr[30];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S:", localtime(&tv.tv_sec));
    sprintf(timeStr + strlen(timeStr), "%03ld", tv.tv_usec / 1000);

    // Format the date as DD/MM/YYYY
    char dateStr[20];
    strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", localtime(&tv.tv_sec));

    fprintf(file, "PID: %d, Number Sent: %d, Time: %s %s, Response: %s\n",
            pid, number, timeStr, dateStr, response);

    fclose(file);
}

void* client_thread(void* arg) {
    ThreadArgs* thread_args = (ThreadArgs*)arg;
    int thread_id = thread_args->thread_id;
    int r = thread_args->r;
    int k = thread_args->k;
    int pid = getpid();

    srand(time(NULL) ^ (pid << 16)); // Seed the random number generator with PID

    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        pthread_exit(NULL);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IP address from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        pthread_exit(NULL);
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        pthread_exit(NULL);
    }

    printf("Thread %d (PID: %d) connected to server\n", thread_id, pid);

    for (int i = 0; i < r; i++) {
        int number = rand() % 20 + 1; // Generate a random number between 1 and 20

        // Create the REQUEST message
        Message request_message;
        request_message.type = REQUEST_MESSAGE_TYPE;
        request_message.process_id = thread_id + '0'; // Convert the thread ID to char

        // Send the REQUEST message to the server
        if (write(sock, &request_message, sizeof(request_message)) < 0) {
            perror("write failed");
            close(sock);
            pthread_exit(NULL);
        }

        // Write PID, number sent, and request mensage to server to the file
        write_to_file(pid, number, "REQUEST");

        // Receive the GRANT message from the server
        Message grant_message;
        if ((valread = read(sock, &grant_message, sizeof(grant_message))) < 0) {
            perror("read failed");
            close(sock);
            pthread_exit(NULL);
        }

        // Check if the received message is a GRANT message
        if (grant_message.type != GRANT_MESSAGE_TYPE) {
            printf("Thread %d (PID: %d) received an unexpected message\n", thread_id, pid);
            printf("%c\n",grant_message.type);
            close(sock);
            pthread_exit(NULL);
        }

        printf("Thread %d (PID: %d) received GRANT message from server\n", thread_id, pid);

        // Write PID, number sent, and server response to the file
        write_to_file(pid, number, "GRANT");

        
        // Simulate the critical section by sleeping for a random time
        sleep(k);

        // Create the RELEASE message
        Message release_message;
        release_message.type = RELEASE_MESSAGE_TYPE;
        release_message.process_id = thread_id + '0'; // Convert the thread ID to char

        // Send the RELEASE message to the server
        if (write(sock, &release_message, sizeof(release_message)) < 0) {
            perror("write failed");
            close(sock);
            pthread_exit(NULL);
        }

        printf("Thread %d (PID: %d) sent RELEASE message\n", thread_id, pid);

        // Write PID, number sent, and request mensage to server to the file
        write_to_file(pid, number, "RELEASE");
        
    }

    close(sock);
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: %s <n (number of clients)> <r (number of file writings)> <k (number of seconds between each request)>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    int r = atoi(argv[2]);
    int k = atoi(argv[3]);

    pthread_t threads[n];
    ThreadArgs thread_args[n];

    for (int i = 0; i < n; i++) {
        thread_args[i].thread_id = i + 1;
        thread_args[i].r = r;
        thread_args[i].k = k;

        if (pthread_create(&threads[i], NULL, client_thread, &thread_args[i]) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    for (int i = 0; i < n; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("pthread_join");
            return 1;
        }
    }

    return 0;
}
