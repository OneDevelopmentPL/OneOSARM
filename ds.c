/* OneOS-ARM Data Structures Implementation */

#include "ds.h"
#include "mem.h"

/* Linked List Implementation */
void list_init(list_t *list)
{
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

void list_push_back(list_t *list, void *data)
{
    list_node_t *node = (list_node_t *)kmalloc(sizeof(list_node_t));
    if (!node) return;

    node->data = data;
    node->next = NULL;
    node->prev = list->tail;

    if (list->tail) {
        list->tail->next = node;
    } else {
        list->head = node;
    }

    list->tail = node;
    list->size++;
}

void list_push_front(list_t *list, void *data)
{
    list_node_t *node = (list_node_t *)kmalloc(sizeof(list_node_t));
    if (!node) return;

    node->data = data;
    node->next = list->head;
    node->prev = NULL;

    if (list->head) {
        list->head->prev = node;
    } else {
        list->tail = node;
    }

    list->head = node;
    list->size++;
}

void *list_pop_back(list_t *list)
{
    if (!list->tail) return NULL;

    list_node_t *node = list->tail;
    void *data = node->data;

    list->tail = node->prev;
    if (list->tail) {
        list->tail->next = NULL;
    } else {
        list->head = NULL;
    }

    kfree(node);
    list->size--;

    return data;
}

void *list_pop_front(list_t *list)
{
    if (!list->head) return NULL;

    list_node_t *node = list->head;
    void *data = node->data;

    list->head = node->next;
    if (list->head) {
        list->head->prev = NULL;
    } else {
        list->tail = NULL;
    }

    kfree(node);
    list->size--;

    return data;
}

void *list_get(list_t *list, size_t index)
{
    if (index >= list->size) return NULL;

    list_node_t *current = list->head;
    for (size_t i = 0; i < index; i++) {
        current = current->next;
    }

    return current->data;
}

size_t list_size(list_t *list)
{
    return list->size;
}

void list_clear(list_t *list)
{
    while (list->head) {
        list_pop_front(list);
    }
}

/* Queue Implementation */
void queue_init(queue_t *queue)
{
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;
}

void queue_enqueue(queue_t *queue, void *data)
{
    list_node_t *node = (list_node_t *)kmalloc(sizeof(list_node_t));
    if (!node) return;

    node->data = data;
    node->next = NULL;

    if (queue->rear) {
        queue->rear->next = node;
    } else {
        queue->front = node;
    }

    queue->rear = node;
    queue->size++;
}

void *queue_dequeue(queue_t *queue)
{
    if (!queue->front) return NULL;

    list_node_t *node = queue->front;
    void *data = node->data;

    queue->front = node->next;
    if (!queue->front) {
        queue->rear = NULL;
    }

    kfree(node);
    queue->size--;

    return data;
}

void *queue_front(queue_t *queue)
{
    return queue->front ? queue->front->data : NULL;
}

size_t queue_size(queue_t *queue)
{
    return queue->size;
}

int queue_empty(queue_t *queue)
{
    return queue->size == 0;
}

/* Stack Implementation */
void stack_init(stack_t *stack)
{
    stack->top = NULL;
    stack->size = 0;
}

void stack_push(stack_t *stack, void *data)
{
    list_node_t *node = (list_node_t *)kmalloc(sizeof(list_node_t));
    if (!node) return;

    node->data = data;
    node->next = stack->top;

    stack->top = node;
    stack->size++;
}

void *stack_pop(stack_t *stack)
{
    if (!stack->top) return NULL;

    list_node_t *node = stack->top;
    void *data = node->data;

    stack->top = node->next;

    kfree(node);
    stack->size--;

    return data;
}

void *stack_top(stack_t *stack)
{
    return stack->top ? stack->top->data : NULL;
}

size_t stack_size(stack_t *stack)
{
    return stack->size;
}

int stack_empty(stack_t *stack)
{
    return stack->size == 0;
}
