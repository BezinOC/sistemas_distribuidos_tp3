// main.c

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void* callScript(void* arg) {
    const char* command = (const char*)arg;
    printf("Calling script1's main function with command: %s\n", command);
    system(command);
    printf("script1's main function was called successfully with command: %s\n", command);
    return NULL;
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

    for (int i = 0; i < n; i++) {
        char command[256];
        snprintf(command, sizeof(command), "./processo %d %d %d", i+1, r, k);
        pthread_create(&threads[i], NULL, callScript, (void*)command);
    }

    for (int i = 0; i < n; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
