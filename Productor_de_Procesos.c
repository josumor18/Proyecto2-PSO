// SEMAFORO 0 SERA PARA ACCEDER A LA MEMORIA SECUNDARIA
// SEMAFORO 1 SERA OARA USAR LA FUNCION PARA BUSCAR EN MEMORIA PRINCIPAL

#include <sys/shm.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <time.h>
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
//indice para manejarse por la memoria secundaria
int index_mem_secundaria = 1;	//empieza en 1 porque el indice 0 es para el creador_procesos		
//nodos de la cola de procesos
pNodo cola_procesos_primero = NULL;
pNodo cola_procesos_ultimo  = NULL;
pthread_mutex_t mutex;
//pthread_mutex_lock(&mutex);
//pthread_mutex_unlock(&mutex);

FILE *log_file;
char nombre_archivo[] = "log_file.txt";


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



char * liberar_espacio_memoria(int pid){
	int i = 0;
	char * espacios = "";
	while (Memoria_principal[i].estado == 'D' || Memoria_principal[i].estado == 'O'){
		if(Memoria_principal[i].id_proceso == pid){
			Memoria_principal[i].estado = 'D';
			Memoria_principal[i].id_proceso = 0;
			printf("PROCESO %d LIBERO ESPACIO %d \n", pid, i);
			
			char * aux = malloc(16);
			snprintf(aux, 16, "%d,", i);
			char * esp_copia = malloc(1 + strlen(espacios));
			strcpy(esp_copia, espacios);
			espacios = malloc(1 + strlen(aux) + strlen(esp_copia));
			strcpy(espacios, esp_copia);
			strcat(espacios, aux);
		}
		i++;
	}
	return espacios;
}

void prueba(int pid){
	int i = 0;
	while (Memoria_principal[i].estado == 'D' || Memoria_principal[i].estado == 'O'){
		if(Memoria_principal[i].id_proceso == pid){
			printf("PROCESO %d AGARRO ESPACIO %d \n", pid, i);
		}
		i++;
	}
}



//regresa 1 si despues de Memoria_principal[index], hay suficientes espacios vacios continuos para completar los espacios por segmento
//regresa -1 si se acabó la memoria
//regresa 0 si no encontró espacio pero aun queda memoria
int validar_espacios_suficientes_segmento(int index, int espacios_mem_x_segmento){
	while (espacios_mem_x_segmento > 0){
		
		if (Memoria_principal[index].estado == 'D'){
			espacios_mem_x_segmento--;  index++;
		}
		
		else if (Memoria_principal[index].estado == 'O'){
			return 0;
		}
		
		else if (Memoria_principal[index+1].estado != 'D' && Memoria_principal[index+1].estado != 'O'){
			return -1;
		}
		
	}
	return 1;
}

//busca la cantidad de espacios de memoria x segmento indicada por parametro, las asgina al pid de parametro y cambia el estado a ocupado, exito 1, fracaso 0
int buscar_segmentos_memoria(int pid, int index, int cant_segmentos, int espacios_mem_x_segmento){
	int i = index;
	for (i = index; Memoria_principal[i].estado == 'O'; i++){
		if (Memoria_principal[i+1].estado != 'D' && Memoria_principal[i+1].estado != 'O'){
			return 0;
		}
	}
	//en este punto, estoy hubicado en un indice de la memoria principal con estado desocupado
	int temp = validar_espacios_suficientes_segmento(i, espacios_mem_x_segmento);
	if (temp == 1){
		for(int insert = espacios_mem_x_segmento; insert > 0; insert--){
			Memoria_principal[i].id_proceso = pid;
			Memoria_principal[i].estado = 'O';
			i++;
		}
	}
	else if (temp == -1){
		return 0;
	}
	
	else{
		return ( buscar_segmentos_memoria(pid, i+1, cant_segmentos, espacios_mem_x_segmento) );
	}
	
	cant_segmentos--;
	if (cant_segmentos > 0){
		if (buscar_segmentos_memoria(pid, i, cant_segmentos, espacios_mem_x_segmento) == 0){
			liberar_espacio_memoria(pid);
			return 0;
		}
	}
	
	return 1;
}



