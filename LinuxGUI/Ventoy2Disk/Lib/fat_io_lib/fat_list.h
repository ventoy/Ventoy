#ifndef __FAT_LIST_H__
#define __FAT_LIST_H__

#ifndef FAT_ASSERT
    #define FAT_ASSERT(x)
#endif

#ifndef FAT_INLINE
    #define FAT_INLINE
#endif

//-----------------------------------------------------------------
// Types
//-----------------------------------------------------------------
struct fat_list;

struct fat_node
{
    struct fat_node    *previous;
    struct fat_node    *next;
};

struct fat_list
{
    struct fat_node    *head;
    struct fat_node    *tail;
};

//-----------------------------------------------------------------
// Macros
//-----------------------------------------------------------------
#define fat_list_entry(p, t, m)     p ? ((t *)((char *)(p)-(char*)(&((t *)0)->m))) : 0
#define fat_list_next(l, p)         (p)->next
#define fat_list_prev(l, p)         (p)->previous
#define fat_list_first(l)           (l)->head
#define fat_list_last(l)            (l)->tail
#define fat_list_for_each(l, p)     for ((p) = (l)->head; (p); (p) = (p)->next)

//-----------------------------------------------------------------
// Inline Functions
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// fat_list_init:
//-----------------------------------------------------------------
static FAT_INLINE void fat_list_init(struct fat_list *list)
{
    FAT_ASSERT(list);

    list->head = list->tail = 0;
}
//-----------------------------------------------------------------
// fat_list_remove:
//-----------------------------------------------------------------
static FAT_INLINE void fat_list_remove(struct fat_list *list, struct fat_node *node)
{
    FAT_ASSERT(list);
    FAT_ASSERT(node);

    if(!node->previous)
        list->head = node->next;
    else
        node->previous->next = node->next;

    if(!node->next)
        list->tail = node->previous;
    else
        node->next->previous = node->previous;
}
//-----------------------------------------------------------------
// fat_list_insert_after:
//-----------------------------------------------------------------
static FAT_INLINE void fat_list_insert_after(struct fat_list *list, struct fat_node *node, struct fat_node *new_node)
{
    FAT_ASSERT(list);
    FAT_ASSERT(node);
    FAT_ASSERT(new_node);

    new_node->previous = node;
    new_node->next = node->next;
    if (!node->next)
        list->tail = new_node;
    else
        node->next->previous = new_node;
    node->next = new_node;
}
//-----------------------------------------------------------------
// fat_list_insert_before:
//-----------------------------------------------------------------
static FAT_INLINE void fat_list_insert_before(struct fat_list *list, struct fat_node *node, struct fat_node *new_node)
{
    FAT_ASSERT(list);
    FAT_ASSERT(node);
    FAT_ASSERT(new_node);

    new_node->previous = node->previous;
    new_node->next = node;
    if (!node->previous)
        list->head = new_node;
    else
        node->previous->next = new_node;
    node->previous = new_node;
}
//-----------------------------------------------------------------
// fat_list_insert_first:
//-----------------------------------------------------------------
static FAT_INLINE void fat_list_insert_first(struct fat_list *list, struct fat_node *node)
{
    FAT_ASSERT(list);
    FAT_ASSERT(node);

    if (!list->head)
    {
        list->head = node;
        list->tail = node;
        node->previous = 0;
        node->next = 0;
    }
    else
        fat_list_insert_before(list, list->head, node);
}
//-----------------------------------------------------------------
// fat_list_insert_last:
//-----------------------------------------------------------------
static FAT_INLINE void fat_list_insert_last(struct fat_list *list, struct fat_node *node)
{
    FAT_ASSERT(list);
    FAT_ASSERT(node);

    if (!list->tail)
        fat_list_insert_first(list, node);
     else
        fat_list_insert_after(list, list->tail, node);
}
//-----------------------------------------------------------------
// fat_list_is_empty:
//-----------------------------------------------------------------
static FAT_INLINE int fat_list_is_empty(struct fat_list *list)
{
    FAT_ASSERT(list);

    return !list->head;
}
//-----------------------------------------------------------------
// fat_list_pop_head:
//-----------------------------------------------------------------
static FAT_INLINE struct fat_node * fat_list_pop_head(struct fat_list *list)
{
    struct fat_node * node;

    FAT_ASSERT(list);

    node = fat_list_first(list);
    if (node)
        fat_list_remove(list, node);

    return node;
}

#endif

