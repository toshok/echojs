
#include "ejs.h"
#include "ejs-types.h"

void
_ejs_list_append_node (EJSList *list, EJSListNode* node)
{
  if (list->tail == NULL) {
    // the list is empty, make this node both the head and tail
    list->head = list->tail = node;
    node->prev = node->next = NULL;
  }
  else {
    list->tail->next = node;
    node->prev = list->tail;
    node->next = NULL;
    list->tail = node;
  }
}

void
_ejs_list_prepend_node (EJSList *list, EJSListNode* node)
{
  if (list->head == NULL) {
    // the list is empty, make this node both the head and tail
    list->head = list->tail = node;
    node->prev = node->next = NULL;
  }
  else {
    list->tail->prev = node;
    node->next = list->head;
    node->prev = NULL;
    list->head = node;
  }
}

void
_ejs_list_insert_node_sorted (EJSList *list, EJSListNode* node, EJSCompareFunc compare)
{
  if (list->head == NULL) {
    // the list is empty, make this node both the head and tail
    list->head = list->tail = node;
    node->prev = node->next = NULL;
  }
  else {
    EJSListNode *cur = list->head;

    // when this loop is done, cur is either NULL or the node we need to insert @node before.
    while (cur) {
      if (compare(node, cur))
	break;
      cur = cur->next;
    }

    if (cur) {
      // we found the spot someplace in the list.
      node->prev = cur->prev;
      node->next = cur;

      if (cur->prev)
	cur->prev->next = node;

      if (cur->next)
	cur->next->prev = node;

      if (cur == list->head)
	list->head = node;
    }
    else {
      // we hit the end of the list, append the node
      _ejs_list_append_node (list, node);
    }
  }
}

void
_ejs_list_pop_head (EJSList *list)
{
  EJSListNode* head = list->head;
  if (!head)
    return;

  list->head = head->next;
  if (head == list->tail)
    list->tail = NULL;
}

int
_ejs_list_length (EJSList *list)
{
    int len = 0;
    EJSListNode* el = list->head;
    while (el) {
        len ++;
        el = el->next;
    }
    return len;
}
