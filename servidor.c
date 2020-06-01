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
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <time.h>
#include <string.h>

#define PORT 3535
#define BACKLOG 35 //Numero máximo de clientes

int clientesConectados = 0;

int serverfd;
int clientfd;
struct sockaddr_in server;
ssize_t bytesRead, bytesSent;

//----------------Variables Globales
long int numMaxRegHistorial = 0; // Contador de Estructuras en datadog.dat
long int numRegEliminados = 0;   // Contador de Ids Eliminados
long int numRegTotales = 0;      // Contador Estructuras Historial menos Eliminados

//---------------Estructuras
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

struct nodoType
{
    long int id;
    long int siguiente;
};

struct infoCliente
{
    int clientfd;
    char ip[16];
};

struct Log
{
    char date[20];
    char ip[16];
    char operacion[10];
    char input[50];
};

void sig_handler(int signo)
{
    if (signo == SIGINT)
        printf("\nHa salido del programa Servidor...\n");
    close(clientfd);
    close(serverfd);
    _exit(0);
}

//---------------Servidor (funciones)
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
struct infoCliente aceptarConexion()
{
    //addrlen = sizeof(dummyAddr);
    //socketClient = accept(socketServer, (struct sockaddr*) &dummyAddr, &addrlen);
    struct sockaddr_in client;
    struct infoCliente clienteInfo;

    int clientfd;
    socklen_t tamano;

    clientfd = accept(serverfd, (struct sockaddr *)&client, &tamano);

    if (clientfd == -1)
    {
        perror("Error with connection.");
        //memset(ipDirection, -1, sizeof(ipDirection));
        return clienteInfo;
    }
    else
    {
        clientesConectados++;
        //printf("\nConexión exitosa: Clientes conectados %i\n", clientesConectados);
    }

    struct sockaddr_in *ipv4Addr;
    struct in_addr ipAddr;
    char ipDirection[INET_ADDRSTRLEN];

    ipv4Addr = (struct sockaddr_in *)&client;
    ipAddr = ipv4Addr->sin_addr;

    inet_ntop(AF_INET, &ipAddr, ipDirection, sizeof(ipDirection));
    //printf("%s\n",ipDirection); //127.0.0.1

    clienteInfo.clientfd = clientfd;
    strcpy(clienteInfo.ip, ipDirection);
    //clienteInfo.ip = ipDirection;

    return clienteInfo;
}

void enviarMensaje(int clienteID, void *mensaje, size_t len)
{
    if ((bytesSent = send(clienteID, mensaje, len, 0)) == -1)
    {
        perror("Error enviando el mensaje.");
        return;
    }
}

void recibirMensaje(int clienteID, void *pointer, size_t len)
{
    if ((bytesRead = read(clienteID, pointer, len)) <= 0)
    {
        perror("Error receiving menu option");
    }
}

void enviarArchivo(int clienteID, char nombreHC[])
{
    int size = 1024;
    char buff[1024];
    int bytesRead;
    FILE *f;
    f = fopen(nombreHC, "r+");
    if (f == NULL)
    {
        perror("error en abrir archivo");
    }
    if (fseek(f, 0, SEEK_END) != 0)
    {
        perror("error en seek archivo");
        return;
    }
    long lenght = ftell(f);
    rewind(f);
    do
    {
        if (fread(buff, (lenght < size) ? lenght : size, 1, f) <= 0)
        {
            perror("error en leer el archivo");
            return;
        }
        enviarMensaje(clienteID, buff, (lenght < size) ? lenght : size);
        lenght -= size;
    } while (lenght >= 0);
}

void recibirArchivo(int clienteID, char nombreHC[])
{
    int size = 1024;
    char buff[1024];
    int bytesRead;
    FILE *f;
    f = fopen(nombreHC, "w+");
    if (f == NULL)
    {
        perror("error en abrir archivo");
    }
    do
    {
        bzero(buff, size);
        bytesRead = recv(clienteID, buff, size, 0);
        if (buff[0] == -1)
        {
            printf("Empty file\n");
            break;
        }
        fwrite(buff, bytesRead, 1, f);
    } while (bytesRead == size);
    fclose(f);
}

//----------------Log

struct Log *AuxLog;
FILE *logger;

void iniciarLogger()
{
    AuxLog = malloc(sizeof(struct Log));

    if ((logger = fopen("serverDogs.log", "ab+")) == NULL)
    {
        perror("Error al intentar abrir el archivo Log");
        return;
    }
}

void cerrarLogger()
{
    int r = fclose(logger);
    if (r != 0)
    {
        perror("Error en fclose dataDogs.dat");
        exit(-1);
    }
}

void imprimirLog(struct Log *log)
{
    printf("\nFecha: %s | Cliente %s | Operación %s | Input: %s\n", log->date, log->ip, log->operacion, log->input);
}

