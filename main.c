#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

int server_fd;
fd_set fds;
int max_fd;
struct s_client {
	int fd;
	int id;
	char *buff;

};
struct s_client* clients[1024];




void print(int fd, const char *str)
{
	write(fd, str, strlen(str));
}

void free_client(struct s_client* cli)
{
	if (cli->buff != NULL)
	{
		free(cli->buff);
		cli->buff = NULL;
	}
	if (cli->fd > 0)
		close(cli->fd);
	free(cli);
}

void free_memory()
{
	int i = 0;
	while(clients[i] != NULL)
	{
		free_client(clients[i]);
		clients[i] = NULL;
		i++;
	}
	close(server_fd);
}

void fatal(void) {
	print(2, "Fatal error\n");
	free_memory();
	exit(1);
}

void new_client()
{
	static int nb = 0;

	int new_fd = accept(server_fd, NULL, NULL);
	if (new_fd == -1)
		fatal();
	struct s_client* cli = malloc(sizeof(struct s_client));
	if (cli == NULL)
		fatal();
	cli->fd = new_fd;
	cli->id = nb++; 
	FD_SET(cli->fd, &fds);
	if (max_fd < cli->fd)
		max_fd = cli->fd;
	int i = 0;
	while (i < 1024)
	{
		if (clients[i] != NULL){
			clients[i] = cli;
			break;
		}	
		i++;
	}
	
}

void read_client(struct s_client* cli)
{

}

int main (int ac, char **av)
{
	if (ac != 2) {
		print(2, "Wrong number of arguments\n");
		return 1;
	}

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1)
		fatal();

	struct sockaddr_in addr;
	struct sockaddr *ptdr = (struct sockaddr *)&addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(av[1]));
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(server_fd, ptdr, sizeof(struct sockaddr_in)) == -1)
		fatal();
	if (listen(server_fd, SOMAXCONN) == -1)
		fatal();

	FD_ZERO(&fds);
	FD_SET(server_fd, &fds);
	
	max_fd = server_fd;
	while(1)
	{
		int ready = select(max_fd + 1, &fds, NULL, NULL, NULL);
		if (ready == -1)
			fatal();
		if (FD_ISSET(server_fd, &fds))
			new_client();
		int i = 0;
		while (i < 1024	)
		{
			if (clients[i] == NULL)
				continue;
			if (FD_ISSET(clients[i]->fd, &fds))
			{
				read_client(clients[i]);	
			}
			i++;
		}
	}	

	return 0;
}
