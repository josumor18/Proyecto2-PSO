// SEMAFORO 0 SERA PARA ACCEDER A LA MEMORIA SECUNDARIA
// SEMAFORO 1 SERA PARA USAR LA FUNCION PARA BUSCAR EN MEMORIA PRINCIPAL

#include <sys/shm.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <time.h>
#include <string.h>
#include "cola_procesos.h"

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

//estructura para rellenar la memoria compartida principal
struct espacio_memoria{
    pid_t id_proceso;     //pid
	char estado;          //O ocupado, D disponible
};


int Id_Semaforo;
struct espacio_memoria *Memoria_principal   = NULL;
struct hilo            *Memoria_secundaria  = NULL;
int ind_sleep = 0;
int proc_sleep[199];
int ind_buscando = 0;
int proc_buscando[199];
int ind_bloqueados = 0;
int proc_bloqueados[199];
int ind_muertos = 0;
int proc_muertos[199];
int ind_terminados = 0;
int proc_terminados[199];

/*
 ========================================================================================================================
 ========================================================================================================================
 ========================================================================================================================
 */ 
 
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

/*
 ========================================================================================================================
 ========================================================================================================================
 ========================================================================================================================
 */ 
 
	//Memoria_principal[0].estado
	//Memoria_secundaria[0].estado
	//aumentarSemaforo(0) //Signal secundaria
	//aumentarSemaforo(1) //Signal principal
	//esperarSemaforo(0) //Wait secundaria
	//esperarSemaforo(1) //Wait principal
void estado_memoria(){
	int i = 0;
	esperarSemaforo(1);
	for (i=0; ; i++){
		//printf("%d\n", i);
		//printf("%c\n", Memoria_principal[i].estado);
		if('D' == Memoria_principal[i].estado || 'O' == Memoria_principal[i].estado){//(strcmp("D", Memoria_principal[i].estado) == 0) || (strcmp("O", Memoria_principal[i].estado) == 0)){
			//printf("Prueba 1");
			if('O' == Memoria_principal[i].estado){//strcmp("O", Memoria_principal[i].estado) == 0){
				//printf("Prueba 2");
				printf("ESPACIO %d)\tPID: %d\n", i, Memoria_principal[i].id_proceso);
			}else{
				printf("ESPACIO %d)\tPID: Desocupado\n", i);
			}
			
		}else{
			break;
		}
		
	}
	aumentarSemaforo(1);
}

////////////////////////////////////////
// almacenar procesos según su estado //
////////////////////////////////////////
void clasificar_procesos(){
	esperarSemaforo(0);
	int i = 1;
	ind_sleep = 0;
	ind_buscando = 0;
	ind_bloqueados = 0;
	ind_muertos = 0;
	ind_terminados = 0;
	for(i = 1; i < 200; i++){
		char state = Memoria_secundaria[i].estado;
		if(state == 'S'){
			proc_sleep[ind_sleep] = Memoria_secundaria[i].id;
			ind_sleep++;
		}else if(state == 'M'){
			proc_buscando[ind_buscando] = Memoria_secundaria[i].id;
			ind_buscando++;
		}else if(state == 'B'){
			proc_bloqueados[ind_bloqueados] = Memoria_secundaria[i].id;
			ind_bloqueados++;
		}else if(state == 'F'){
			proc_muertos[ind_muertos] = Memoria_secundaria[i].id;
			ind_muertos++;
		}else if(state == 'T'){
			proc_terminados[ind_terminados] = Memoria_secundaria[i].id;
			ind_terminados++;
		}
	}
	
	aumentarSemaforo(0);
}

void estado_procesos(){
	clasificar_procesos();
	printf("\nProcesos en Sleep:\n");
	int i = 0;
	for(i = 0; i < ind_sleep; i++){
		printf("> PID: %d\n", proc_sleep[i]);
	}
	
	printf("\nProcesos Buscando espacio en memoria:\n");
	for(i = 0; i < ind_buscando; i++){
		printf("> PID: %d\n", proc_buscando[i]);
	}
	
	printf("\nProcesos Bloqueados:\n");
	for(i = 0; i < ind_bloqueados; i++){
		printf("> PID: %d\n", proc_bloqueados[i]);
	}
	
	printf("\nProcesos Muertos:\n");
	for(i = 0; i < ind_muertos; i++){
		printf("> PID: %d\n", proc_muertos[i]);
	}
	
	printf("\nProcesos Terminados:\n");
	for(i = 0; i < ind_terminados; i++){
		printf("> PID: %d\n", proc_terminados[i]);
	}
}
 
/*
 ========================================================================================================================
 ========================================================================================================================
 ========================================================================================================================
 */ 
 
 int main(int argc, char* argv[]){
	
	//variables para las memorias compartidas
	key_t Clave_memoria_principal;
	key_t Clave_memoria_secundaria;
	int Id_Memoria_principal;
	int Id_Memoria_secundaria;
	
	//variables para los semaforos
	key_t Clave_semaforo;
	struct sembuf Operacion;
	union semun arg;
	

	//establezco semilla para el random segun la fecha del sistema
	srand(time(NULL));
		
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
	

	char buffer_menu[2];
	printf("\nIngrese 1 si desea obtener el estado de la memoria");
	printf("\nIngrese 2 si desea obtener el estado de los procesos\n");
    printf("\nIngrese cualquier otro caracter para cerrar el cliente\n");
	scanf("%s", buffer_menu);
	while((strcmp("1", buffer_menu) == 0) || (strcmp("2", buffer_menu) == 0)){
		if(Memoria_secundaria[0].cancelado == 1){
			printf("\nEl finalizador ya ha sido ejecutado\n");
			
			if (Id_Memoria_principal != -1 && Id_Memoria_secundaria != -1){
				shmdt ((char *)Memoria_principal);
				shmdt ((char *)Memoria_secundaria);
			}
			exit(0);
		}else{
			if(strcmp("1", buffer_menu) == 0){	
				printf("\nEstado de la Memoria\n");
				estado_memoria();
			}
			if(strcmp("2", buffer_menu) == 0){
				printf("\nEstado de los Procesos\n");
				estado_procesos();
			}
			
			printf("\nIngrese 1 si desea obtener el estado de la memoria");
			printf("\nIngrese 2 si desea obtener el estado de los procesos\n");
			printf("\nIngrese cualquier otro caracter para cerrar el cliente\n");
			scanf("%s", buffer_menu);
		}
	}
	
	if (Id_Memoria_principal != -1 && Id_Memoria_secundaria != -1){
				shmdt ((char *)Memoria_principal);
				shmdt ((char *)Memoria_secundaria);
	}
			
	return 0;
}