void imprimirTodosLog()
{
    iniciarLogger();

    fseek(logger, 0, SEEK_END);
    long ultimaPosicion = ftell(logger);
    fseek(logger, 0, SEEK_SET);

    long cantidadLogs = ultimaPosicion / (sizeof(struct Log));

    for (long i = 0; i < cantidadLogs; i++)
    {
        //bzero(AuxLog, sizeof(AuxLog));
        int r = fread(AuxLog, (sizeof(struct Log)), 1, logger);
        if (r == 0)
        {
            //perror("Error en fread");
            printf("\nFin Log\n");
        }
        else
        {
            imprimirLog(AuxLog);
        }
    }

    cerrarLogger();
}

void guardarRegistroLogger(char ip[], char operacion[], char input[])
{
    iniciarLogger();

    bzero(AuxLog, sizeof(AuxLog));

    time_t tiempo = time(0);
    struct tm *tlocal = localtime(&tiempo);
    char output[20];
    strftime(output, 20, "%Y-%m-%dT%H:%M:%S", tlocal);
    //printf("%s\n", output);

    //AuxLog->date = output;
    //mempcpy(AuxLog->date, output, sizeof(AuxLog->date));
    strcpy(AuxLog->date, output);
    //AuxLog->ip = ip;
    //mempcpy(AuxLog->ip, ip, sizeof(AuxLog->ip));
    strcpy(AuxLog->ip, ip);
    //AuxLog->operacion = operacion;
    //mempcpy(AuxLog->operacion, operacion, sizeof(AuxLog->operacion));
    strcpy(AuxLog->operacion, operacion);
    //AuxLog->input = input;
    //mempcpy(AuxLog->input, input, sizeof(AuxLog->input));
    strcpy(AuxLog->input, input);

    int r = fwrite(AuxLog, sizeof(struct Log), 1, logger);
    if (r == 0)
    {
        perror("Error en fwrite");
        exit(-1);
    }
    else
    {
        //printf("El registro Log fue guardado con Exito\n");
        //imprimirLog(AuxLog);
    }

    cerrarLogger();
}

//----------------Funciones Auxiliares

void obtenerNumReg()
{
    // Variables para Archivos
    FILE *apdata, *aproom;
    int r;
    // Apertura de archivos
    apdata = fopen("dataDogs.dat", "rb+");
    if (apdata == NULL)
    {
        perror("Error in fopen");
    }
    if (!(aproom = fopen("deletedId.dat", "rb+")))
    {
        numRegEliminados = 0;
    }
    else
    {
        // Reposicionamiento del cursor de los archivos
        fseek(aproom, 0, SEEK_END);
        // Obtencion de la cantidad de registros de id eliminados
        numRegEliminados = ftell(aproom) / (sizeof(long int));
        //Cerrar Archivo
        r = fclose(aproom);
        if (r != 0)
        {
            perror("Error en fclose deletedId.dat");
            exit(-1);
        }
    }
    // Reposicionamiento del cursor de los archivos
    fseek(apdata, 0, SEEK_END);
    // Obtencion de la cantidad de registros en datadog
    numMaxRegHistorial = ftell(apdata) / (sizeof(struct dogType));
    // Calculo registros existentes
    numRegTotales = numMaxRegHistorial - numRegEliminados;
    //Cerrar Archivo
    r = fclose(apdata);
    if (r != 0)
    {
        perror("Error en fclose dataDogs.dat");
        exit(-1);
    }
}

void pasarMinusculas(char cadena[])
{
    for (int indice = 0; cadena[indice] != '\0'; ++indice)
    {
        cadena[indice] = tolower(cadena[indice]);
    }
}

float modulo(long int a, long int b)
{
    if (a < 0)
    {
        return a - (b * (int)floor(a / b));
    }
    else
    {
        return a % b;
    }
}

//"enviar" calcularHash(texto, sizeof(texto)-1 o contador);
long int calcularHash(char const *texto, int longitudTexto)
{
    long int hash = 0;
    long int primo = 5303;
    long int x = 7;
    for (int i = longitudTexto - 1; i >= 0; i--)
    {
        int caracter = toascii(texto[i]);
        hash = modulo(((hash * x) + caracter), primo);
    }
    return hash;
}

