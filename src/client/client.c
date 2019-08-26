#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	unsigned threads = (argc > 1) ? (unsigned)atoi(argv[1]) : 1;

	printf("starting %u worker threads...\n", threads);

	return 0;
}
