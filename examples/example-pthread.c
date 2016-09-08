/**
 * This example illustrates the usage of a `turtle_client` in a multithreaded
 * application using `pthread` and `mutex`. A set of `N_THREADS` clients
 * concurrently access to the ASTER-GDEM2 elevation data over a 2x2 deg^2
 * angular grid.
 */

/* C89 standard library. */
#include <stdlib.h>
#include <stdio.h>

/* POSIX threads. */
#include <pthread.h>
#include <semaphore.h>

/* The TURTLE API. */
#include "turtle.h"

/* The number of threads/clients to run. */
#define N_THREADS 16

/**
 * The thread specific data are stored in a dedicated structure. Each thread
 * will track the elevation data along a randomly generated line going from
 * `(latitude_0, longitude_0)` to `(latitude_1, longitude_1)`.
 */
/* Storage for thread specific data. */
struct thread_data {
	pthread_t tid;
	double latitude_0;
	double longitude_0;
	double latitude_1;
	double longitude_1;
};

/**
 * The geodetic datum is declared as a global static variable, to be seen
 * by all threads/clients.
 */
/* Handle for the datum. */
struct turtle_datum * datum = NULL;

/**
 * In order to lock critical sections a semaphore is used. The lock and
 * unlock callbacks simply encapsulate the sem_wait and sem_post functions.
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
 * The following implements a threaded task. Each thread instanciates its own
 * client in order to concurrently access the datum elevation data. Then the
 * client is used to estimate the elevation at successive steps along a line.
 */
static void * run_thread(void * args)
{
	/* Unpack arguments and create the client. */
	struct thread_data * data = (struct thread_data *)args;
	struct turtle_client * client;
	turtle_client_create(datum, &client);

	/* Step along the track. */
	const int n = 1001;
	int i;
	for (i = 0; i < n; i++) {
		const double latitude = data->latitude_0+(i*(data->latitude_1-
			data->latitude_0))/(n-1);
		const double longitude = data->longitude_0+(i*(data->longitude_1
			-data->longitude_0))/(n-1);
		double elevation;
		turtle_client_elevation(client, latitude, longitude,
			&elevation);
		fprintf(stdout, "[%02ld] %.3lf %.3lf %.3lf\n", (long)data->tid,
			latitude, longitude, elevation);
	}

	/* Clean and exit. */
	turtle_client_destroy(&client);
	pthread_exit(0);
}

/**
 * Finally the threads are spawned in the main function after various
 * initialisation. For this example to work, you'll need to get the
 * ASTER-GDEM2 elevation data for the following 4 tiles:
 *
 *     * ASTGMT2_N45E002_dem.tif
 *     * ASTGMT2_N45E003_dem.tif
 *     * ASTGMT2_N46E002_dem.tif
 *     * ASTGMT2_N46E003_dem.tif
 *
 * The tiles should be extracted to a folder named ASTGMT2.
 *
 * **Note** that since there are only 4 tiles that can be accessed, setting the
 * datum `stack_size` to 4 or more will result in speed up since then there is
 * no need to switch, i.e. unload and reload, tiles between threads.
 */
/* A basic error handler with an abrupt exit(). */
void error_handler(enum turtle_return rc, turtle_caller_t * caller)
{
	fprintf(stderr, "error in %s (%s) : %s.\n", __FILE__,
		turtle_strfunc(caller), turtle_strerror(rc));
	pthread_exit(0);
}

/* Draw a random number uniformly in [0;1] using the standard C library. */
static double uniform(void)
{
	return ((double)rand())/RAND_MAX;
}

int main()
{
	/* Initialise the semaphore, the TURTLE library and the datum. */
	sem_init(&semaphore, 0, 1);
	turtle_initialise(error_handler);
	turtle_datum_create(
		"../turtle-data/ASTGTM2", /* <= The elevation data folder. */
		2,                        /* <= The stack size for tiles.  */
		lock, unlock,             /* <= The lock/unlock callbacks. */
		&datum);

	/*
	 * Create the client threads and initialise the thread specific data
	 * randomly.
	 */
	struct thread_data data[N_THREADS];
	int i;
	for (i = 0; i < N_THREADS; i++) {
		if (uniform() <= 0.5) {
			data[i].latitude_0 = 45.;
			data[i].latitude_1 = 47.;
			data[i].longitude_0 = 2.+2.*uniform();
			data[i].longitude_1 = 2.+2.*uniform();
		}
		else {
			data[i].latitude_0 = 45.+2.*uniform();
			data[i].latitude_1 = 45.+2.*uniform();
			data[i].longitude_0 = 2.;
			data[i].longitude_1 = 4.;
		}
		if (pthread_create(&(data[i].tid), NULL, run_thread, data+i)
			!= 0) goto clean_and_exit;
	}

	/* Wait for all threads to finish. */
	for (i = 0; i < N_THREADS; i++) {
		if (pthread_join(data[i].tid, NULL) != 0)
			goto clean_and_exit;
	}

	/* Finalise TURTLE and the semaphore. */
clean_and_exit:
	turtle_datum_destroy(&datum);
	turtle_finalise();
	sem_destroy(&semaphore);
	pthread_exit(0);
}