void guardarEnTablaHash(char const *nombre, int longitudTexto, long int id)
{
    long int hash = calcularHash(nombre, longitudTexto);
    FILE *archivo1;
    FILE *archivo2;
    if ((archivo1 = fopen("tablaHash.bin", "r+")) != NULL)
    {
        //*************LECTURA EN ARCHIVO 1 ************
        rewind(archivo1);                               // "rebobinar", sitúa el cursor de lectura/escritura al principio del archivo.
        long int posEnTabla1 = hash * sizeof(long int); // halló la posición de memoria deseada
        fseek(archivo1, posEnTabla1, SEEK_SET);         // pararme en la posición "posEnTabla1" del archivo desde SEEK_SET (principio del fichero)
        long int posEnTabla2;
        fread(&posEnTabla2, sizeof(long int), 1, archivo1); //guardo posición de la tabla 2 donde se encuentran datos de este hash
        //printf("\n posición en la segunda tabla: %li", posEnTabla2);

        struct nodoType dato;

        if (posEnTabla2 == -1)
        {
            dato.id = id;
            dato.siguiente = -1;

            if ((archivo2 = fopen("datosHash.bin", "a+")) != NULL)
            {
                fseek(archivo2, 0, SEEK_END);  // pararme al final del archivo
                posEnTabla2 = ftell(archivo2); // Guardar posición para escribirla en el archivo 1
                fwrite(&dato, sizeof(dato), 1, archivo2);
            }
            else
            {
                printf("Error al abrir el archivo2\n");
            }
            fclose(archivo2);

            fclose(archivo1);
            if ((archivo1 = fopen("tablaHash.bin", "r+")) != NULL)
            {
                fseek(archivo1, posEnTabla1, SEEK_SET);              // pararme en la posición "posEnTabla1" del archivo desde SEEK_SET (principio del fichero)
                fwrite(&posEnTabla2, sizeof(long int), 1, archivo1); //actualizar con el nuevo apuntador
            }
        }
        else
        {
            //*************LECTURA EN ARCHIVO 2 ************
            if ((archivo2 = fopen("datosHash.bin", "r+")) != NULL)
            {
                rewind(archivo2);                        // "rebobinar", sitúa el cursor de lectura/escritura al principio del archivo.
                fseek(archivo2, posEnTabla2, SEEK_SET);  // pararme en la posición "posEnTabla2" del archivo desde SEEK_SET (principio del fichero)
                fread(&dato, sizeof(dato), 1, archivo2); // Lee el primer nodo que está guardado

                long int siguienteNodo = dato.siguiente;
                long int nodoAnterior = posEnTabla2;
                while (siguienteNodo != -1)
                {
                    rewind(archivo2); // "rebobinar"
                    nodoAnterior = siguienteNodo;
                    fseek(archivo2, siguienteNodo, SEEK_SET); // pararme en la posición "siguienteNodo" del archivo desde SEEK_SET (principio del fichero)
                    fread(&dato, sizeof(dato), 1, archivo2);  // Lee el primer nodo que está guardado
                    siguienteNodo = dato.siguiente;
                }
                fclose(archivo2);

                struct nodoType datoNuevo;
                datoNuevo.id = id;
                datoNuevo.siguiente = -1;

                if ((archivo2 = fopen("datosHash.bin", "a+")) != NULL)
                {
                    fseek(archivo2, 0, SEEK_END);                       // pararme al final del archivo
                    posEnTabla2 = ftell(archivo2);                      // Guardar posición para escribirla en el nodo anterior
                    fwrite(&datoNuevo, sizeof(datoNuevo), 1, archivo2); // Agregar el nuevo nodo
                }
                else
                {
                    printf("Error al abrir el archivo2\n");
                }
                fclose(archivo2);

                if ((archivo2 = fopen("datosHash.bin", "rb+")) != NULL)
                {
                    dato.siguiente = posEnTabla2;
                    fseek(archivo2, nodoAnterior, SEEK_SET);  // pararme en la posición "nodoAnterior" del archivo desde SEEK_SET (principio del fichero)
                    fwrite(&dato, sizeof(dato), 1, archivo2); //actualizar con el nuevo apuntador
                }
                else
                {
                    printf("Error al abrir el archivo \n");
                }
            }
            else
            {
                printf("Error al abrir el archivo 2\n");
            }
            fclose(archivo2);
        }
    }
    else
    { //************Error al abrir el archivo -- por que no existe
        struct nodoType dato;
        dato.id = id;
        dato.siguiente = -1;

        if ((archivo2 = fopen("datosHash.bin", "w+")) != NULL)
        {
            fwrite(&dato, sizeof(dato), 1, archivo2);
        }
        fclose(archivo2);

        long int tablaHash[5303];
        for (int i = 0; i < 5303; i++)
        {
            tablaHash[i] = -1;
        }
        tablaHash[hash] = 0;

        if ((archivo1 = fopen("tablaHash.bin", "w+")) != NULL)
        {
            fwrite(&tablaHash, sizeof(long int), 5303, archivo1);
        }
    }
    fclose(archivo1);
}

