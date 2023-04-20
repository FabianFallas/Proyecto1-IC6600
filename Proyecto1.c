///////////////////////////////////////////////////////////
//El codigo se ejecuta de la siguiente forma e  visual studio code:
//- gcc -o Proyecto1 Proyecto1.c -lpthread
//- ./Proyecto1 palabraabuscar archivo
//////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <regex.h>

#define MAX_THREADS 4 // número máximo de hilos
#define BUFFER_SIZE 1024 // tamaño del buffer

typedef struct {
    FILE *file; // puntero al archivo que se está procesando
    char *buffer; // buffer para almacenar los datos leídos del archivo
    size_t size; // tamaño del buffer
    regex_t regex; // expresión regular que se está buscando
} thread_data_t;

// función que realiza la búsqueda en un archivo
void *worker(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    char *line = NULL;
    size_t line_len = 0;
    int offset = 0;
    int result;
    while (1) {
        // lee una línea del archivo
        if (getline(&line, &line_len, data->file) == -1) {
            // si no se puede leer más del archivo, cierra el archivo y libera el buffer
            fclose(data->file);
            free(data->buffer);
            break;
        }
        // realiza la búsqueda en la línea leída usando la expresión regular
        result = regexec(&data->regex, line, 0, NULL, 0);
        if (result == 0) {
            // si la expresión regular encuentra una coincidencia, imprime la línea
            printf("%s", line);
        } else if (result != REG_NOMATCH) {
            // si hay un error en la expresión regular, muestra un mensaje de error y sale del programa
            fprintf(stderr, "Error en expresión regular\n");
            exit(1);
        }
        // actualiza el desplazamiento en el archivo
        offset += strlen(line);
        if (offset >= data->size) {
            // si el desplazamiento es mayor o igual al tamaño del buffer, se lee más datos del archivo
            offset = 0;
            fseek(data->file, -1 * data->size, SEEK_CUR);
            data->size = fread(data->buffer, 1, BUFFER_SIZE, data->file);
            if (data->size == 0) {
                // si no se pueden leer más datos del archivo, cierra el archivo y libera el buffer
                fclose(data->file);
                free(data->buffer);
                break;
            }
        }
    }
    // libera la memoria asignada a la línea
    free(line);
    return NULL;
}

int main(int argc, char *argv[]) {
    // Verificar que se han proporcionado suficientes argumentos
    if (argc < 3) {
        fprintf(stderr, "Uso: %s [patron] [archivo...]\n", argv[0]);
        return 1;
    }

    // Compilar la expresión regular a partir del patrón proporcionado por el usuario
    regex_t regex;
    if (regcomp(&regex, argv[1], 0) != 0) {
        fprintf(stderr, "Error en expresión regular\n");
        return 1;
    }

    // Declarar estructuras de datos para cada hilo y para el archivo
    thread_data_t data[MAX_THREADS];
    pthread_t threads[MAX_THREADS];
    int num_threads = 0;

    // Procesar cada archivo proporcionado por el usuario
    for (int i = 2; i < argc; i++) {
        // Abrir el archivo
        FILE *file = fopen(argv[i], "r");
        if (file == NULL) {
            fprintf(stderr, "No se pudo abrir archivo %s\n", argv[i]);
            continue;
        }

        // Leer el archivo en bloques hasta que se llegue al final
        while (!feof(file)) {
            // Si ya se han creado el número máximo de hilos permitidos, esperar a que terminen antes de continuar
            if (num_threads == MAX_THREADS) {
                for (int j = 0; j < MAX_THREADS; j++) {
                    pthread_join(threads[j], NULL);
                }
                num_threads = 0;
            }

            // Asignar la estructura de datos correspondiente para el siguiente hilo
            data[num_threads].file = file;
            data[num_threads].buffer = malloc(BUFFER_SIZE);
            data[num_threads].size = fread(data[num_threads].buffer, 1, BUFFER_SIZE, file);
            data[num_threads].regex = regex;

            // Crear el siguiente hilo y aumentar el contador
            pthread_create(&threads[num_threads], NULL, worker, &data[num_threads]);
            num_threads++;
        }
    }

    // Esperar a que terminen todos los hilos antes de liberar memoria
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Liberar la memoria utilizada por la expresión regular
    regfree(&regex);

    // Liberar la memoria utilizada por las estructuras de datos de los hilos y cerrar los archivos
    for (int i = 0; i < num_threads; i++) {
        free(data[i].buffer);
        fclose(data[i].file);
    }

    return 0;
}
