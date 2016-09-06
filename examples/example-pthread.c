/* C89 standard library. */
#include <stdlib.h>
#include <stdio.h>

/* POSIX threads. */
#include <pthread.h>
#include <semaphore.h>

/* The TURTLE API. */
#include "turtle.h"

#define N_THREADS 16

/* Storage for thread specific data. */
struct thread_data {
	pthread_t tid;
	enum turtle_return rc;
	double latitude_0;
	double longitude_0;
	double latitude_1;
	double longitude_1;
};

/* Handle for the datum. */
struct turtle_datum * datum = NULL;

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

/* Run a thread. */
static void * run_thread(void * args)
{
	struct thread_data * data = (struct thread_data *)args;
	struct turtle_client * client = NULL;
	enum turtle_return rc = turtle_client_create(datum, &client);
	if (rc != TURTLE_RETURN_SUCCESS) goto clean_and_exit;

	const int n = 1001;
	int i;
	for (i = 0; i < n; i++) {
		const double latitude = data->latitude_0+(i*(data->latitude_1-
			data->latitude_0))/(n-1);
		const double longitude = data->longitude_0+(i*(data->longitude_1
			-data->longitude_0))/(n-1);
		double elevation;
		rc = turtle_client_elevation(client, latitude, longitude,
			&elevation);
		if (rc != TURTLE_RETURN_SUCCESS) goto clean_and_exit;
		fprintf(stdout, "[%02ld] %.3lf %.3lf %.3lf\n", (long)data->tid,
			latitude, longitude, elevation);
	}

clean_and_exit:
	data->rc = rc;
	turtle_client_destroy(&client);
	pthread_exit(0);
}

/* Draw a random number in [0;1] using the standard C library. */
static double uniform(void)
{
	return ((double)rand())/RAND_MAX;
}

int main()
{
	/* Initialise. */
	sem_init(&semaphore, 0, 1);
	turtle_initialise();
	enum turtle_return rc = turtle_datum_create("../turtle-data/ASTGTM2",
		2, lock, unlock, &datum);
	if (rc != TURTLE_RETURN_SUCCESS) {
		fprintf(stderr, "%s\n", turtle_strerror(rc));
		goto clean_and_exit;
	}

	/* Create the threads. */
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
		if (data[i].rc != TURTLE_RETURN_SUCCESS)
			fprintf(stderr, "[%ld] %s\n", (long)data[i].tid,
				turtle_strerror(data[i].rc));
	}

	/* Finalise. */
clean_and_exit:
	rc = turtle_datum_destroy(&datum);
	if (rc != TURTLE_RETURN_SUCCESS)
		fprintf(stderr, "%s\n", turtle_strerror(rc));
	turtle_finalise();
	sem_destroy(&semaphore);
	pthread_exit(0);
}