void ingresarRegistro(struct infoCliente clienteInfo)
{
    int clienteID = clienteInfo.clientfd;
    // ------------Variables a usar-------------------
    //Variables Lectura Datos
    struct dogType dato;
    //Variables guardar registros
    FILE *apdata, *aproom, *temp;
    int r;
    //Variables id
    long int idTraspaso;
    int idDisponible = 0;
    long int numRegEliminadosAntiguos = numRegEliminados;

    recibirMensaje(clienteID, &dato, sizeof(struct dogType));

    struct dogType *dog;
    dog = &dato;

    apdata = fopen("dataDogs.dat", "rb+");
    if (apdata == NULL)
    {
        perror("Error in fopen");
    }
    //Revisar si hay espacios disponibles borrados anteriormente
    if (aproom = fopen("deletedId.dat", "rb+"))
    {
        //
        temp = fopen("temp.dat", "wb+");
        if (temp == NULL)
        {
            perror("Error en fopen");
        }
        //fseek ubica el puntero al Inicio del archivo
        fseek(aproom, 0, SEEK_SET);
        if (numRegEliminadosAntiguos > 0)
        {
            for (long int i = 0; i < numRegEliminadosAntiguos; i++)
            {
                r = fread(&idTraspaso, sizeof(long int), 1, aproom);
                if (r == 0)
                {
                    perror("Error en fread");
                    exit(-1);
                }
                if (i == 0)
                { // Tomaremos el primer id para asignar el nuevo registro
                    dato.id = idTraspaso;
                    idDisponible = 1;
                    fseek(apdata, (idTraspaso) * (sizeof(struct dogType)), SEEK_SET);
                    //Actualizacion Variables Globales
                    numRegEliminados = numRegEliminados - 1;
                    numRegTotales = numMaxRegHistorial - numRegEliminados;
                }
                else
                {
                    r = fwrite(&idTraspaso, sizeof(long int), 1, temp);
                    if (r == 0)
                    {
                        perror("Error en fwrite temp.dat");
                        exit(-1);
                    }
                }
            }
        }
        //Cerrar Archivo
        r = fclose(aproom);
        if (r != 0)
        {
            perror("Error en fclose deletedId.dat");
            exit(-1);
        }
        r = fclose(temp);
        if (r != 0)
        {
            perror("Error en fclose temp.dat");
            exit(-1);
        }
        //Borrado de Archivo Deleteid y cambio de nombre de temp por deleteid
        system("rm deletedId.dat");
        system("mv temp.dat deletedId.dat");
    }
    // Si no encontro espacios disponibles borrados anteriormente
    if (idDisponible == 0)
    {
        dato.id = numMaxRegHistorial; //asigo el id siguiente al mas grande que exista
        //fseek ubica el puntero al final del archivo
        fseek(apdata, 0, SEEK_END);
        //Actualizacion Variables Globales
        numRegEliminados = 0;
        numMaxRegHistorial = numMaxRegHistorial + 1;
        numRegTotales = numMaxRegHistorial;
    }
    // Proceso de Guardado
    r = fwrite(dog, sizeof(struct dogType), 1, apdata);
    if (r == 0)
    {
        perror("Error en fwrite");
        exit(-1);
    }
    else
    {
        // printf("El registro fue creado con Exito con id: %ld \n", dato.id);
        enviarMensaje(clienteID, &dato.id, sizeof(long int));

        //guardar Log
        char idCHAR[50];
        sprintf(idCHAR, "%ld", dato.id);
        guardarRegistroLogger(clienteInfo.ip, "INSERCION", idCHAR);
        //--
    }
    //Cerrar Archivo
    r = fclose(apdata);
    if (r != 0)
    {
        perror("Error en fclose dataDogs.dat");
        exit(-1);
    }

    //----------------Ubicacion Tabla Hash-----------------------
    pasarMinusculas(dato.nombre);
    int contador = 0;
    while (dato.nombre[++contador] != 0)
        ;
    guardarEnTablaHash(dato.nombre, contador, dato.id);
}

