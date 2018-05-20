#ifndef B_H
#define B_H

struct Proceso{
	int burst;
	int prioridad;
};

typedef struct _nodo{
	pthread_t valor;
	struct _nodo *siguiente;
} tipoNodo;

typedef tipoNodo *pNodo;

pthread_t leer (pNodo *primero, pNodo *ultimo);
void add(pNodo *primero, pNodo *ultimo, pthread_t v);
int isVacio(pNodo *primero, pNodo *ultimo);

#endif
