// SEMAFORO 0 SERA PARA ACCEDER A LA MEMORIA SECUNDARIA
// SEMAFORO 1 SERA OARA USAR LA FUNCION PARA BUSCAR EN MEMORIA PRINCIPAL


#include <sys/shm.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <time.h>

#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
// La union ya está definida en sys/sem.h
#else
// Tenemos que definir la union
union semun 
{ 
	int val;
	struct semid_ds *buf;
	unsigned short int *array;
	struct seminfo *__buf;
};
#endif


//estructura para rellenar la memoria compartida secundaria
struct hilo{
    int cancelado;        // 1 si se canceló desde el Finalizador, 0 si no
	pid_t id;			  //pid
	char estado;          //M buscando en memoria, F muertos por falta de espacio, S en memoria (sleep), T terminados,  B bloqueados         
};

int Id_Semaforo;
struct espacio_memoria *Memoria_principal   = NULL;
struct hilo            *Memoria_secundaria  = NULL;
//indice para manejarse por la memoria secundaria
int index_mem_secundaria = 1;	//empieza en 1 porque el indice 0 es para el creador_procesos		



void aumentarSemaforo(int indice_semaforo){
	struct sembuf Operacion;
	//	Para "pasar" por el semáforo parándonos si está "rojo", debemos rellenar
	//	esta estructura.
	//	sem_num es el indice del semáforo en el array por el que queremos "pasar"
	//	sem_op es -1 para hacer que el proceso espere al semáforo.
	//	sem_flg son flags de operación. De momento nos vale un 0.
	Operacion.sem_num = indice_semaforo;
	Operacion.sem_op = 1;
	Operacion.sem_flg = 0;

	semop (Id_Semaforo, &Operacion, 1);
}

void esperarSemaforo(int indice_semaforo){
	struct sembuf Operacion;
	//	Para "pasar" por el semáforo parándonos si está "rojo", debemos rellenar
	//	esta estructura.
	//	sem_num es el indice del semáforo en el array por el que queremos "pasar"
	//	sem_op es -1 para hacer que el proceso espere al semáforo.
	//	sem_flg son flags de operación. De momento nos vale un 0.
	Operacion.sem_num = indice_semaforo;
	Operacion.sem_op = -1;
	Operacion.sem_flg = 0;

	semop (Id_Semaforo, &Operacion, 1);
}


int main(){
	
	//variables para las memorias compartidas
	key_t Clave_memoria_principal;
	key_t Clave_memoria_secundaria;
	int Id_Memoria_principal;
	int Id_Memoria_secundaria;
	int i,j;
	
	//variables para los semaforos
	key_t Clave_semaforo;
	struct sembuf Operacion;
	union semun arg;
	

	//obtener una clave para la memoria compartida
	Clave_memoria_principal  = ftok ("/bin/ls", 33);
	Clave_memoria_secundaria = ftok ("/bin/ln", 34);
	if (Clave_memoria_principal == -1 || Clave_memoria_secundaria == -1){
		printf("Error al intentar conseguir las llaves de memoria compartida\n");
		exit(0);
	}
	
	//obtener clave para semaforo
	Clave_semaforo = ftok ("/bin/cat", 33);
	if (Clave_semaforo == (key_t)-1){
		printf("Error al intentar conseguir la llave de semaforos\n");
		exit(0);
	}


	//obtenemos el id de la memoria. Al no poner
	//el flag IPC_CREAT, estamos suponiendo que dicha memoria ya está creada.
	Id_Memoria_principal  = shmget (Clave_memoria_principal,  0, 										   0777);
	Id_Memoria_secundaria = shmget (Clave_memoria_secundaria, sizeof(struct hilo)*200,                     0777);
	if (Id_Memoria_principal == -1 || Id_Memoria_secundaria == -1) {
		printf("Error al intentar crear las memorias compartidas\n");
		exit (0);
	}
	
	// Se obtiene un array de semaforos (6 en este caso)
	// El IPC_CREAT indica que lo  cree si no lo está ya
	// el 0600 con permisos de lectura y escritura para el usuario que lance
	// los procesos.        
	Id_Semaforo = semget (Clave_semaforo, 6, 0600 | IPC_CREAT);
	if (Id_Semaforo == -1){
		printf("Error al intentar crear el array de semaforos\n");
		exit (0);
	}

	
	//obtenemos un puntero a la memoria compartida
	Memoria_principal  = (struct espacio_memoria *)shmat (Id_Memoria_principal,  (char *)0, 0);
	Memoria_secundaria = (struct hilo *)		   shmat (Id_Memoria_secundaria, (char *)0, 0);
	if (Memoria_principal == NULL || Memoria_secundaria == NULL){
		printf("Error al intentar apuntar a las memorias compartidas\n");
		exit (0);
	}
	
	
	esperarSemaforo(0);	
	for(int i = 0; 1; i++){
		
		if(Memoria_secundaria[i].estado != 'Z' && Memoria_secundaria[i].estado != 'M' && Memoria_secundaria[i].estado != 'F' && 
		   Memoria_secundaria[i].estado != 'S' && Memoria_secundaria[i].estado != 'T' && Memoria_secundaria[i].estado != 'B')
		{
			break;
		}
		Memoria_secundaria[i].cancelado = 1;
	}
	aumentarSemaforo(0);


	//desasociar memoria compartida de la zona de datos del programa
	if (Id_Memoria_principal != -1 && Id_Memoria_secundaria != -1){
		shmdt ((char *)Memoria_principal);
		shmdt ((char *)Memoria_secundaria);
	}
	
	//destruir memoria compartida
	shmctl (Id_Memoria_principal,  IPC_RMID, (struct shmid_ds *)NULL);
	shmctl (Id_Memoria_secundaria, IPC_RMID, (struct shmid_ds *)NULL);
	//destruyo el semaforo segun los indices del arreglo de semaforos
	//(id del semaforo, indice, parametro extraño)
	semctl (Id_Semaforo, 		   0, 0);
	semctl (Id_Semaforo, 		   1, 0);
	
	return 0;
}
