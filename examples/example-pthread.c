/*
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org>
 */

/*
 * This example illustrates the usage of a `turtle_client` in a multithreaded
 * application using `pthread` and `mutex`. A set of `N_THREADS` clients
 * concurrently access data of a Global Digital Elevation Model (GDEM) over a
 * 2x2 deg^2 angular grid.
 * 
 * Note that for this example to work you'll need the 4 tiles at (45N, 2E),
 * (45N, 3E), (46N, 2E), and (46N, 3E) from a global model, e.g. N45E002.hgt,
 * etc ... for SRTMGL1. These tiles are assumed to be located in a folder named
 * `share/topography`.
 */

/* C89 standard library */
#include <stdio.h>
#include <stdlib.h>
/* POSIX threads */
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
/* The TURTLE library */
#include "turtle.h"

/* The number of threads/clients to run */
#define N_THREADS 4

/* Handle for the stack of maps, from the GDEM */
struct turtle_stack * stack = NULL;

/* Storage for thread parameters */
struct thread_parameters {
        pthread_t tid;
        double latitude_0;
        double longitude_0;
        double latitude_1;
        double longitude_1;
};

/* The thread `run` function */
static void * run_thread(void * args)
{
        /* Prototypes for lock & unlock functions, securing concurent accesss */
        int lock(void);
        int unlock(void);

        /* Unpack arguments and create the client */
        struct thread_parameters * params = (struct thread_parameters *)args;
        struct turtle_client * client;
        turtle_client_create(&client, stack);

        /* Step along the track */
        const int n = 1001;
        int i;
        for (i = 0; i < n; i++) {
                const double latitude = params->latitude_0 +
                    (i * (params->latitude_1 - params->latitude_0)) / (n - 1);
                const double longitude = params->longitude_0 +
                    (i * (params->longitude_1 - params->longitude_0)) / (n - 1);
                double elevation;
                int inside;
                turtle_client_elevation(
                    client, latitude, longitude, &elevation, &inside);
                lock();
                fprintf(stdout, "[%02ld] %.3lf %.3lf %.3lf\n",
                    (long)params->tid, latitude, longitude, elevation);
                unlock();
                if (!inside) break;
        }

        /* Clean and exit */
        turtle_client_destroy(&client);
        pthread_exit(0);
}

/* Semaphore for locking critical sections */
static sem_t * semaphore;

int lock(void)
{
        /* Get the lock */
        return sem_wait(semaphore);
}

int unlock(void)
{
        /* Release the lock. */
        return sem_post(semaphore);
}

/* Draw a random number uniformly in [0;1] using the standard C library */
static double uniform(void) { return ((double)rand()) / RAND_MAX; }

/* The main function, spawning the threads */
int main()
{
        /* Initialise the semaphore and the stack */
        semaphore = sem_open("/semaphore", O_CREAT | O_EXCL, 0644, 1);
        sem_unlink("/semaphore");
        turtle_stack_create(&stack, "share/topography", 0, &lock, &unlock);

        /*
         * Create the client threads and initialise the thread specific data
         * randomly
         */
        srand(time(NULL));
        struct thread_parameters params[N_THREADS];
        int i;
        for (i = 0; i < N_THREADS; i++) {
                if (uniform() <= 0.5) {
                        params[i].latitude_0 = 45.;
                        params[i].latitude_1 = 47.;
                        params[i].longitude_0 = 2. + 2. * uniform();
                        params[i].longitude_1 = 2. + 2. * uniform();
                } else {
                        params[i].latitude_0 = 45. + 2. * uniform();
                        params[i].latitude_1 = 45. + 2. * uniform();
                        params[i].longitude_0 = 2.;
                        params[i].longitude_1 = 4.;
                }
                if (pthread_create(
                        &(params[i].tid), NULL, run_thread, params + i) != 0)
                        goto clean_and_exit;
        }

        /* Wait for all threads to finish */
        for (i = 0; i < N_THREADS; i++) {
                if (pthread_join(params[i].tid, NULL) != 0) goto clean_and_exit;
        }

        /* Finalise the semaphore, clean the stack and exit to the OS */
clean_and_exit:
        turtle_stack_destroy(&stack);
        sem_close(semaphore);
        exit(EXIT_SUCCESS);
}
