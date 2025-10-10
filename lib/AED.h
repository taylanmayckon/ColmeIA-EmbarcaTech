#ifndef AED_H
#define AED_H

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#define MAX_FILA 40

typedef struct nodo{
    TickType_t timestamp;
    struct nodo *next;
} NODO;

typedef struct{
    NODO *INICIO;
    NODO *FIM;
} DESCRITOR;

typedef DESCRITOR *FILA;

void cria_fila(FILA *pf);
int fila_vazia(FILA f);
void inserir_fila(FILA f, TickType_t timestamp);
TickType_t consulta_fila(FILA f);
void retira_fila(FILA f);
void destruir_fila(FILA f);

#endif