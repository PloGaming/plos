#include <common/dll.h>
#include <stdbool.h>

/**
 * @brief Initializes the list
 * This function initializes a circular double linked list
 * @param list The pointer to the list
 */
inline void dll_init(struct double_ll_node *list)
{
    list->next = list;
    list->prev = list;
}

/**
 * @brief Adds a node between 2 nodes (prev and next)
 * 
 * @param new The node to add
 * @param prev The previous node
 * @param next The next node
 */
static inline void _dll_add(struct double_ll_node *new, struct double_ll_node *prev, struct double_ll_node *next)
{
    prev->next = new;
    new->prev = prev;
    new->next = next;
    next->prev = new;
}

/**
 * @brief Adds a node after the current one
 * 
 * @param head The current head pointer
 * @param new The new node we want to add
 */
inline void dll_add_after(struct double_ll_node *head, struct double_ll_node *new)
{
    _dll_add(new, head, head->next);
}

/**
 * @brief Adds a node before the current one
 * 
 * @param head The current head pointer
 * @param new The new node we want to add
 */
inline void dll_add_before(struct double_ll_node *head, struct double_ll_node *new)
{
    _dll_add(new, head->prev, head);
}
 
/**
 * @brief Deletes a node by linking the prev with the next
 * 
 * @param node The node we want to delete from the list
 */
inline void dll_delete(struct double_ll_node *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

/**
 * @brief Function to determine if a list is empty
 * 
 * @param head The head of the list
 * @return true if the list is emtpy
 * @return false if the list has at least 1 element
 */
inline bool dll_empty(struct double_ll_node *head)
{
    return head->next == head && head->prev == head;
}