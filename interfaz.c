#define _XOPEN_SOURCE
#define _POSIX_C_SOURCE 200809L
#define TIME_FORMAT "%FT%T%z"

#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hash.h"
#include "strutil.h"
#include "lista.h"
#include "abb.h"
#include "heap.h"
#include "modificaciones_tda.h"


#define KILOBYTE 1000

const char* ORDENAR_ARCHIVO = "ordenar_archivo";
const char* AGREGAR_ARCHIVO = "agregar_archivo";
const char* VER_VISITANTES = "ver_visitantes";

/*Esta funcion se encarga de cargar una particion con sus K lineas correspondientes.*/
void cargar_particion(FILE* log_original,FILE* particion,size_t K_LINEAS, size_t* cantidad_registros_cargados,size_t cantidad_lineas_archivo){
  linea_registro_t** lineas_particion;
  lineas_particion = malloc(sizeof(linea_registro_t*)*K_LINEAS);
  char* linea=NULL;
  size_t tam_linea = 0;
  size_t particiones_cargadas = 0 ;
  ssize_t leidos;
  for(size_t l=0; l<K_LINEAS;l++){
    leidos = getline(&linea,&tam_linea,log_original);
    if(leidos!=-1){
        lineas_particion[l] = crear_linea_registro();
        cargar_linea_registro(lineas_particion[l],linea);
        (*cantidad_registros_cargados)++;
        particiones_cargadas++;
    }
  }
  free(linea);

  heap_sort((void**)lineas_particion,particiones_cargadas,(cmp_func_t)funcion_cmp_logs);

  for(size_t i = 0 ; i < particiones_cargadas; i++){
    fprintf(particion,"%s	%s	%s	%s",lineas_particion[i]->ip,lineas_particion[i]->fecha,lineas_particion[i]->recurso,lineas_particion[i]->ruta);
  }
  linea_registro_destruir(lineas_particion,particiones_cargadas);
}

void generar_log_ordenado(FILE** particiones_temporales,size_t K_PARTICIONES,const char* output){
  FILE* log_ordenado = fopen(output,"w");
  if(!log_ordenado) return;

  char filename[50];
  for(size_t i = 0 ; i < K_PARTICIONES; i++){
  sprintf(filename,"particion%zu.txt",i);
  particiones_temporales[i] = fopen(filename,"r");
  }

  char* linea=NULL;
  size_t tam_linea = 0;
  ssize_t leidos;

  registro_t** registros = malloc(sizeof(registro_t*)*K_PARTICIONES);
  heap_t* heap = heap_crear((cmp_func_t)funcion_cmp_registros);

  for(size_t i = 0 ; i < K_PARTICIONES; i++){
    leidos = getline(&linea,&tam_linea,particiones_temporales[i]);
    registros[i] = crear_registro(linea,particiones_temporales[i]);
    heap_encolar(heap,(void*)registros[i]);
  }
  free(linea);

  registro_t* registro_heap;
  linea = NULL;
  tam_linea = 0 ;
  leidos = 0;

  while(!heap_esta_vacio(heap)){
     void* elemento = heap_desencolar(heap);
     FILE* ultimo_registro = ((registro_t*)elemento)->fp;
     char* linea_registro = ((registro_t*)elemento)->linea;

     leidos = getline(&linea,&tam_linea,ultimo_registro);

     if(leidos != -1){
      registro_heap = crear_registro(linea,ultimo_registro);
       heap_encolar(heap,(void*)registro_heap);
     }


     char* linea_escribir = strdup(linea_registro);
     char** campos = split(linea_escribir,'\t');
     fprintf(log_ordenado,"%s	%s	%s	%s",campos[0],campos[1],campos[2],campos[3]);

     free((char*)((registro_t*)elemento)->linea);
     free((registro_t*)elemento);
     free(linea_escribir);

     free_strv(campos);
  }
  free(linea);


  for(size_t i = 0 ;i<K_PARTICIONES;i++){
    fclose(particiones_temporales[i]);
  }

  free(registros);
  heap_destruir(heap,NULL);
  fclose(log_ordenado);
}

