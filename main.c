#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdio.h>

int server_fd;
fd_set fds;
int max_fd;
struct s_client{
    int fd;
    int id;
    char *buff;
    int size;
    int capacity;
};
struct s_client clients[1024];



void print(int fd, const char *str)
{
    write(fd, str, strlen(str));
}

void free_client(int i)
{
    if (clients[i].buff != NULL) {
        free(clients[i].buff);
        clients[i].buff = NULL;
    }
    if (clients[i].fd != -1) {
        FD_CLR(clients[i].fd, &fds);
        close(clients[i].fd);
        clients[i].fd = -1;
    }
}

void free_memory(void)
{
    if (server_fd != -1)
        close(server_fd);
    int i = 0;
    while (i < 1024)
    {
        free_client(i);
        i++;
    }
}

void fatal()
{
    print(2, "Error fatal\n");
    free_memory();
    exit(1);
}


void broadcast(int sender, char *msg)
{
    int i = 0;
    while (i < 1024)
    {
        if (clients[i].fd != -1 && i != sender)
        {
            if (send(clients[i].fd, msg, strlen(msg), 0) == -1)
                fatal();
        }
        i++;
    }
}

void new_client(void)
{
    static int nb = 0;
    // print(1, "new connection\n");

    int new_fd = accept(server_fd, NULL, NULL);
    if (new_fd == -1)
        fatal();

    int i = 0;
    while (i < 1024)
    {
        if (clients[i].fd != -1)
            i++;
        else
            break;
    }

    if (i == 1024) {
        close (new_fd);
        return;
    }
    
    clients[i].fd = new_fd;
    clients[i].id = nb++;
    if (max_fd < new_fd)
        max_fd = new_fd;
    
    clients[i].buff = NULL;
    clients[i].size = 0;
    clients[i].capacity = 0;

    FD_SET(new_fd, &fds);
    
    char msg[64];
    sprintf(msg, "server: client %d just arrived\n", clients[i].id);
    broadcast(i, msg);

}

void read_client(int i)
{
    // checking buff capacity
    if (clients[i].capacity - clients[i].size < 1024)
    {
        if (clients[i].capacity == 0)
            clients[i].capacity = 1024;
        else
            clients[i].capacity *= 2;
        char *new_buff = realloc(clients[i].buff, clients[i].capacity);
        if (new_buff == NULL)
            fatal();
        clients[i].buff = new_buff;
    }

    // read what has been received
    int size = recv(clients[i].fd, clients[i].buff + clients[i].size, 1024 - 1, 0);
    if (size == -1)
        fatal();
    if (size == 0) // means that client has disconnected
    {
        char msg[64];
        sprintf(msg, "server: client %d just left\n", clients[i].id);
        broadcast(i, msg);
        free_client(i);
        return;
    }

    clients[i].size += size;
    clients[i].buff[clients[i].size] = '\0';

    char *ptr = clients[i].buff;
    char *srch = strstr(ptr, "\n");

    // send to everyone each time there is a '\n'
    while (srch != NULL)
    {
        *srch = '\0';
        char msg[64];
        sprintf(msg, "client %d: ", clients[i].id);
        
        broadcast(i, msg);
        broadcast(i, ptr);
        broadcast(i, "\n");

        ptr = srch + 1;
        srch = strstr(ptr, "\n");
    }

    // if still message left with no '\n'
    if (ptr != clients[i].buff)
    {
        int len = strlen(ptr);
        strcpy(clients[i].buff, ptr);
        clients[i].size = len;
    }
}

int main (int ac, char **av)
{
    if (ac != 2)
    {
        print(2, "Wrong number of arguments\n");
        return 1;
    }
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
        fatal();

    struct sockaddr_in addr;
    struct sockaddr *ptr = (struct sockaddr *)&addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(av[1]));
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, ptr, (sizeof(struct sockaddr_in))) == -1)
        fatal();
    if (listen(server_fd, SOMAXCONN) == -1)
        fatal();

    FD_ZERO(&fds);
    FD_SET(server_fd, &fds);
    max_fd = server_fd;

    int i = 0;
    while (i < 1024)
    {
        clients[i].fd = -1;
        clients[i].buff = NULL;
        i++;
    }

    while (1)
    {
        fd_set cpy_fds = fds;
        
        int ready = select(max_fd + 1, &cpy_fds, NULL, NULL, NULL);
        if (ready == -1)
            fatal();
        if (FD_ISSET(server_fd, &cpy_fds))
            new_client();
        i = 0;
        while (i < 1024)
        {
            if (clients[i].fd != -1 && FD_ISSET(clients[i].fd, &cpy_fds))
                read_client(i);
            i++;
        }
    }
}