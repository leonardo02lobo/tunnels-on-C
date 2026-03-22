#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/mman.h>
    #include <sys/wait.h>
    #include <semaphore.h>
    #include <fcntl.h>
#endif

#define SIZE 256

void to_uppercase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = toupper(str[i]);
    }
}

int main(){
    #ifdef _WIN32

        HANDLE hPipeRead, hPipeWrite;
        SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};

        CreatePipe(&hPipeRead, &hPipeWrite, &sa, 0);

        HANDLE hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SIZE, "SharedMemory");
        char *shared_mem = (char *)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, SIZE);

        HANDLE sem = CreateSemaphore(NULL, 0, 1, "Semaphore");

        if (fork() == 0){
            char buffer[SIZE];

            printf("Ingrese texto: ");
            fgets(buffer, SIZE, stdin);

            WriteFile(hPipeWrite, buffer, strlen(buffer) + 1, NULL, NULL);

            WaitForSingleObject(sem, INFINITE);

            printf("Texto modificado: %s\n", shared_mem);
            exit(0);
        }

        if (fork() == 0){
            char buffer[SIZE];

            ReadFile(hPipeRead, buffer, SIZE, NULL, NULL);
            to_uppercase(buffer);

            strcpy(shared_mem, buffer);

            ReleaseSemaphore(sem, 1, NULL);
            exit(0);
        }

        WaitForSingleObject(sem, INFINITE);
        printf("Padre lee: %s\n", shared_mem);

    #else
        int pipefd[2];
        pipe(pipefd);

        char *shared_mem = mmap(NULL, SIZE, PROT_READ | PROT_WRITE,
                                MAP_SHARED | MAP_ANONYMOUS, -1, 0);

        sem_t *sem = mmap(NULL, sizeof(sem_t),
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);

        sem_init(sem, 1, 0);

        pid_t pid1 = fork();

        if (pid1 == 0){
            char buffer[SIZE];

            printf("Ingrese texto: ");
            fgets(buffer, SIZE, stdin);

            write(pipefd[1], buffer, strlen(buffer) + 1);

            sem_wait(sem);

            printf("Hijo1 recibe: %s\n", shared_mem);
            exit(0);
        }

        pid_t pid2 = fork();

        if (pid2 == 0){
            char buffer[SIZE];

            read(pipefd[0], buffer, SIZE);

            to_uppercase(buffer);

            strcpy(shared_mem, buffer);

            sem_post(sem);
            exit(0);
        }

        wait(NULL);
        wait(NULL);

        printf("Padre lee: %s\n", shared_mem);
    #endif
    return 0;
}