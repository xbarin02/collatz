#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
/*#include <arpa/inet.h>*/
#include <strings.h>

const uint16_t serverport = 5006;

int main(/*int argc, char *argv[]*/)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serv_addr;

	if (fd < 0) {
		perror("socket");
		abort();
	}

	/* TODO setsockospt */

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(serverport);

	if (bind(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("bind");
		abort();
	}

	if (listen(fd, 10) < 0) {
		perror("listen");
		abort();
	}

	while (1) {
		int cl_fd = accept(fd, NULL, NULL);

		printf("connected\n");

		close(cl_fd);
	}

	return 0;
}
