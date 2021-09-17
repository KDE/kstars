/*
** Author: Eric Veach, July 1994.
**
*/

#include <stddef.h>
#include "dict-list.h"
#include "memalloc.h"

/* really __gl_dictListNewDict */
Dict *dictNewDict(void *frame, int (*leq)(void *frame, DictKey key1, DictKey key2))
{
    Dict *dict = (Dict *)memAlloc(sizeof(Dict));
    DictNode *head;

    if (dict == NULL)
        return NULL;

    head = &dict->head;

    head->key  = NULL;
    head->next = head;
    head->prev = head;

    dict->frame = frame;
    dict->leq   = leq;

    return dict;
}

/* really __gl_dictListDeleteDict */
void dictDeleteDict(Dict *dict)
{
    DictNode *node, *next;

    for (node = dict->head.next; node != &dict->head; node = next)
    {
        next = node->next;
        memFree(node);
    }
    memFree(dict);
}

/* really __gl_dictListInsertBefore */
DictNode *dictInsertBefore(Dict *dict, DictNode *node, DictKey key)
{
    DictNode *newNode;

    do
    {
        node = node->prev;
    } while (node->key != NULL && !(*dict->leq)(dict->frame, node->key, key));

    newNode = (DictNode *)memAlloc(sizeof(DictNode));
    if (newNode == NULL)
        return NULL;

    newNode->key     = key;
    newNode->next    = node->next;
    node->next->prev = newNode;
    newNode->prev    = node;
    node->next       = newNode;

    return newNode;
}

/* really __gl_dictListDelete */
void dictDelete(Dict *dict, DictNode *node) /*ARGSUSED*/
{
    node->next->prev = node->prev;
    node->prev->next = node->next;
    memFree(node);
}

/* really __gl_dictListSearch */
DictNode *dictSearch(Dict *dict, DictKey key)
{
    DictNode *node = &dict->head;

    do
    {
        node = node->next;
    } while (node->key != NULL && !(*dict->leq)(dict->frame, key, node->key));

    return node;
}
