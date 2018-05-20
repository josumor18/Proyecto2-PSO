#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "cola_procesos.h"
#include "string.h"



//agrega un nuevo nodo al final de la cola con el sThreads indicado de parametro
void add(pNodo *primero, pNodo *ultimo, pthread_t v){
	pNodo nuevo;
	nuevo = (pNodo)malloc(sizeof(tipoNodo));
	nuevo->valor = v;
	nuevo->siguiente = NULL;

	if (*ultimo)  (*ultimo)->siguiente = nuevo;
	*ultimo = nuevo;
	if (!*primero)   *primero = nuevo;
}


//retorna el PCB del primer nodo de la cola y lo elimina
pthread_t leer(pNodo *primero, pNodo *ultimo){
	pNodo nodo;
	pthread_t v;
	nodo = *primero;
	if (!nodo) {
		printf("ERROR AL LEER NODO NO EXISTENTE\n");
		return v;
	}
	*primero = nodo->siguiente;
	v = nodo->valor;
	free(nodo);
	if (!*primero)  *ultimo = NULL;
	return v;
}


int isVacio(pNodo *primero, pNodo *ultimo){
	pNodo nodo = *primero;
	if(nodo){
		return 1;			//tiene >=1 elementos
	}
	return 0;				//cola vacia
}



