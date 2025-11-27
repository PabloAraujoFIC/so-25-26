// Pablo Araújo Rodríguez   pablo.araujo@udc.es
// Uriel Liñares Vaamonde   uriel.linaresv@udc.es

#ifndef LISTA_H
#define LISTA_H
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

typedef struct Node * tPos;

typedef struct Node {
    int id;
    void *data;
    struct Node *next;
    struct Node *prev;
} Node;

typedef struct { Node *head; } List;

List* createList(void);
void initList(List *list);
bool isEmptyList(List *list);
int insertItem(List *list, void *data);
void* findItem(List *list, void *key, int (*cmp)(void*, void*));
void deleteItem(List *list, void *key, int (*cmp)(void*, void*),
                void (*destroy)(void*));
void deleteList(List *list, void (*destroy)(void*));
void clearList(List *list, void (*destroy)(void*));
void recorrerLista(List *list);
void *getItem(List *list, tPos index);
void * first(List *list);
void * last(List *list);
#endif //LISTA_H
