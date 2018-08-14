/**
 * This example illustrates the usage of a `turtle_client` in a multithreaded
 * application using `pthread` and `mutex`. A set of `N_THREADS` clients
 * concurrently access to Global Digitale Elevation Model (GDEM) data over a
 * 2x2 deg^2 angular grid.
 */

/* C89 standard library. */
#include <stdio.h>
#include <stdlib.h>

/* POSIX threads. */
#include <pthread.h>
#include <semaphore.h>

/* The TURTLE API. */
#include "turtle.h"

/* The number of threads/clients to run. */
#define N_THREADS 4

/**
 * First let's implements the threaded task and some storage for its input
 * parameters. Each thread's task instanciates its own client in order to
 * concurrently access the GDEM data. Then the client is used to
 * estimate the elevation at successive steps along a line going from
 * `(latitude_0, longitude_0)` to `(latitude_1, longitude_1)`.
 *
 * *Note* that the stack handle is declared as a global static variable,
 * to be seen by all threads/clients.
 */
/* Handle for the stack. */
struct turtle_stack * stack = NULL;

/* Storage for the thread input parameters. */
struct thread_parameters {
        pthread_t tid;
        double latitude_0;
        double longitude_0;
        double latitude_1;
        double longitude_1;
};

/* The thread `run` function. */
static void * run_thread(void * args)
{
        /* Prototypes for lock & unlock functions, securing concurent accesss */
        int lock(void);
        int unlock(void);

        /* Unpack arguments and create the client. */
        struct thread_parameters * params = (struct thread_parameters *)args;
        struct turtle_client * client;
        turtle_client_create(stack, &client);

        /* Step along the track. */
        const int n = 1001;
        int i;
        for (i = 0; i < n; i++) {
                const double latitude = params->latitude_0 +
                    (i * (params->latitude_1 - params->latitude_0)) / (n - 1);
                const double longitude = params->longitude_0 +
                    (i * (params->longitude_1 - params->longitude_0)) / (n - 1);
                double elevation;
                turtle_client_elevation(
                    client, latitude, longitude, &elevation, NULL);
                lock();
                fprintf(stdout, "[%02ld] %.3lf %.3lf %.3lf\n",
                    (long)params->tid, latitude, longitude, elevation);
                unlock();
        }

        /* Clean and exit. */
        turtle_client_destroy(&client);
        pthread_exit(0);
}

/**
 * Then we need to provide some locking mechanism to TURTLE in order to protect
 * critical sections. That for a semaphore is used. The TURTLE lock and unlock
 * callbacks simply encapsulate the sem_wait and sem_post functions.
 */
/* Semaphore for locking critical sections. */
static sem_t semaphore;

int lock(void)
{
        /* Get the lock */
        return sem_wait(&semaphore);
}

int unlock(void)
{
        /* Release the lock. */
        return sem_post(&semaphore);
}

/**
 * Finally the threads are spawned in the main function after various
 * initialisation. For this example to work, you'll need to get the
 * ASTER-GDEM2 (or SRTM) elevation data for the following 4 tiles:
 *
 * * `ASTGMT2_N45E002_dem.tif`
 * * `ASTGMT2_N45E003_dem.tif`
 * * `ASTGMT2_N46E002_dem.tif`
 * * `ASTGMT2_N46E003_dem.tif`
 *
 * The tiles should be extracted to a folder named share/topography.
 *
 * **Note** that since there are only 4 tiles that can be accessed, setting the
 * stack `size` to 4 or more will result in speed up since then there is
 * no need to switch, i.e. unload and reload, tiles between threads.
 */
/* A basic error handler with an abrupt exit(). */
void error_handler(
    enum turtle_return rc, turtle_function_t * caller, const char * message)
{
        fprintf(stderr, "error: %s.\n", message);
        pthread_exit(0);
}

/* Draw a random number uniformly in [0;1] using the standard C library. */
static double uniform(void) { return ((double)rand()) / RAND_MAX; }

/* The main function, spawning the threads. */
int main()
{
        /* Initialise the semaphore, the TURTLE library and the stack. */
        sem_init(&semaphore, 0, 1);
        turtle_initialise(error_handler);
        turtle_stack_create(
            "share/topography", /* <= The elevation data folder. */
            4,                  /* <= The stack size for tiles.  */
            lock, unlock,       /* <= The lock/unlock callbacks. */
            &stack);

        /*
         * Create the client threads and initialise the thread specific data
         * randomly.
         */
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

        /* Wait for all threads to finish. */
        for (i = 0; i < N_THREADS; i++) {
                if (pthread_join(params[i].tid, NULL) != 0) goto clean_and_exit;
        }

/* Finalise TURTLE and the semaphore. */
clean_and_exit:
        turtle_stack_destroy(&stack);
        turtle_finalise();
        sem_destroy(&semaphore);
        pthread_exit(0);
}
