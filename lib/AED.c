#include "AED.h"

void cria_fila(FILA *pf){
    *pf = (DESCRITOR *)malloc(sizeof(DESCRITOR));

    if(!(*pf)) {
        printf("[ERRO NA FILA] Memória insuficiente!\n");
    }

    (*pf)->INICIO = (*pf)->FIM = NULL;
}


int fila_vazia(FILA f){
    return (f->INICIO == NULL);
}


void inserir_fila(FILA f, TickType_t timestamp){
    NODO *novo;
    novo = (NODO *) malloc(sizeof(NODO));

    if(!novo) printf("\nERRO! Memoria insuficiente para a fila.\n");

    novo->timestamp = timestamp;
    novo->next = NULL;

    if(fila_vazia(f))
        f->INICIO=novo;
    else
        f->FIM->next=novo;
    f->FIM=novo;
}


TickType_t consulta_fila(FILA f){
    if(fila_vazia(f)) return 0; // Depois pensa em um jeito melhor

    else return (f->INICIO->timestamp);
}


void retira_fila(FILA f){
    if(fila_vazia(f)) return 0; // Não sei se vai dar erro, mas é só para garantir
    else{
        NODO *aux = f->INICIO->next;
        f->INICIO = f->INICIO->next;
        if (!f->INICIO) f->FIM=NULL;
        free(aux);
    }
}


void destruir_fila(FILA f){
    // Vê se é isso mesmo a função de destruir para essa aplicação, e lembra de criar dnv depois
    NODO *aux;
    while(f->INICIO){
        aux=f->INICIO;
        f->INICIO = f->INICIO->next;
        free(aux);
    }
    free(f);
}