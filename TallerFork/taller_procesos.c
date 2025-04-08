/**********************************************
 * Autor: Samuel Gantiva
 * Institución: Pontificia Universidad Javeriana
 * Fecha: 13/02/2025
 * Clase: Sistemas Operativos
 * Tema: - Memoria dinámica, fork, pipes
 *       - Comunicación entre procesos
 *       - Compilación Automática
 * Programa Principal: Suma de Archivos
 * Descripción: Este programa lee dos archivos con arreglos de enteros,
 *              utiliza procesos (fork) para calcular sumas parciales y total,
 *              y emplea tuberías (pipes) para comunicar los resultados al padre.
 ***********************************************/

 #include <stdio.h>
 #include <stdlib.h>
 #include <unistd.h>
 #include <sys/wait.h>
 #include <fcntl.h>
 
 /**
  * Función: read_file
  * Descripción: Lee n enteros desde un archivo y los almacena en un arreglo dinámico.
  * Parámetros:
  *   - filename: Nombre del archivo a leer.
  *   - n: Cantidad de enteros esperados.
  *   - success: Puntero para indicar si la operación fue exitosa (1) o falló (0).
  * Retorno: Puntero al arreglo dinámico o NULL si falla.
  */
 int *read_file(const char *filename, int n, int *success) {
     FILE *file = fopen(filename, "r");
     if (!file) {
         perror("Error al abrir el archivo");
         *success = 0;
         return NULL;
     }
 
     // Reserva memoria dinámica para el arreglo
     int *array = (int *)malloc(n * sizeof(int));
     if (!array) {
         perror("Fallo en la asignación de memoria");
         fclose(file);
         *success = 0;
         return NULL;
     }
 
     // Lee los enteros del archivo
     for (int i = 0; i < n; i++) {
         if (fscanf(file, "%d", &array[i]) != 1) {
             fprintf(stderr, "Error al leer el entero %d de %s\n", i, filename);
             free(array);
             fclose(file);
             *success = 0;
             return NULL;
         }
     }
 
     fclose(file);
     *success = 1;
     return array;
 }
 
 /**
  * Función: calculate_sum
  * Descripción: Calcula la suma de los elementos de un arreglo.
  * Parámetros:
  *   - array: Puntero al arreglo de enteros.
  *   - n: Cantidad de elementos en el arreglo.
  * Retorno: Suma total de los elementos.
  */
 int calculate_sum(int *array, int n) {
     int sum = 0;
     for (int i = 0; i < n; i++) {
         sum += array[i];
     }
     return sum;
 }
 
 /**
  * Función: main
  * Descripción: Programa principal que coordina la lectura de archivos,
  *              la creación de procesos y la comunicación vía pipes.
  * Parámetros:
  *   - argc: Cantidad de argumentos.
  *   - argv: Arreglo de argumentos (N1, archivo00, N2, archivo01).
  * Retorno: 0 si es exitoso, otro valor si falla.
  */
 int main(int argc, char *argv[]) {
     // Verifica la cantidad de argumentos
     if (argc != 5) {
         fprintf(stderr, "Uso: %s N1 archivo00 N2 archivo01\n", argv[0]);
         exit(EXIT_FAILURE);
     }
 
     // Parsea los argumentos
     int N1 = atoi(argv[1]);
     char *file00 = argv[2];
     int N2 = atoi(argv[3]);
     char *file01 = argv[4];
 
     if (N1 <= 0 || N2 <= 0) {
         fprintf(stderr, "N1 y N2 deben ser enteros positivos\n");
         exit(EXIT_FAILURE);
     }
 
     // Lee los archivos en arreglos dinámicos
     int success;
     int *array00 = read_file(file00, N1, &success);
     if (!success) exit(EXIT_FAILURE);
     int *array01 = read_file(file01, N2, &success);
     if (!success) {
         free(array00);
         exit(EXIT_FAILURE);
     }
 
     // Crea tuberías para comunicación entre procesos
     int pipe_grandchild[2]; // Del nieto al padre
     int pipe_second[2];     // Del segundo hijo al padre
     int pipe_first[2];      // Del primer hijo al padre
 
     if (pipe(pipe_grandchild) == -1 || pipe(pipe_second) == -1 || pipe(pipe_first) == -1) {
         perror("Fallo al crear las tuberías");
         free(array00);
         free(array01);
         exit(EXIT_FAILURE);
     }
 
     // Primer fork: Crea el primer hijo
     pid_t pid1 = fork();
     if (pid1 < 0) {
         perror("Fallo en el fork");
         exit(EXIT_FAILURE);
     }
 
     if (pid1 == 0) { // Primer hijo
         close(pipe_first[0]); // Cierra el extremo de lectura no usado
 
         // Segundo fork: Crea el nieto
         pid_t pid_grand = fork();
         if (pid_grand < 0) {
             perror("Fallo en el fork");
             exit(EXIT_FAILURE);
         }
 
         if (pid_grand == 0) { // Nieto
             close(pipe_second[0]);
             close(pipe_second[1]);
             close(pipe_first[1]);
             close(pipe_grandchild[0]); // Cierra el extremo de lectura
 
             // Calcula la suma del primer archivo y la envía al padre
             int sumA = calculate_sum(array00, N1);
             write(pipe_grandchild[1], &sumA, sizeof(int));
             close(pipe_grandchild[1]);
 
             free(array00);
             free(array01);
             exit(EXIT_SUCCESS);
         }
 
         // Tercer fork: Crea el segundo hijo
         pid_t pid2 = fork();
         if (pid2 < 0) {
             perror("Fallo en el fork");
             exit(EXIT_FAILURE);
         }
 
         if (pid2 == 0) { // Segundo hijo
             close(pipe_grandchild[0]);
             close(pipe_grandchild[1]);
             close(pipe_first[1]);
             close(pipe_second[0]); // Cierra el extremo de lectura
 
             // Calcula la suma del segundo archivo y la envía al padre
             int sumB = calculate_sum(array01, N2);
             write(pipe_second[1], &sumB, sizeof(int));
             close(pipe_second[1]);
 
             free(array00);
             free(array01);
             exit(EXIT_SUCCESS);
         }
 
         // Primer hijo: Calcula la suma total y la envía al padre
         int total_sum = calculate_sum(array00, N1) + calculate_sum(array01, N2);
         write(pipe_first[1], &total_sum, sizeof(int));
         close(pipe_first[1]);
 
         // Cierra las tuberías después de que los hijos las usen
         close(pipe_grandchild[0]);
         close(pipe_grandchild[1]);
         close(pipe_second[0]);
         close(pipe_second[1]);
 
         // Espera a que el nieto y el segundo hijo terminen
         wait(NULL);
         wait(NULL);
 
         free(array00);
         free(array01);
         exit(EXIT_SUCCESS);
     }
 
     // Proceso padre
     close(pipe_grandchild[1]); // Cierra los extremos de escritura
     close(pipe_second[1]);
     close(pipe_first[1]);
 
     // Lee los resultados de las tuberías
     int sumA, sumB, total_sum;
     read(pipe_grandchild[0], &sumA, sizeof(int));
     read(pipe_second[0], &sumB, sizeof(int));
     read(pipe_first[0], &total_sum, sizeof(int));
 
     close(pipe_grandchild[0]); // Cierra los extremos de lectura
     close(pipe_second[0]);
     close(pipe_first[0]);
 
     // Imprime los resultados
     printf("Suma del archivo %s (Nieto): %d\n", file00, sumA);
     printf("Suma del archivo %s (Segundo hijo): %d\n", file01, sumB);
     printf("Suma total (Primer hijo): %d\n", total_sum);
 
     // Espera a que el primer hijo termine
     wait(NULL);
 
     // Libera la memoria dinámica
     free(array00);
     free(array01);
     return 0;
 }