bool ordenar_archivo(size_t memoria_byte,const char* archivo,const char* output){
  FILE* log_original = fopen(archivo,"r");
  if(!log_original) return false;
  //Cuento las lineas y la linea mas grande.
  size_t cantidad_lineas_archivo = 0;
  size_t tam_max_linea = 0;

  size_t memoria = memoria_byte * KILOBYTE;

  char* linea=NULL;
  size_t tam_linea = 0;

  //Primera pasada para contar cantidad de lineas y buscar el tma max de linea.
  while(getline(&linea,&tam_linea,log_original)!=-1){
      if(tam_linea>tam_max_linea) tam_max_linea=tam_linea;
      cantidad_lineas_archivo++;
  }

  free(linea);
  size_t K_PARTICIONES = memoria / tam_max_linea;

  size_t K_LINEAS = (cantidad_lineas_archivo / K_PARTICIONES ) + 1;

  fseek( log_original, 0, SEEK_SET );

  //Hago un vector de particiones  y  c/u lo cargo con K lineas.
  FILE* particiones_temporales[K_PARTICIONES];

  size_t cantidad_registros_cargados = 0;
  size_t* p_cantidad_registros_cargados = &cantidad_registros_cargados;

  for(size_t i = 0 ;i<K_PARTICIONES;i++){
    char filename[50];
    sprintf(filename,"particion%zu.txt",i);
    particiones_temporales[i] = fopen(filename,"w");

    cargar_particion(log_original,particiones_temporales[i],K_LINEAS,p_cantidad_registros_cargados,cantidad_lineas_archivo);
    fclose(particiones_temporales[i]);
  }

  generar_log_ordenado(particiones_temporales,K_PARTICIONES,output);
  fclose(log_original);

  for(size_t i = 0; i<K_PARTICIONES;i++){
    char filename[50];
    sprintf(filename,"particion%zu.txt",i);
    remove(filename);
  }
  return true;
}


abb_t* encontrar_DoS(hash_t* hash){

	hash_iter_t* iterador_hash = hash_iter_crear(hash);
	abb_t* abb_DoS = abb_crear(funcion_cmp_ip, NULL);

	while(!hash_iter_al_final(iterador_hash)){
		lista_t* horarios = hash_obtener(hash, hash_iter_ver_actual(iterador_hash));

		lista_iter_t* iterador1 = lista_iter_crear(horarios);
		lista_iter_t* iterador2 = lista_iter_crear(horarios);

		for(int i = 0; i < 4; i++){
			lista_iter_avanzar(iterador2);
		}

		while(!lista_iter_al_final(iterador2)){
			time_t hora1 = iso8601_to_time(lista_iter_ver_actual(iterador1));
			time_t hora2 = iso8601_to_time(lista_iter_ver_actual(iterador2));
			if (difftime(hora2, hora1) < 2){
				abb_guardar(abb_DoS, hash_iter_ver_actual(iterador_hash), NULL);
				break;
			}
			lista_iter_avanzar(iterador1);
			lista_iter_avanzar(iterador2);
		}
		lista_destruir(horarios, free);
		lista_iter_destruir(iterador1);
		lista_iter_destruir(iterador2);
		hash_iter_avanzar(iterador_hash);
	}
	hash_iter_destruir(iterador_hash);
	return abb_DoS;
}

void imprimir_DoS(abb_t* arbol){
	abb_iter_t* iterador_abb = abb_iter_in_crear(arbol);
	while(!abb_iter_in_al_final(iterador_abb)){
		printf("DoS: %s\n", abb_iter_in_ver_actual(iterador_abb));
		abb_iter_in_avanzar(iterador_abb);
	}
	abb_iter_in_destruir(iterador_abb);
}

bool agregar_archivo(const char* archivo, abb_t* abb_ips){
	FILE* log = fopen(archivo,"r");
	if(!log) return false;
	hash_t* hash = hash_crear(NULL);

	char* linea = NULL;
	size_t tam_linea = 0;
	ssize_t leidos;
	leidos = getline(&linea, &tam_linea, log);

	char** registro;

	while(leidos != -1){
		registro = split(linea, *(char*)"\t");
		char* ip = registro[0];
		char* hora = strdup(registro[1]);
		if (!hash_pertenece(hash,ip)){
			lista_t* lista_horarios = lista_crear();
			hash_guardar(hash, ip, lista_horarios);
			abb_guardar(abb_ips, ip, NULL);
		}

		lista_insertar_ultimo(hash_obtener(hash, ip), hora);
		leidos = getline(&linea, &tam_linea, log);
		free_strv(registro);
	}
	free(linea);

	abb_t* abb_DoS = encontrar_DoS(hash);
	imprimir_DoS(abb_DoS);
	abb_destruir(abb_DoS);

	hash_destruir(hash);
  fclose(log);
  return true;
}