void verRegistro(struct infoCliente clienteInfo)
{
    int clienteID = clienteInfo.clientfd;

    long int env[2];
    env[0] = numRegTotales;
    env[1] = numMaxRegHistorial;
    enviarMensaje(clienteID, &env, sizeof(env));
    //----------------Variables --------------------
    long int idVer; // id a buscar
    int archivo;
    struct dogType dog;
    FILE *apdata;
    char abrir;
    int r;
    char nombreHC[32]; // nombre historia clinica
    char respuesta[32];
    //--------------Verificacion de datos-------------
    // abrir archivo datos
    apdata = fopen("dataDogs.dat", "rb");
    if (apdata == NULL)
    {
        perror("Error in fopen apdata");
    }
    else
    {
        recibirMensaje(clienteID, &idVer, sizeof(idVer));
        //fseek ubica el puntero en el id a ver
        fseek(apdata, (idVer) * (sizeof(struct dogType)), SEEK_SET);
        //fread lee el archivo en la posicion dada
        r = fread(&dog, (sizeof(struct dogType)), 1, apdata);
        if (r == 0)
        {
            perror("Error en fread");
        }
        else
        {
            enviarMensaje(clienteID, &dog, sizeof(struct dogType));

            //guardar Log
            char idCHAR[50];
            sprintf(idCHAR, "%ld", idVer);
            guardarRegistroLogger(clienteInfo.ip, "LECTURA", idCHAR);
            //--

            if (dog.id == -1)
            {
                //printf("El Registro no Existe\n"); // preguntar que hacer aqui
            }
            else
            {
                recibirMensaje(clienteID, &abrir, sizeof(abrir));
                if (abrir == 'S' || abrir == 's')
                {
                    strcpy(respuesta, "abrio historia clinica \n");
                    enviarMensaje(clienteID, &respuesta, sizeof(respuesta));

                    // si existe la historia clinica
                    //concatena el Id con .txt
                    sprintf(nombreHC, "%ld", dog.id);
                    strcat(nombreHC, ".txt");
                    // Usa la libreria de control de archivos, busca si existe el archivo
                    // el primer parametro nombre archivo
                    // segundo parametro cuando se abre el archivo
                    // tercer parametro permisos sobre el archivo
                    archivo = open(nombreHC, O_CREAT | O_WRONLY | O_EXCL, S_IWUSR | S_IRUSR);
                    if (archivo < 0)
                    {
                        // si el archivo ya se creo
                        if (errno == EEXIST)
                        {
                            // enviar nombre de el archivo de la historia clinica
                            enviarMensaje(clienteID, nombreHC, sizeof(nombreHC));
                            //enviar historia clinica
                            enviarArchivo(clienteID, nombreHC);
                            //recibe el archivo actualizado
                            recibirArchivo(clienteID, nombreHC);
                        }
                        else
                        {
                            perror("Error al leer el archivo");
                        }
                    }
                    else
                    {
                        // llena el archivo nuevo con los datos
                        FILE *hc = fopen(nombreHC, "wb+");
                        fprintf(hc, "--------------------------------------------------------------------------\n");
                        fprintf(hc, "--------------------------Historia Clinica--------------------------------\n");
                        fprintf(hc, "Id: %ld \n", dog.id);
                        fprintf(hc, "Nombre: %s\n", dog.nombre);
                        fprintf(hc, "Tipo: %s\n", dog.tipo);
                        fprintf(hc, "Edad: %i \n", dog.edad);
                        fprintf(hc, "Raza: %s\n", dog.raza);
                        fprintf(hc, "Estatura: %i \n", dog.estatura);
                        fprintf(hc, "Peso: %f \n", dog.peso);
                        fprintf(hc, "Sexo: %c \n", dog.sexo);
                        fprintf(hc, "--------------------------------------------------------------------------\n");
                        fclose(hc);
                        enviarMensaje(clienteID, nombreHC, sizeof(nombreHC));
                        enviarArchivo(clienteID, nombreHC);
                        recibirArchivo(clienteID, nombreHC);
                    }
                }

                // no abre historia
                else if (abrir == 'N' || abrir == 'n')
                {
                    strcpy(respuesta, "No abrio historia clinica \n");
                    enviarMensaje(clienteID, &respuesta, sizeof(respuesta));
                }
                else
                {
                    strcpy(respuesta, "Error al ingresar Valores \n");
                    enviarMensaje(clienteID, &respuesta, sizeof(respuesta));
                }
            }
        }
        // cerrar
        fclose(apdata);
    }
}

