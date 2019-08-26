#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#ifdef _OPENMP
	#include <omp.h>
#endif

int threads_get_thread_id()
{
#ifdef _OPENMP
	return omp_get_thread_num();
#else
	return 0;
#endif
}

int main(int argc, char *argv[])
{
	int threads = (argc > 1) ? atoi(argv[1]) : 1;

	assert(threads > 0);

	printf("starting %u worker threads...\n", threads);

	#pragma omp parallel num_threads(threads)
	{
		printf("thread %i started\n", threads_get_thread_id());

		do {
			/* TODO get assignment from server */
			/* TODO spawn sub-process */
			/* TODO send the result back to server */
		} while (1);
	}

	return 0;
}