//busca la cantidad de paginas indicada por parametro, las asigna al pid de parametro y cambia el estado a ocupado, exito 1, fracaso 0
int buscar_paginas_memoria(int pid, int cant_paginas){
	int i;
	for (i = 0; Memoria_principal[i].estado == 'O'; i++){
		if (Memoria_principal[i+1].estado != 'D' && Memoria_principal[i+1].estado != 'O'){
			return 0;
		}
	}  
	//en este punto, estoy ubicado en un indice de la memoria principal con estado desocupado
	Memoria_principal[i].estado = 'O';
	Memoria_principal[i].id_proceso = pid;
	cant_paginas--;
	if(cant_paginas > 0){
		if (buscar_paginas_memoria(pid, cant_paginas) == 0){
			Memoria_principal[i].estado = 'D';
			Memoria_principal[i].id_proceso = 0;
			return 0;
		}
	}
	return 1;
}

char* getEspaciosPorProceso(int pid){
	int i = 0;
	char * espacios = "";
	while(Memoria_principal[i].estado == 'D' || Memoria_principal[i].estado == 'O'){
		if(Memoria_principal[i].id_proceso == pid){
			char * aux = malloc(16);
			snprintf(aux, 16, "%d,", i);
			char * esp_copia = malloc(1 + strlen(espacios));
			strcpy(esp_copia, espacios);
			espacios = malloc(1 + strlen(aux) + strlen(esp_copia));
			strcpy(espacios, esp_copia);
			strcat(espacios, aux);
		}
		i++;
	}
	//printf("\nEspacios: %s\n", espacios);
	return espacios;
}

char * getFechaHora(){
	time_t tiempo = time(0);
    struct tm *tlocal = localtime(&tiempo);
    char output[128];
    strftime(output,128,"%d/%m/%y %H:%M:%S",tlocal);
    char * output_pointer = (char *)malloc(129);
    strcpy(output_pointer, output);
    return output_pointer;
}
/*
 ========================================================================================================================
 ========================================================================================================================
 ========================================================================================================================
 */

void *proceso_buscador_paginas(void *args){
	
	struct hilo pb;
	pb.cancelado = 0;
	pb.id = pthread_self();
	pb.estado = 'B';
	int index_personal;
	esperarSemaforo(0);	
	if(!Memoria_secundaria[0].cancelado){
		index_personal = index_mem_secundaria;
		Memoria_secundaria[index_personal] = pb;
		index_mem_secundaria++;
		aumentarSemaforo(0);
	}
	else{
		aumentarSemaforo(0);
		pthread_exit((void *)0);
	}
	
	pid_t temp = pthread_self();    
	
		
	int dormir, cant_paginas;
	//random entre 1 y 10
	cant_paginas = rand() % (10+1-1) + 1;
	//random entre 20 y 60
	dormir = rand() % (60+1-20) + 20;
	
	printf("PROCESO %d HE SIDO CREADO Y OCUPA %d PAGINAS\n", temp, cant_paginas);
	//buscar espacio en memoria
	if(!Memoria_secundaria[index_personal].cancelado){
		esperarSemaforo(1); 
		Memoria_secundaria[index_personal].estado = 'M';
		//fprintf(log_file, "%d\tburcar espacio\t-----\tfecha,hora\t-----\n", temp);
		if (buscar_paginas_memoria((int)pthread_self(), cant_paginas) != 1){	
			printf("PROCESO %d NO HABIA CAMPO \n", temp);
			Memoria_secundaria[index_personal].estado = 'F';
			//******************ESCRIBIR EN BITACORA QUE NO HABIA CAMPO******************
			//fprintf(log_file, "%d\tMorir\tNA\tfecha,hora\tNA\n", temp);
			aumentarSemaforo(1);
			pthread_exit((void *)0);
		};
		//******************ESCRIBIR EN BITACORA QUE ENTRÓ Y AGARRO LOS ESPACIOS QUE TIENEN EL PID DE EL******************
		fprintf(log_file, "%d\tAsignación\t%s\t%s\n", temp, getFechaHora(), getEspaciosPorProceso(temp));
		prueba((int)pthread_self());
		aumentarSemaforo(1);
	}
	
	Memoria_secundaria[index_personal].estado = 'S';
	//ciclo para dormir el tiempo indicado por el random
	printf("PROCESO %d DORMIRA %d \n", temp, dormir);
	while(dormir > 0 && !Memoria_secundaria[index_personal].cancelado){
		sleep(1);
		dormir--;
	}
	
	//liberar espacio de memoria
	Memoria_secundaria[index_personal].estado = 'B';
	if(!Memoria_secundaria[index_personal].cancelado){
		esperarSemaforo(1); 
		char * espacios = liberar_espacio_memoria((int)pthread_self());
		//******************ESCRIBIR EN BITACORA QUE LIBERÓ LOS ESPACIOS QUE TIENÍA ASIGNADOS******************
		fprintf(log_file, "%d\tDesasignación\t%s\t%s\n", temp, getFechaHora(), espacios);
		aumentarSemaforo(1);
	}
	
	printf("PROCESO %d TERMINO SU EJECUCION \n", temp);
	Memoria_secundaria[index_personal].estado = 'T';
	pthread_exit((void *)0);
}


