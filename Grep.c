#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <regex.h>
#include <time.h>
#define MAX_THREADS 1 // número máximo de hilos
#define BUFFER_SIZE 8192 // tamaño del buffer

typedef struct {
    char *buffer; // buffer para almacenar los datos leídos del archivo
    size_t size; // tamaño del buffer
    size_t pos; // posición actual dentro del buffer
    regex_t regex; // expresión regular que se está buscando
    int thread; //identificador del hilo
} thread_data_t;

pthread_mutex_t mutex_file; // semáforo que garantiza las lecturas únicas del archivo
FILE *_file; // puntero al archivo que se está procesando

// función que realiza la búsqueda en un archivo
void *worker(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    char *line = NULL;
    size_t line_len = 0;
    int result;
    while (1) {
        // busca una línea dentro del buffer
        char *nl = memchr(&data->buffer[data->pos], '\n', data->size - data->pos);
        if (nl != NULL) {
            // se encontró una línea, la copia a un nuevo buffer
            line_len = nl - &data->buffer[data->pos] + 1;
            line = malloc(line_len + 1);
            memcpy(line, &data->buffer[data->pos], line_len);
            line[line_len] = '\0';
            data->pos += line_len;
        } else if (feof(_file)) {
            // si se llegó al final del archivo, sale del ciclo
            break;
        } else {
            // si no se encontró una línea y no se llegó al final del archivo, lee más datos del archivo al buffer
            memmove(data->buffer, &data->buffer[data->pos], data->size - data->pos);
            data->size -= data->pos;
            data->pos = 0;
            //pthread_mutex_lock(&mutex_file); //bloquea el archivo
            size_t n = fread(&data->buffer[data->size], 1, BUFFER_SIZE - data->size, _file);
            //pthread_mutex_unlock(&mutex_file);// quita el bloqueo del archivo
            data->size += n;
            data->buffer[data->size] = '\0';
            continue;
        }
        // realiza la búsqueda en la línea leída usando la expresión regular
        result = regexec(&data->regex, line, 0, NULL, 0);
        if (result == 0) {
            // si la expresión regular encuentra una coincidencia, imprime la línea
            //printf("%d: %s", data->thread, line); // Linea para debuggeo de impresión por hilo
            printf("%s", line);
        } else if (result != REG_NOMATCH) {
            // si hay un error en la expresión regular, muestra un mensaje de error y sale del programa
            fprintf(stderr, "Error en expresión regular\n");
            exit(1);
        }
        // libera la memoria asignada a la línea
        free(line);
        line = NULL;
        line_len = 0;
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    //Variables de medición de tiempo
    clock_t start, end;
    start = clock();

    // Verificar que se han proporcionado suficientes argumentos
    if (argc < 3) {
        fprintf(stderr, "Uso: %s [patron] [archivo...]\n", argv[0]);
        return 1;
    }

    // Compilar la expresión regular a partir del patrón proporcionado por el usuario
    regex_t regex;

    char *exp = argv[1];
    if (regcomp(&regex, exp, 0) != 0) {
        fprintf(stderr, "Error en expresión regular\n");
        return 1;
    }

    // Declarar estructuras de datos para cada hilo y para el archivo
    thread_data_t data[MAX_THREADS];
    pthread_t threads[MAX_THREADS];

    // Abrir el archivo
    char *file_name = argv[2];
    _file = fopen(file_name, "r");;
    pthread_mutex_init(&mutex_file,NULL); // inicializa el semáforo
    if (_file == NULL) {
        fprintf(stderr, "No se pudo abrir archivo %s\n", argv[2]);
        return 0;
    }

    int num_threads = 0;
    // Asignar la estructura de datos correspondiente para el siguiente hilo
    while(num_threads < MAX_THREADS){
        data[num_threads].buffer = malloc(BUFFER_SIZE + 1);
        //pthread_mutex_lock(&mutex_file);
        data[num_threads].size = fread(data[num_threads].buffer, 1, BUFFER_SIZE, _file);
        //pthread_mutex_unlock(&mutex_file);
        data[num_threads].buffer[data[num_threads].size] = '\0';
        data[num_threads].pos = 0;
        data[num_threads].regex = regex;
        data[num_threads].thread = num_threads;

        // Crear el siguiente hilo
        pthread_create(&threads[num_threads], NULL, worker, &data[num_threads]);

        // Aumentar el contador de hilos y verificar si se ha alcanzado el máximo de hilos
        num_threads++;
    }

    // Espera a que todos los hilos hayan terminado antes de continuar
    for (int j = 0; j < num_threads; j++) {
        pthread_join(threads[j], NULL);
    }
    // Libera los buffers
    for (int j = 0; j < num_threads; j++) {
        free(data[j].buffer);
        data[j].buffer = NULL;
    }

    fclose(_file); //Cierra el archivo
    pthread_mutex_destroy(&mutex_file); //Destruye el semáforo  
    

    // Liberar la memoria utilizada por la expresión regular
    regfree(&regex);

    //Se calcula el tiempo de ejecución del proceso
    end = clock();
    double time = (double)(end-start) / CLOCKS_PER_SEC;
    printf("Ejecución: %f seg.\n", time);

    return 0;
}