#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>

#define PORT 3535
#define IP_SERVER "127.0.0.1"

int clientfd;
struct sockaddr_in client;
ssize_t bytesRead, bytesSent;

struct dogType
{
    long int id;
    char nombre[32];
    char tipo[32];
    int edad;
    char raza[16];
    int estatura;
    float peso;
    char sexo;
};

void sig_handler(int signo)
{
    if (signo == SIGINT)
        printf("\nHa salido del programa Servidor...\n");
    close(clientfd);
    _exit(0);
}

void solicitarConexion()
{
    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket does not created.");
        return;
    }

    client.sin_family = AF_INET;
    client.sin_port = htons(PORT);
    client.sin_addr.s_addr = inet_addr(IP_SERVER);

    while (connect(clientfd, (struct sockaddr *)&client, (socklen_t)sizeof(struct sockaddr)) != -1)
    {
        perror("Intentando conectar con el servidor... ");
        sleep(1);
    }
    printf("Conexión exitosa\n");
}

void enviarMensaje(void *mensaje, size_t len)
{
    if ((bytesSent = send(clientfd, mensaje, len, 0)) == -1)
    {
        perror("Error enviando el mensaje.");
        return;
    }
}

void recibirMensaje(void *mensaje, size_t len)
{
    if ((bytesRead = recv(clientfd, mensaje, len, 0)) <= 0)
    {
        perror("Error recibiendo el mensaje");
    }
}

void pasarMinusculas(char cadena[])
{
    for (int indice = 0; cadena[indice] != '\0'; ++indice)
    {
        cadena[indice] = tolower(cadena[indice]);
    }
}

void ingresarRegistro(void *ap)
{
}
void verRegistro()
{
}
void borrarRegistro()
{
    long int idBorrar;
    long int rec[2];
    recibirMensaje(&rec, sizeof(rec));

    printf("Existe un total de : %ld  Registros\n", rec[0]);
    //----------------BUSQUEDA DEL REGISTRO-----------------------
    printf("Digite el id de la Mascota a eliminar:\n");
    scanf("%ld", &idBorrar);
    while (idBorrar < 0 || idBorrar >= rec[1])
    {
        printf("Digite un id Valido para eliminar:\n");
        scanf("%ld", &idBorrar);
    }

    enviarMensaje(&idBorrar, sizeof(idBorrar));

    long int idBorrado;
    recibirMensaje(&idBorrado, sizeof(int));

    if (idBorrado == -1)
    {
        printf("Este Id no existe en la base de datos (el registro ya fue eliminado)\n");
    }
    else
    {
        printf("\nSe eliminó el registro %ld Correctamente!\n", idBorrar);
    }
}

void buscarRegistro()
{

    char nombre[32];
    printf("Digite nombre a buscar:\n");
    scanf("%s", nombre);
    pasarMinusculas(nombre);
    enviarMensaje(nombre, sizeof(nombre));
    struct dogType *mascota;

    long int idRecibido;
    int contador = 0;

    while (1)
    {
        recibirMensaje(&idRecibido, sizeof(idRecibido));
        if (idRecibido == -2)
        {
            if (contador == 0)
            {
                printf("\nNo existen registros con el nombre: %s\n", nombre);
            }
            printf("\nFin de la operación\n");
            break;
        }

        printf("Id: %ld    Nombre: %s\n", idRecibido, nombre);
        contador++;
    }
}

void mostrarMenu()
{
    //Variables a Utilizar
    FILE *archivo;
    int decision = 0;
    struct dogType *dato;
    dato = malloc(sizeof(struct dogType));
    int programaCorriendo = 1;

    while (programaCorriendo == 1)
    {
        //Menu Inicial
        printf("********* Veterinaria *********\n");
        printf("Escriba el número correspondiente a la opción que desee y luego presione Enter:\n");
        printf("1. Ingresar registro\n");
        printf("2. Ver registro\n");
        printf("3. Borrar registro\n");
        printf("4. Buscar registro\n");
        printf("5. Salir\n");
        printf("\nOpción: ");

        scanf("%i", &decision);
        enviarMensaje(&decision, sizeof(decision));

        switch (decision)
        {
        case 1:
            ingresarRegistro((void *)dato);
            break;
        case 2:
            verRegistro();
            break;
        case 3:
            borrarRegistro();
            break;
        case 4:
            buscarRegistro();
            break;
        case 5:
            programaCorriendo = 0;
            break;
        default:
            printf("No es una opción valida. Volverá al Menú principal\n");
            break;
        }

        printf("\nPulse ENTER para terminar este apartado\n"); //corregir para que funcione con cualquier tecla
        while (getchar() != '\n')
            ;
        getchar(); // wait for ENTER
    }
}

int main(int argc, char *argv[])
{
    if (signal(SIGINT, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGINT\n");

    int r;

    char buffer[32];

    solicitarConexion();
    //while (1)
    mostrarMenu();

    //r = recv(clientfd, buffer, 32, 0);
    //buffer[r] = 0;
    //printf("\nMensaje recibido: %s\n", buffer);

    //r = send(clientfd, "hola servidor", 13, 0);

    close(clientfd);
}