/*
 ========================================================================================================================
 ========================================================================================================================
 ========================================================================================================================
 */
void *proceso_buscador_segmentos(void *args){
	struct hilo pb;
	pb.cancelado = 0;
	pb.id = pthread_self();
	pb.estado = 'B';
	int index_personal;
	esperarSemaforo(0);	
	if(!Memoria_secundaria[0].cancelado){
		index_personal = index_mem_secundaria;
		Memoria_secundaria[index_personal] = pb;
		index_mem_secundaria++;
		aumentarSemaforo(0);
	}
	else{
		aumentarSemaforo(0);
		pthread_exit((void *)0);
	}
	
	pid_t temp = pthread_self();    
	
		
	int dormir, cant_segmentos, espacios_mem_x_segmento;
	//random entre 1 y 5
	cant_segmentos = rand() % (5+1-1) + 1;
	//random entre 20 y 60
	dormir = rand() % (60+1-20) + 20;
	//random entre 1 y 3
	espacios_mem_x_segmento = rand() % (3+1-1) + 1;
	
	printf("PROCESO %d HE SIDO CREADO Y OCUPA %d SEGMENTOS y %d ESPACIOS\n", temp, cant_segmentos, espacios_mem_x_segmento);
	//buscar espacio en memoria
	if(!Memoria_secundaria[index_personal].cancelado){
		esperarSemaforo(1); 
		Memoria_secundaria[index_personal].estado = 'M';
		
		
		if (buscar_segmentos_memoria((int)pthread_self(), 0, cant_segmentos, espacios_mem_x_segmento) != 1){	
			//******************ESCRIBIR EN BITACORA QUE NO HABIA CAMPO******************
			printf("PROCESO %d NO HABIA CAMPO \n", temp);
			Memoria_secundaria[index_personal].estado = 'F';
			aumentarSemaforo(1);
			pthread_exit((void *)0);
		};
		

		//******************ESCRIBIR EN BITACORA QUE ENTRÓ Y AGARRO LOS ESPACIOS QUE TIENEN EL PID DE EL******************
		fprintf(log_file, "%d\tAsignación\t%s\t%s\n", temp, getFechaHora(), getEspaciosPorProceso(temp));
		prueba((int)pthread_self());
		aumentarSemaforo(1);
	}
	
	Memoria_secundaria[index_personal].estado = 'S';
	//ciclo para dormir el tiempo indicado por el random
	printf("PROCESO %d DORMIRA %d \n", temp, dormir);
	while(dormir > 0 && !Memoria_secundaria[index_personal].cancelado){
		sleep(1);
		dormir--;
	}
	
	//liberar espacio de memoria
	Memoria_secundaria[index_personal].estado = 'B';
	if(!Memoria_secundaria[index_personal].cancelado){
		esperarSemaforo(1); 
		char * espacios = liberar_espacio_memoria((int)pthread_self());
		//******************ESCRIBIR EN BITACORA QUE LIBERÓ LOS ESPACIOS QUE TIENÍA ASIGNADOS******************
		fprintf(log_file, "%d\tDesasignación\t%s\t%s\n", temp, getFechaHora(), espacios);
		
		aumentarSemaforo(1);
	}
	
	printf("PROCESO %d TERMINO SU EJECUCION \n", temp);
	Memoria_secundaria[index_personal].estado = 'T';
	pthread_exit((void *)0);
}


