#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc < 6) {
        printf("Usage: %s <number1> <number2> <number3> <number4> <number5>\n", argv[0]);
        return 1;
    }

    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IP address from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server\n");

    for (int i = 1; i < argc; i++) {
        int number = atoi(argv[i]);

        // Send the number to the server
        sprintf(buffer, "%d", number);
        if (write(sock, buffer, strlen(buffer)) < 0) {
            perror("write failed");
            exit(EXIT_FAILURE);
        }

        printf("Number sent: %d\n", number);

        // Read the response from the server
        if ((valread = read(sock, buffer, BUFFER_SIZE)) < 0) {
            perror("read failed");
            exit(EXIT_FAILURE);
        }

        buffer[valread] = '\0';
        printf("Server response: %s\n", buffer);

        sleep(3); // Wait for 3 seconds before sending the next number
    }

    close(sock);
    return 0;
}
