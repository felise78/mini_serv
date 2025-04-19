#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <errno.h>

#include <stdio.h>

int server_fd = -1;
fd_set fds;
int max_fd = 0;
struct s_client {
    int fd;
    int id;
    char *buff;
    int size;
    int capacity;
};
struct s_client clients[1024];
char *msg_buff = NULL;

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

void free_memory()
{
    if (server_fd != -1)
        close(server_fd);   
    int i = 0;
    while (i < 1024)
    {
        free_client(i);
        i++;
    }
    if (msg_buff != NULL)
        free(msg_buff);
    msg_buff = NULL;

}

void fatal()
{
    print(2, "Error fatal\n ");
    // perror(str);
    free_memory();
    exit(1);
}

void send_2_all_clients(int cli, char *msg, int size)
{
    int i = 0;
    while (i < 1024)
    {
        if (clients[i].fd != -1 && i != cli)
        {
            if (send(clients[i].fd, msg, size, 0) == -1)
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
        close(new_fd);
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
    char msg[128];
    int size = sprintf(msg, "server: client %d just arrived\n", clients[i].id);
    if (size == -1)
        fatal(); 
    send_2_all_clients(i, msg, size);
}


void read_client(int i)
{
    int space = clients[i].capacity - clients[i].size;
    if (space < 1024) {
        if (clients[i].capacity == 0)
            clients[i].capacity = 1024;
        else
            clients[i].capacity *= 2;
        char* new_buff = realloc(clients[i].buff, clients[i].capacity);
        if (new_buff == NULL)
            fatal();
        clients[i].buff = new_buff;
        
    }
    ssize_t r = recv(clients[i].fd, clients[i].buff + clients[i].size, 1024 - 1, 0);
    if (r == -1)
        fatal();
    if (r == 0) {
        char msg[128];
        int size = sprintf(msg, "server: client %d just left\n", clients[i].id);
        if (size == -1)
            fatal(); 
        send_2_all_clients(i, msg, size);
        free_client(i);
        return;
    }
    clients[i].size += r;
    clients[i].buff[clients[i].size] = '\0';
    
    char *ptr = clients[i].buff;
    char *srch = strstr(ptr, "\n");

    while (srch != NULL) {  
        *srch = '\0';
        int len = strlen(ptr);
        char* new_buff = (char*)realloc(msg_buff, len + 128);
        if (new_buff == NULL)
            fatal();
        msg_buff = new_buff;

        // check error
        int ret = sprintf(msg_buff, "client %d: %s\n", clients[i].id, ptr);
        if (ret == -1)
            fatal();

        send_2_all_clients(i, msg_buff, ret);
        
        // Libérer msg_buff à chaque itération pour éviter les fuites
        free(msg_buff);
        msg_buff = NULL;

        ptr = srch + 1;
        srch = strstr(ptr, "\n");
    }

    if (ptr != clients[i].buff) {
        int size = strlen(ptr);
        strcpy(clients[i].buff, ptr);
        clients[i].size = size;
    }

}

int main (int ac, char **av)
{
    if (ac != 2)
    {
        print(2, "Wrong numbers of arguments\n");
        return 1;
    }
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
        fatal();

    struct sockaddr_in addr;
    struct sockaddr *ptr = (struct sockaddr*)&addr;
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
    while (i < 1024) {
        clients[i].fd = -1;
        clients[i].buff = NULL;
        i++;
    }

    while (1)
    {
        fd_set read_fds = fds; 
        int ready = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
       
        if (ready == -1) {
            // Les signaux comme SIGINT causent EINTR
            if (errno == EINTR) {
                // Le signal a interrompu select(), nettoyons et sortons proprement
                free_memory();
                return 0;
            }
            fatal();
        }
            
        if (FD_ISSET(server_fd, &read_fds))
            new_client();

        int i = 0;
        while (i < 1024)
        {
            if (clients[i].fd != -1 && FD_ISSET(clients[i].fd, &read_fds))
                read_client(i);
            i++;
        }
    }

}
