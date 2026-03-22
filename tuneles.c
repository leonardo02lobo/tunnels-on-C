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
        HANDLE hPipe;
        HANDLE hMapFile;
        HANDLE sem;
        char *shared_mem;

        hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL,
                                    PAGE_READWRITE, 0, SIZE,
                                    "Global\\MySharedMemory");

        shared_mem = (char *)MapViewOfFile(hMapFile,
                                        FILE_MAP_ALL_ACCESS,
                                        0, 0, SIZE);

        sem = CreateSemaphore(NULL, 0, 1, "Global\\MySemaphore");

        if (argc > 1 && strcmp(argv[1], "hijo1") == 0) {

            char buffer[SIZE];

            printf("Ingrese texto: ");
            fgets(buffer, SIZE, stdin);

            hPipe = CreateFile("\\\\.\\pipe\\MyPipe",
                            GENERIC_WRITE, 0, NULL,
                            OPEN_EXISTING, 0, NULL);

            DWORD written;
            WriteFile(hPipe, buffer, strlen(buffer) + 1, &written, NULL);

            CloseHandle(hPipe);

            WaitForSingleObject(sem, INFINITE);

            printf("Hijo1 recibe: %s\n", shared_mem);

            return 0;
        }
        if (argc > 1 && strcmp(argv[1], "hijo2") == 0) {

            char buffer[SIZE];
            DWORD read;

            hPipe = CreateNamedPipe(
                "\\\\.\\pipe\\MyPipe",
                PIPE_ACCESS_INBOUND,
                PIPE_TYPE_BYTE | PIPE_WAIT,
                1, SIZE, SIZE, 0, NULL
            );

            ConnectNamedPipe(hPipe, NULL);

            ReadFile(hPipe, buffer, SIZE, &read, NULL);

            to_uppercase(buffer);

            strcpy(shared_mem, buffer);

            CloseHandle(hPipe);

            ReleaseSemaphore(sem, 1, NULL);

            return 0;
        }
        STARTUPINFO si1 = { sizeof(si1) };
        PROCESS_INFORMATION pi1;

        STARTUPINFO si2 = { sizeof(si2) };
        PROCESS_INFORMATION pi2;

        char cmd1[] = ".\\tuneles.exe hijo1";
        char cmd2[] = ".\\tuneles.exe hijo2";

        CreateProcess(NULL, cmd2, NULL, NULL, FALSE, 0, NULL, NULL, &si2, &pi2);

        Sleep(500); 

        CreateProcess(NULL, cmd1, NULL, NULL, FALSE, 0, NULL, NULL, &si1, &pi1);

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
