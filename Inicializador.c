// SEMAFORO 0 SERA PARA ACCEDER A LA MEMORIA SECUNDARIA
// SEMAFORO 1 SERA OARA USAR LA FUNCION PARA BUSCAR EN MEMORIA PRINCIPAL

//COMANDOS LINUX ....  ipcs    lista recursos compartidos              ipcrm -a     elimina todos los recursos compartidos
#include <sys/shm.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

//cambio nuevo
//codigo necesario para semaforos
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


void esperarSemaforo(int Id_Semaforo, int indice_semaforo){
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

int main(int argc, char* argv[]){
	
	//valido la cantidad de argumentos... debe ser 1
	if(argc != 2){
			printf("No ha ingresado todos los parametros. Ingrese funcion [cantidad de paginas o espacios en memoria]\n");
			return 0;
	}
	
	//recupero el argumento
	int cant_memoria = atoi(argv[1]);

	//variables para el manejo de memoria compartida
	key_t Clave_memoria_principal;
	key_t Clave_memoria_secundaria;
	int Id_Memoria_principal;
	int Id_Memoria_secundaria;
	int i,j;
	
	//punteros a la memoria compartida
	struct espacio_memoria *Memoria_principal   = NULL;
	struct hilo            *Memoria_secundaria  = NULL;


	//variables para el semaforo
	key_t Clave_semaforo;
	int Id_Semaforo;
	union semun arg;
	
	
	
	//	Conseguimos una clave para la memoria compartida. Todos los
	//	procesos que quieran compartir la memoria, deben obtener la misma
	//	clave. Esta se puede conseguir por medio de la función ftok.
	//	A esta función se le pasa un fichero cualquiera que exista y esté
	//	accesible (todos los procesos deben pasar el mismo fichero) y un
	//	entero cualquiera (todos los procesos el mismo entero).
	
	Clave_memoria_principal  = ftok ("/bin/ls", 33);
	Clave_memoria_secundaria = ftok ("/bin/ln", 34);
	if (Clave_memoria_principal == -1 || Clave_memoria_secundaria == -1){
		printf("Error al intentar conseguir las llaves de memoria\n");
		exit(0);
	}
	
	//lo mismo para la clave del semaforo
	Clave_semaforo = ftok ("/bin/cat", 33);
	if (Clave_semaforo == (key_t)-1){
		printf("Error al intentar conseguir la llave de semaforos\n");
		exit(0);
	}

	//	Creamos la memoria con la clave recién conseguida. Para ello llamamos
	//	a la función shmget pasándole la clave, el tamaño de memoria que
	//	queremos reservar y unos flags.
	//	Los flags son  los permisos de lectura/escritura/ejecucion 
	//	para propietario, grupo y otros (es el 777 en octal) y el 
	//	flag IPC_CREAT para indicar que cree la memoria.
	//	La función nos devuelve un identificador para la memoria recién
	//	creada.

	//la cantidad de memoria principal va a ser el tamaño del struct spacio_memoria multiplicado por la cantidad de memoria indicado por parametro del main
	Id_Memoria_principal  = shmget (Clave_memoria_principal,  sizeof(struct espacio_memoria)*cant_memoria, 0777 | IPC_CREAT);
	//la cantidad de memoria secundaria va a ser el tañao del struct hilo multiplicado por una cantidad lo suficientemente grande como para que no se llene en toda la ejecucion
	Id_Memoria_secundaria = shmget (Clave_memoria_secundaria, sizeof(struct hilo)*200,                     0777 | IPC_CREAT);
	if (Id_Memoria_principal == -1 || Id_Memoria_secundaria == -1) {
		printf("Error al intentar crear la memoria\n");
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
	
	

	//	Una vez creada la memoria, hacemos que uno de nuestros punteros
	//	apunte a la zona de memoria recién creada. Para ello llamamos a
	//	shmat, pasándole el identificador obtenido anteriormente y un
	//	par de parámetros extraños, que con ceros vale.
	Memoria_principal  = (struct espacio_memoria *)shmat (Id_Memoria_principal,  (char *)0, 0);
	Memoria_secundaria = (struct hilo *)           shmat (Id_Memoria_secundaria, (char *)0, 0);
	if (Memoria_principal == NULL || Memoria_secundaria == NULL){
		printf("Error al intentar apuntar a la memoria\n");
		exit (0);
	}

	//	Se inicializa el semáforo con un valor conocido. Si lo ponemos a 0,
	//	es semáforo estará "rojo". Si lo ponemos a 1, estará "verde".
	//	El 0 de la función semctl es el índice del semáforo que queremos.
	arg.val = 1;
	semctl (Id_Semaforo, 0, SETVAL, &arg);
	arg.val = 1;
	semctl (Id_Semaforo, 1, SETVAL, &arg);

	//relleno la memoria compartida principal con espacios de memoria disponibles (marcados con D)
	for (i=0; i<cant_memoria ; i++){
		Memoria_principal[i] = (struct espacio_memoria){.id_proceso = 0, .estado = 'D'};
	}
	
	//relleno la memoria compartida secundaria con el valor del pid en -1
	for (i=0; i<200 ; i++){
		Memoria_secundaria[i] = (struct hilo){.cancelado = 0, .estado = 'N', .id = -1};
	}

	//desasociar memoria compartida de la zona de datos del programa
	shmdt ((char *)Memoria_principal);
	shmdt ((char *)Memoria_secundaria);
	
	//esperarSemaforo(Id_Semaforo, 0);
	
	return 0;
}