void ver_visitantes(char* ip1, char* ip2, abb_t* abb_ips){

	printf("Visitantes:\n");
	abb_iter_t* iterador_abb = abb_iter_in_crear(abb_ips);

	while (!abb_iter_in_al_final(iterador_abb) && funcion_cmp_ip(ip1, abb_iter_in_ver_actual(iterador_abb)) >= 0){
		abb_iter_in_avanzar(iterador_abb);
	}

	while (!abb_iter_in_al_final(iterador_abb) && funcion_cmp_ip(abb_iter_in_ver_actual(iterador_abb), ip2) <= 0){
		printf("\t%s\n", abb_iter_in_ver_actual(iterador_abb));
		abb_iter_in_avanzar(iterador_abb);
	}

	abb_iter_in_destruir(iterador_abb);
}

bool comprobar_parametros(int funcion, char** campos){
	
	int i = 0;

	while(campos[i]){
		i++;
	}

	if (funcion == 0 && i != 3) return false;
	if (funcion == 1 && i != 2) return false;
	if (funcion == 2 && i != 3) return false;

	return true;
}

char* parsear_linea(char* linea,size_t numero_campo,const char* funcion){
  char** campos = split(linea,' ');
  char* campo;

  if(numero_campo==2 || (strcmp(funcion,ORDENAR_ARCHIVO) && numero_campo == 1)){
    campo = strndup(campos[numero_campo],strlen(campos[numero_campo])-1);
   } else {
     campo = strndup(campos[numero_campo],strlen(campos[numero_campo]));
   }

  free_strv(campos);
  return campo;
}


size_t llamar_funcion(char* comando){

	size_t funcion;
	if(strcmp(comando, ORDENAR_ARCHIVO)==0){
		funcion = 0;
	} else if (strcmp(comando, AGREGAR_ARCHIVO)==0){
		funcion = 1;
	} else {
		funcion = 2;
	}

	return funcion;
}

int main(int argc, char* argv[]){

	if(argc != 2){
		fprintf(stderr,"Cantidad de parametros erronea\n");
		return 1;
	}

	size_t mem_disponible = atoi(argv[1]);

	abb_t* ab_ips = abb_crear(funcion_cmp_ip,free);
	if (!ab_ips) return -1;

	char* comando = NULL;
	size_t cap = 0;
	ssize_t leidos;

	leidos = getline(&comando,&cap,stdin);

	while(leidos!=-1){

		char** campos = split(comando, ' ');
		size_t funcion = llamar_funcion(campos[0]);

		if(funcion==0){
			if (!comprobar_parametros(funcion, campos)){
				fprintf(stderr,"Cantidad de parametros erronea\n");
			} else {
				char* input = parsear_linea(comando,1,ORDENAR_ARCHIVO);
				char* output = parsear_linea(comando,2,ORDENAR_ARCHIVO);
				if(ordenar_archivo(mem_disponible,input,output)){
					printf("OK\n");
				} else {
					fprintf(stderr,"Error en comando ordenar_archivo\n");
				}
				free(input);
				free(output);
			}
      		} else if (funcion==1) {
			if (!comprobar_parametros(funcion, campos)){
				fprintf(stderr,"Cantidad de parametros erronea\n");
			} else {
				char* input = parsear_linea(comando,1,AGREGAR_ARCHIVO);
				if(agregar_archivo(input,ab_ips)) printf("OK\n");
				else fprintf(stderr,"Error en comando agregar_archivo\n");
				free(input);
			}
		} else {
			if (!comprobar_parametros(funcion, campos)){
				fprintf(stderr,"Cantidad de parametros erronea\n");
			} else {
				if(abb_cantidad(ab_ips)==0){
					fprintf(stderr,"Error en comando ver_visitantes\n");
				} else {
					char* ip_a = parsear_linea(comando,1,VER_VISITANTES);
					char* ip_b = parsear_linea(comando,2,VER_VISITANTES);
					ver_visitantes(ip_a,ip_b,ab_ips);
					free(ip_a);
					free(ip_b);
					printf("OK\n");
				}
			}
		}
		leidos = getline(&comando,&cap,stdin);
	}

	abb_destruir(ab_ips);
	free(comando);
	return 0;
}
