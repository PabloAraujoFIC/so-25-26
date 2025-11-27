// Pablo Araújo Rodríguez   pablo.araujo@udc.es
// Uriel Liñares Vaamonde   uriel.linaresv@udc.es

#include "lista.h"

// Crear lista
List* createList(void) {
    List *list = malloc(sizeof(List));
    if (!list) {
        fprintf(stderr, "Error: could not create list\n");
        exit(EXIT_FAILURE);
    }
    list->head = NULL;
    return list;
}
// Inicializar lista vacía
void initList(List *list) {
    if (list) list->head = NULL;
}

// Comprobar si la lista está vacía
bool isEmptyList(List *list) {
    return (list == NULL || list->head == NULL);
}

// Insertar elemento (al inicio por simplicidad)
int insertItem(List *list, void *data) {
    if (!list) return 1;

    Node *newNode = malloc(sizeof(Node));
    if (!newNode) {
        fprintf(stderr, "Error: could not insert item\n");
        exit(EXIT_FAILURE);
    }

    newNode->data = data;
    newNode->prev = NULL;
    newNode->next = list->head;

    if (list->head)
        list->head->prev = newNode;
    newNode->id = 0;
    list->head = newNode;
    return 0;
}

// Buscar un elemento (según función de comparación)
void* findItem(List *list, void *key, int (*cmp)(void*, void*)) {
    if (!list) return NULL;
    Node *current = list->head;
    while (current) {
        if (cmp(current->data, key) == 0) {
            return current->data;
        }
        current = current->next;
    }
    return NULL;
}

// Borrar un elemento (según comparación)
void deleteItem(List *list, void *key, int (*cmp)(void*, void*),
                void (*destroy)(void*)) {
    if (!list || !list->head) return;

    Node *current = list->head;
    Node *prev = NULL;

    while (current) {
        if (cmp(current->data, key) == 0) {
            Node *next = current->next;
            if (prev == NULL) {
                list->head = next;
            } else {
                prev->next = next;
            }
            if (next) next->prev = prev;
            if (destroy) destroy(current->data);
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

// Eliminar lista completa
void deleteList(List *list, void (*destroy)(void*)) {
    if (!list) return;
    Node *current = list->head;
    while (current) {
        Node *tmp = current;
        current = current->next;
        if (destroy) destroy(tmp->data);
        free(tmp);
    }
    free(list);
}

void recorrerLista(List *list)
{
    if (!list) return;
    Node *current = list->head;
    while (current)
    {
        printf("%p\n", current->data);
        current = current->next;
    }
}

void *getItem(List *list, tPos pos) {
    if (!list || !pos) return NULL;
    for (Node *current = list->head; current; current = current->next) {
        if (current == pos) return current->data;
    }
    return NULL;
}

void * first(List *list)
{
    if (!list) return NULL;
    return list->head;
}

void * last(List *list)
{
    tPos current = NULL;
    if (!list) return NULL;
    for (current = list->head; current->next == NULL; current = current->next);
    return current;
}

void clearList(List *list, void (*destroy)(void*))
{
    if (!list) return;
    Node *current = list->head;
    while (current) {
        Node *tmp = current;
        current = current->next;
        if (destroy) destroy(tmp->data);
        free(tmp);
    }
    list->head = NULL;
}
