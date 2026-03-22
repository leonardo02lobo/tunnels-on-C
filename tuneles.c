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

void to_uppercase(char *str)
{
    int i = 0;
    for (i = 0; str[i]; i++)
    {
        str[i] = toupper(str[i]);
    }
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    HANDLE hPipeRead, hPipeWrite;
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};

    CreatePipe(&hPipeRead, &hPipeWrite, &sa, 0);

    HANDLE hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SIZE, "Global\\SharedMemory");
    char *shared_mem = (char *)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, SIZE);

    HANDLE sem = CreateSemaphore(NULL, 0, 1, "Global\\Semaphore");

    if (argc > 1 && strcmp(argv[1], "hijo1") == 0)
    {

        char buffer[SIZE];

        printf("Ingrese texto: ");
        fgets(buffer, SIZE, stdin);

        DWORD written;
        WriteFile(hPipeWrite, buffer, strlen(buffer) + 1, &written, NULL);

        WaitForSingleObject(sem, INFINITE);

        printf("Hijo1 recibe: %s\n", shared_mem);
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "hijo2") == 0)
    {

        char buffer[SIZE];
        DWORD read;

        ReadFile(hPipeRead, buffer, SIZE, &read, NULL);

        to_uppercase(buffer);

        strcpy(shared_mem, buffer);

        ReleaseSemaphore(sem, 1, NULL);
        return 0;
    }
    STARTUPINFO si1 = {sizeof(si1)};
    PROCESS_INFORMATION pi1;

    STARTUPINFO si2 = {sizeof(si2)};
    PROCESS_INFORMATION pi2;

    char cmd1[] = "tuneles.exe hijo1";
    char cmd2[] = "tuneles.exe hijo2";

    CreateProcess(NULL, cmd1, NULL, NULL, TRUE, 0, NULL, NULL, &si1, &pi1);
    CreateProcess(NULL, cmd2, NULL, NULL, TRUE, 0, NULL, NULL, &si2, &pi2);

    WaitForSingleObject(sem, INFINITE);

    printf("Padre lee: %s\n", shared_mem);

    WaitForSingleObject(pi1.hProcess, INFINITE);
    WaitForSingleObject(pi2.hProcess, INFINITE);
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

    if (pid1 == 0)
    {
        char buffer[SIZE];

        printf("Ingrese texto: ");
        fgets(buffer, SIZE, stdin);

        write(pipefd[1], buffer, strlen(buffer) + 1);

        sem_wait(sem);

        printf("Hijo1 recibe: %s\n", shared_mem);
        exit(0);
    }

    pid_t pid2 = fork();

    if (pid2 == 0)
    {
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