/*
 ========================================================================================================================
 ========================================================================================================================
 ========================================================================================================================
 */
 
 
void *creador_procesos(void *args){
	//guardo el registro de este hilo en la memoria secundaria, el espacio 0 es reservado solo para él
	struct hilo cp;
	cp.id = 0;
	cp.cancelado = 0;
	cp.estado = 'Z';
	Memoria_secundaria[0] = cp;
	
	char *tipo_algoritmo = (char*)args;
	
	
	int dormir;
	//ciclo que se ejecuta mientras Finalizador no le ponga cancelado en 1
	while(!Memoria_secundaria[0].cancelado){
		
		//crear procesos
		pthread_t proceso_buscador_thread;
		
		if(tipo_algoritmo[0] == 'S'){	//para segmentacion
			if(pthread_create(&proceso_buscador_thread, NULL, proceso_buscador_segmentos, (void *)0)){
				printf("Error creando el hilo de proceso_buscador por segmentacion.\n");
			}
		}
		else{						   //para paginacion
			if(pthread_create(&proceso_buscador_thread, NULL, proceso_buscador_paginas, (void *)0)){
				printf("Error creando el hilo de proceso_buscador por paginacion.\n");
			}
		}
		add(&cola_procesos_primero, &cola_procesos_ultimo, proceso_buscador_thread);
		//random entre 30 y 60
		dormir = rand() % (20+1-10) + 10;
		//dormir = rand() % (60+1-30) + 30;
		printf("**creador dormira %d \n", dormir);

		while(!Memoria_secundaria[0].cancelado && dormir > 0){
			sleep(1);
			dormir--;
		}
	
	}
	
	pthread_exit((void *)0);
}

/*
 ========================================================================================================================
 ========================================================================================================================
 ========================================================================================================================
 */
 
 
int main(int argc, char* argv[]){
	
	//valido la cantidad de argumentos... debe ser 1
	if(argc != 2){
			printf("No ha ingresado todos los parametros. Ingrese funcion [cantidad de paginas o espacios en memoria]\n");
			return 0;
	}
	
	//recupero el argumento
	char* modo_ejecucion = (argv[1]);
	
	if (*modo_ejecucion == 'P'){
		printf("Iniciando en modo PAGINACION\n");
	}
	else if(*modo_ejecucion == 'S'){
		printf("Iniciando en modo SEGMENTACION\n");
	}
	else{
		*modo_ejecucion = 'P';
		printf("Parametro incorrecto... Iniciando en modo PAGINACION por defecto\n");
	}
	
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
	
	//Se abre el archivo para escribir la bitacora
	log_file = fopen(nombre_archivo, "w");
	if(log_file == NULL){
		printf("\n- No se ha podido crear el archivo de bitacora\n\n");
	}else{
		printf("\nArchivo de bitacora creado\n\n");
		fprintf(log_file, "Bitacora\nInicio de la ejecución: %s\n\nPID\t\tTIPO\t\tFecha y hora\t\tEspacio asignado/desasignado\n", getFechaHora());
	}
	
	aumentarSemaforo(0);
	aumentarSemaforo(1);
	
	pthread_t creador_procesos_thread;
	if(pthread_create(&creador_procesos_thread, NULL, creador_procesos, (void *)modo_ejecucion)){
		printf("Error creando el hilo de creacion de procesos.\n");
	}
	
	pthread_join(creador_procesos_thread, NULL); 
	
	//mientras hayan elementos en la cola, les hago join y los saco
	while(isVacio(&cola_procesos_primero, &cola_procesos_ultimo) == 1){
		pthread_join( leer(&cola_procesos_primero, &cola_procesos_ultimo), NULL);
	}

	//desasociar memoria compartida de la zona de datos del programa
	if (Id_Memoria_principal != -1 && Id_Memoria_secundaria != -1){
		shmdt ((char *)Memoria_principal);
		shmdt ((char *)Memoria_secundaria);
	}
	
	//Cerrar el archivo de bitacora
	fclose(log_file);
	
	return 0;
}
