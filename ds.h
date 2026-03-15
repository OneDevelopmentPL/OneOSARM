/* OneOS-ARM Basic Data Structures */

#ifndef DS_H
#define DS_H

/* Define basic types for bare-metal */
typedef unsigned char uint8_t;
typedef unsigned int size_t;

/* Linked List */
typedef struct list_node {
    void *data;
    struct list_node *next;
    struct list_node *prev;
} list_node_t;

typedef struct {
    list_node_t *head;
    list_node_t *tail;
    size_t size;
} list_t;

/* Queue (FIFO) */
typedef struct {
    list_node_t *front;
    list_node_t *rear;
    size_t size;
} queue_t;

/* Stack (LIFO) */
typedef struct {
    list_node_t *top;
    size_t size;
} stack_t;

/* Linked List Operations */
void list_init(list_t *list);
void list_push_back(list_t *list, void *data);
void list_push_front(list_t *list, void *data);
void *list_pop_back(list_t *list);
void *list_pop_front(list_t *list);
void *list_get(list_t *list, size_t index);
size_t list_size(list_t *list);
void list_clear(list_t *list);

/* Queue Operations */
void queue_init(queue_t *queue);
void queue_enqueue(queue_t *queue, void *data);
void *queue_dequeue(queue_t *queue);
void *queue_front(queue_t *queue);
size_t queue_size(queue_t *queue);
int queue_empty(queue_t *queue);

/* Stack Operations */
void stack_init(stack_t *stack);
void stack_push(stack_t *stack, void *data);
void *stack_pop(stack_t *stack);
void *stack_top(stack_t *stack);
size_t stack_size(stack_t *stack);
int stack_empty(stack_t *stack);

#endif
