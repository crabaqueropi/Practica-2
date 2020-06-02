all:
	gcc Servidor/p2-dogServer.c -o Servidor/servidor -pthread
	gcc Cliente/p2-dogClient.c -o Cliente/cliente

clean:
	rm Servidor/servidor
	rm Cliente/cliente