void borrarRegistro(struct infoCliente clienteInfo)
{
    int clienteID = clienteInfo.clientfd;

    long int env[2];
    env[0] = numRegTotales;
    env[1] = numMaxRegHistorial;
    enviarMensaje(clienteID, &env, sizeof(env));

    long int idBorrar;
    recibirMensaje(clienteID, &idBorrar, sizeof(idBorrar));

    // ------------Variables a usar-------------------
    //Variables borrar registros
    FILE *apdata, *aproom, *archivo1, *archivo2;
    int r;
    //Variable Contar id
    long int idNull = -1;
    char nombreNull[32] = "xxx";
    long int idEncontrado;
    char nombre[32]; //para borrarlo de hash
    int contador = 0;
    //----------------Recuento de Registros-----------------------
    apdata = fopen("dataDogs.dat", "rb+");
    if (apdata == NULL)
    {
        perror("Error in fopen apdata");
    }
    if (!(aproom = fopen("deletedId.dat", "rb+")))
    {
        aproom = fopen("deletedId.dat", "wb+");
    }

    //fseek ubica el puntero en el id a borrar
    fseek(apdata, (idBorrar) * (sizeof(struct dogType)), SEEK_SET);
    //----------------Confirmacion Existencia Registro-----------------------
    r = fread(&idEncontrado, 1, sizeof(long int), apdata);
    if (r == 0)
    {
        perror("Error en fread dataDogs.dat");
        exit(-1);
    }
    if (idEncontrado == -1)
    {
        //printf("Este Id no existe en la base de datos (el registro ya fue eliminado)\n");
        enviarMensaje(clienteID, &idEncontrado, sizeof(idEncontrado));
    }
    else
    {
        //fseek ubica el puntero en el id a borrar
        fseek(apdata, (idBorrar) * (sizeof(struct dogType)), SEEK_SET);
        //----------------Reemplazo del Registro por id Null--------------------
        r = fwrite(&idNull, sizeof(long int), 1, apdata);
        if (r == 0)
        {
            perror("Error en fwrite dataDogs.dat");
            exit(-1);
        }
        //----------------Saber nombre para borrar del hash
        r = fread(&nombre, 1, sizeof(char[32]), apdata);
        if (r == 0)
        {
            perror("Error en fread dataDogs.dat");
            exit(-1);
        }
        //---------------Poner el nombre en "xxx" (valor nulo)
        fseek(apdata, (((idBorrar) * (sizeof(struct dogType))) + (sizeof(long int))), SEEK_SET);

        r = fwrite(&nombreNull, sizeof(char[32]), 1, apdata);
        if (r == 0)
        {
            perror("Error en fwrite dataDogs.dat");
            exit(-1);
        }
        //Mensaje éxito
        //printf("\nSe eliminó el registro %li Correctamente!\n", idBorrar);
        enviarMensaje(clienteID, &idBorrar, sizeof(idBorrar));

        //guardar Log
        char idCHAR[50];
        sprintf(idCHAR, "%ld", idBorrar);
        guardarRegistroLogger(clienteInfo.ip, "BORRADO", idCHAR);
        //--

        //----------------Remover de Tabla Hash-----------------------
        pasarMinusculas(nombre);
        contador = 0;
        while (nombre[contador] != 0)
        {
            contador++;
        }
        long int hash = calcularHash(nombre, contador);

        if ((archivo1 = fopen("tablaHash.bin", "r+")) != NULL)
        { //intenta abrir el archivo1 - si existe hace lo siguiente
            //*************LECTURA EN ARCHIVO 1 ************
            rewind(archivo1);                               // "rebobinar", sitúa el cursor de lectura/escritura al principio del archivo.
            long int posEnTabla1 = hash * sizeof(long int); // halló la posición de memoria deseada
            fseek(archivo1, posEnTabla1, SEEK_SET);         // pararme en la posición "posEnTabla1" del archivo desde SEEK_SET (principio del fichero)
            long int posEnTabla2;
            fread(&posEnTabla2, sizeof(long int), 1, archivo1); //guardo posición de la tabla 2 donde se encuentran datos de este hash

            struct nodoType dato;
            struct nodoType datoAnterior;
            struct dogType mascota;

            //*************LECTURA EN ARCHIVO 2 ************
            if ((archivo2 = fopen("datosHash.bin", "r+")) != NULL)
            {
                rewind(archivo2);                        // "rebobinar", sitúa el cursor de lectura/escritura al principio del archivo.
                fseek(archivo2, posEnTabla2, SEEK_SET);  // pararme en la posición "posEnTabla2" del archivo desde SEEK_SET (principio del fichero)
                fread(&dato, sizeof(dato), 1, archivo2); // Lee el primer nodo que está guardado

                long int siguienteNodo = dato.siguiente;
                long int nodoAnterior = -1;
                long int nodoActual = posEnTabla2;

                if (dato.id == idBorrar)
                {
                    rewind(archivo1); // "rebobinar", sitúa el cursor de lectura/escritura al principio del archivo.
                    // posEnTabla1 ya se calculo
                    fseek(archivo1, posEnTabla1, SEEK_SET);                // pararme en la posición "posEnTabla1" del archivo desde SEEK_SET (principio del fichero)
                    fwrite(&siguienteNodo, sizeof(long int), 1, archivo1); //Actualizo al id del siguiente nodo (debido a que el eliminado fue el primer nodo)

                    // poner valores nulos al nodo borrado
                    dato.id = -1;
                    dato.siguiente = -1;
                    rewind(archivo2);                         // "rebobinar", sitúa el cursor de lectura/escritura al principio del archivo.
                    fseek(archivo2, nodoActual, SEEK_SET);    // pararme en la posición "nodoAnterior" del archivo desde SEEK_SET (principio del fichero)
                    fwrite(&dato, sizeof(dato), 1, archivo2); // Lee el primer nodo que está guardado
                }
                else
                {
                    while (siguienteNodo != -1)
                    {
                        rewind(archivo2); // "rebobinar"
                        //nodoAnterior = siguienteNodo;//direccion
                        datoAnterior = dato;
                        fseek(archivo2, siguienteNodo, SEEK_SET); // pararme en la posición "siguienteNodo" del archivo desde SEEK_SET (principio del fichero)
                        fread(&dato, sizeof(dato), 1, archivo2);  // Lee el primer nodo que está guardado

                        nodoAnterior = nodoActual;
                        nodoActual = siguienteNodo;
                        siguienteNodo = dato.siguiente;

                        if (dato.id == idBorrar)
                        {
                            datoAnterior.siguiente = siguienteNodo;
                            rewind(archivo2);                                         // "rebobinar", sitúa el cursor de lectura/escritura al principio del archivo.
                            fseek(archivo2, nodoAnterior, SEEK_SET);                  // pararme en la posición "posEnTabla1" del archivo desde SEEK_SET (principio del fichero)
                            fwrite(&datoAnterior, sizeof(datoAnterior), 1, archivo2); //Actualizo al id del siguiente nodo (debido a que el eliminado fue el primer nodo)

                            // poner valores nulos al nodo borrado
                            dato.id = -1;
                            dato.siguiente = -1;
                            rewind(archivo2);                         // "rebobinar", sitúa el cursor de lectura/escritura al principio del archivo.
                            fseek(archivo2, nodoActual, SEEK_SET);    // pararme en la posición "posEnTabla2" del archivo desde SEEK_SET (principio del fichero)
                            fwrite(&dato, sizeof(dato), 1, archivo2); // Lee el primer nodo que está guardado

                            siguienteNodo = -1; //terminar ciclo
                        }
                    }
                }
                fclose(archivo2);
            }
            else
            {
                printf("Error al abrir el archivo 2\n");
            }
            fclose(archivo2);
        }
        else
        { //************Error al abrir el archivo -- por que no existe
            printf("Error al abrir tablaHash.bin\n");
        }
        fclose(archivo1);

        //----------------Guarda el id para reutilizar--------------------
        fseek(aproom, 0, SEEK_END);
        r = fwrite(&idBorrar, sizeof(long int), 1, aproom);
        if (r == 0)
        {
            perror("Error en fwrite deletedId.dat");
            exit(-1);
        }
        //Actualizacion Variables Globales
        numRegEliminados = numRegEliminados + 1;
        numRegTotales = numMaxRegHistorial - numRegEliminados;
    }
    //Cerrar Archivo
    r = fclose(apdata);
    if (r != 0)
    {
        perror("Error en fclose dataDogs.dat");
        exit(-1);
    }
    r = fclose(aproom);
    if (r != 0)
    {
        perror("Error en fclose deletedId.dat");
        exit(-1);
    }
    //----------------Eliminacion Archivo Historia Clinica-----------------------
    FILE *historia;
    char nombreHC[32];
    sprintf(nombreHC, "%ld", idBorrar);
    strcat(nombreHC, ".txt");
    if ((historia = fopen(nombreHC, "r")) != NULL)
    {
        char consola[32] = "rm ";
        strcat(consola, nombreHC);
        system(consola);
    }
    else
    {
        if (errno != ENOENT)
        {
            perror("Error al borrar Historia Clinica");
        }
    }
}

