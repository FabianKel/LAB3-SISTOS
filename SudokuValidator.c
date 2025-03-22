#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <omp.h>

#define SIZE 9

int sudoku[SIZE][SIZE];

// Función para cargar la matriz desde el archivo
void load_sudoku(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Error al abrir el archivo");
        exit(EXIT_FAILURE);
    }

    char *data = mmap(NULL, SIZE * SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("Error al mapear el archivo");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < SIZE * SIZE; i++) {
        sudoku[i / SIZE][i % SIZE] = data[i] - '0';
    }

    munmap(data, SIZE * SIZE);
    close(fd);
}

// Función para verificar una fila
int check_row(int row) {
    int seen[SIZE] = {0};
    for (int i = 0; i < SIZE; i++) {
        int num = sudoku[row][i];
        if (seen[num - 1]) return 0;
        seen[num - 1] = 1;
    }
    return 1;
}

// Función para verificar una columna
void* check_columns(void* arg) {
    int seen[SIZE] = {0};
    for (int col = 0; col < SIZE; col++) {
        for (int i = 0; i < SIZE; i++) {
            int num = sudoku[i][col];
            if (seen[num - 1]) return (void*)0;
            seen[num - 1] = 1;
        }
    }
    printf("Thread ID: %ld\n", syscall(SYS_gettid));
    pthread_exit(0);
}

// Función para verificar un subcuadrante 3x3
int check_subgrid(int row, int col) {
    int seen[SIZE] = {0};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            int num = sudoku[row + i][col + j];
            if (seen[num - 1]) return 0;
            seen[num - 1] = 1;
        }
    }
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <archivo_sudoku>\n", argv[0]);
        return EXIT_FAILURE;
    }

    load_sudoku(argv[1]);

    // Creación del proceso hijo para ejecutar "ps"
    pid_t pid = fork();
    if (pid == 0) {
        char ppid[10];
        snprintf(ppid, 10, "%d", getppid());
        execlp("ps", "ps", "-p", ppid, "-lLf", NULL);
        exit(EXIT_SUCCESS);
    }
    wait(NULL);

    // Creación del pthread para verificar columnas
    pthread_t thread;
    pthread_create(&thread, NULL, check_columns, NULL);
    pthread_join(thread, NULL);

    printf("Thread ID (main): %ld\n", syscall(SYS_gettid));

    // Verificación de filas usando OpenMP
    int valid = 1;
    #pragma omp parallel for shared(valid)
    for (int i = 0; i < SIZE; i++) {
        if (!check_row(i)) valid = 0;
    }

    // Verificación de subcuadrantes 3x3
    #pragma omp parallel for shared(valid) collapse(2)
    for (int i = 0; i < SIZE; i += 3) {
        for (int j = 0; j < SIZE; j += 3) {
            if (!check_subgrid(i, j)) valid = 0;
        }
    }

    printf("La solución es %s\n", valid ? "VÁLIDA" : "INVÁLIDA");

    // Segundo fork() para ejecutar "ps" otra vez
    pid = fork();
    if (pid == 0) {
        char ppid[10];
        snprintf(ppid, 10, "%d", getppid());
        execlp("ps", "ps", "-p", ppid, "-lLf", NULL);
        exit(EXIT_SUCCESS);
    }
    wait(NULL);

    return 0;
}
