#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

#define PORT 3535
#define BACKLOG 2 //Numero máximo de clientes

int serverfd = 0;
int clientfd = 0;
struct sockaddr_in server, client;
ssize_t bytesRead,bytesSent;

void sig_handler(int signo)
{
    if (signo == SIGINT)
        printf("\nHa salido del programa Servidor...\n");
    close(clientfd);
    close(serverfd);
    _exit(0);
}

void iniciarServidor()
{
    if ((serverfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket does not created.");
        return;
    }

    int option = 1;
    //setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&option, sizeof(int));

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(server.sin_zero, 8);

    if (bind(serverfd, (struct sockaddr *)&server, sizeof(struct sockaddr)) != 0)
    {
        perror("Error while binding the socket with address and port.");
        return;
    }

    if (listen(serverfd, BACKLOG) != 0)
    {
        perror("Error while listening.");
        return;
    }
}

//devuelve 1 si es true y 0 si es falso
int aceptarConexion()
{
    //addrlen = sizeof(dummyAddr);
    //socketClient = accept(socketServer, (struct sockaddr*) &dummyAddr, &addrlen);

    socklen_t tamano;
    clientfd = accept(serverfd, (struct sockaddr *)&client, &tamano);

    if (clientfd == -1)
    {
        perror("Error with connection.");
        //memset(ipDirection, -1, sizeof(ipDirection));
        return 0;
    }

    /*
    ipv4Addr = (struct sockaddr_in *)&client;
    ipAddr = ipv4Addr->sin_addr;

    inet_ntop(AF_INET, &ipAddr, ipDirection, sizeof(ipDirection));
    */
    return 1;
}

void recibirMensaje(void *pointer, size_t len)
{
    if ((bytesRead = read(clientfd, pointer, len)) <= 0){
        perror("Error receiving menu option");
    }
}

int main()
{
    if (signal(SIGINT, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGINT\n");

    iniciarServidor();

    int option = 0;

    while (true)
    {
        while (aceptarConexion() == 1)
        {
            while (option != 5)
            {
                recibirMensaje(&option, sizeof(int));
                printf("\nOpcion: %i\n", option);
                //selectOption(option);
            }
            option = 0;
        }
    }

    int r;

    r = send(clientfd, "hola mundo", 10, 0);
    if (r < 0)
    {
        perror("\n-->Error en send(): ");
        exit(-1);
    }


    close(clientfd);
    close(serverfd);
}