void buscarRegistro(struct infoCliente clienteInfo)
{
    int clienteID = clienteInfo.clientfd;

    char nombre[32];
    recibirMensaje(clienteID, nombre, sizeof(nombre));
    //printf("Nombre: %s\n", nombre);

    //guardar Log
    guardarRegistroLogger(clienteInfo.ip, "BUSQUEDA", nombre);
    //--

    FILE *archivo1;
    FILE *archivo2;
    FILE *archivoDatos;
    long int posicionMascota = -1;
    int mascotasEncontradas = 0;
    int contador = 0;
    while (nombre[++contador] != 0)
        ;

    long int hash = calcularHash(nombre, contador);

    if ((archivo1 = fopen("tablaHash.bin", "r+")) != NULL)
    { //intenta abrir el archivo1 - si existe hace lo siguiente
        //*************LECTURA EN ARCHIVO 1 ************
        rewind(archivo1);                               // "rebobinar", sitúa el cursor de lectura/escritura al principio del archivo.
        long int posEnTabla1 = hash * sizeof(long int); // halló la posición de memoria deseada
        fseek(archivo1, posEnTabla1, SEEK_SET);         // pararme en la posición "posEnTabla1" del archivo desde SEEK_SET (principio del fichero)
        long int posEnTabla2;
        fread(&posEnTabla2, sizeof(long int), 1, archivo1); //guardo posición de la tabla 2 donde se encuentran datos de este hash

        struct nodoType dato;
        struct dogType mascota;

        if (posEnTabla2 == -1)
        {
            printf("\nNo existen registros con el nombre: %s\n", nombre);
        }
        else
        {
            //*************LECTURA EN ARCHIVO 2 ************
            if ((archivo2 = fopen("datosHash.bin", "r+")) != NULL)
            {
                rewind(archivo2);                        // "rebobinar", sitúa el cursor de lectura/escritura al principio del archivo.
                fseek(archivo2, posEnTabla2, SEEK_SET);  // pararme en la posición "posEnTabla2" del archivo desde SEEK_SET (principio del fichero)
                fread(&dato, sizeof(dato), 1, archivo2); // Lee el primer nodo que está guardado

                long int siguienteNodo = dato.siguiente;
                int igualdad = 0;

                if ((archivoDatos = fopen("dataDogs.dat", "r+")) != NULL)
                {
                    posicionMascota = (dato.id) * sizeof(struct dogType);
                    fseek(archivoDatos, posicionMascota, SEEK_SET); // pararme en la posición "posEnTabla1" del archivo desde SEEK_SET (principio del fichero)
                    fread(&mascota, sizeof(struct dogType), 1, archivoDatos);

                    pasarMinusculas(mascota.nombre);
                    for (int i = 0; i < contador; i++)
                    {
                        if (mascota.nombre[i] != nombre[i])
                            igualdad = -1;
                    }

                    if (igualdad == 0)
                    {
                        mascota.nombre[contador] = 0;
                        //printf("Id: %ld    Nombre: %s\n", mascota.id, mascota.nombre);
                        enviarMensaje(clienteID, &mascota.id, sizeof(mascota.id));
                        mascotasEncontradas++;
                    }
                }
                else
                {
                    printf("Error al abrir el dataDogs.dat \n");
                }
                fclose(archivoDatos);

                while (siguienteNodo != -1)
                {
                    rewind(archivo2);                         // "rebobinar"
                    fseek(archivo2, siguienteNodo, SEEK_SET); // pararme en la posición "siguienteNodo" del archivo desde SEEK_SET (principio del fichero)
                    fread(&dato, sizeof(dato), 1, archivo2);  // Lee el primer nodo que está guardado
                    siguienteNodo = dato.siguiente;

                    if ((archivoDatos = fopen("dataDogs.dat", "r+")) != NULL)
                    {
                        posicionMascota = (dato.id) * sizeof(struct dogType);
                        fseek(archivoDatos, posicionMascota, SEEK_SET); // pararme en la posición "posEnTabla1" del archivo desde SEEK_SET (principio del fichero)
                        fread(&mascota, sizeof(struct dogType), 1, archivoDatos);

                        igualdad = 0;
                        pasarMinusculas(mascota.nombre);

                        for (int i = 0; i < contador; i++)
                        {
                            if (mascota.nombre[i] != nombre[i])
                                igualdad = -1;
                        }

                        if (igualdad == 0)
                        {
                            mascota.nombre[contador] = 0;
                            //printf("Id: %ld    Nombre: %s\n", mascota.id, mascota.nombre);
                            enviarMensaje(clienteID, &mascota.id, sizeof(mascota.id));
                            mascotasEncontradas++;
                        }
                    }
                    else
                    {
                        printf("Error al abrir el dataDogs.dat \n");
                    }
                    fclose(archivoDatos);
                }
                fclose(archivo2);
                if (mascotasEncontradas == 0)
                {
                    printf("\nNo existen registros con el nombre: %s\n", nombre);
                }
            }
            else
            {
                printf("Error al abrir el archivo 2\n");
            }
        }
    }
    else
    { //************Error al abrir el archivo -- por que no existe
        printf("No existen registros en la tabla Hash\n");
    }
    long int finOperacion = -2;
    enviarMensaje(clienteID, &finOperacion, sizeof(finOperacion));
    fclose(archivo1);
}

void *seleccionarOpcion(void *ap)
{

    //int *clienteID = ap;
    struct infoCliente *clienteInfo = ap;

    if (clienteInfo->clientfd != -1)
    {
        //printf("\n****Cliente: %i\n", clienteInfo->clientfd);

        int opcion;
        //int clienteMostrar = *clienteID;
        struct infoCliente clienteMostrar = *clienteInfo;

        do
        {
            recibirMensaje(clienteInfo->clientfd, &opcion, sizeof(int));

            //printf("\nCliente: %i   Opción: %i\n", clienteMostrar.clientfd, opcion);

            switch (opcion)
            {
            case 1:
                ingresarRegistro(*clienteInfo);
                break;
            case 2:
                verRegistro(*clienteInfo);
                break;
            case 3:
                borrarRegistro(*clienteInfo);
                break;
            case 4:
                buscarRegistro(*clienteInfo);
                break;
            case 5:
                //printf("Cerrando Conexión. (Cliente: %i)\n", clienteMostrar.clientfd);
                clientesConectados--;
                //printf("\nClientes conectados: %i\n", clientesConectados);
                close(clienteInfo->clientfd);
                break;
            case 6:
                imprimirTodosLog();
                break;
            default:
                printf("No es una opción valida. Volverá al Menú principal\n");
                break;
            }
        } while (opcion != 5);
    }
}

int main()
{
    if (signal(SIGINT, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGINT\n");

    iniciarServidor();
    sleep(1);
    obtenerNumReg();

    //int opcion = 0;

    pthread_t hilo[BACKLOG];
    //int clientes[BACKLOG];
    int r;
    int conta = 0;
    //int clienteID;
    struct infoCliente clienteInfo;
    struct infoCliente clientes[BACKLOG];

    while (true)
    {
        clienteInfo = aceptarConexion();
        if (clienteInfo.clientfd > 0)
        {
            clientes[conta] = clienteInfo;
            clientfd = clienteInfo.clientfd;
            r = pthread_create(&hilo[conta], NULL, (void *)seleccionarOpcion, (void *)&clientes[conta]);
            if (r != 0)
            {
                perror("\n-->pthread_create error: ");
                exit(-1);
            }
            //seleccionarOpcion();
            conta++;
            clienteInfo.clientfd = -1;
        }
    }
    close(serverfd);
}