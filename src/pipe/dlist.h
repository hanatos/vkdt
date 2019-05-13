#pragma once

// doubly-linked list macros for all structs
// that have *prev and *next members.

// O(1) append to head
#define DLIST_PREPEND(L,E) {\
  (E)->prev = (L)?(L)->prev:0;\
  (E)->next = (L);\
  if(L) (L)->next = (E);\
  return (E);\
}

#define DLIST_RM_ELEMENT(E) {\
  if((E)->prev) (E)->prev->next = (E)->next;
  if((E)->next) (E)->next->prev = (E)->prev;
  (E)->next = (E)->prev = 0;
}

// O(n) remove from list
#define DLIST_REMOVE(L,E)\
for(__typeof(L) I=(L);I!=0;I=I->next) {\
  if(I==(E)) {\
    if(I->prev) I->prev->next = I->next;\
    if(I->next) I->next->prev = I->prev;\
    I->prev = I->next = 0;\
    return ((L)==(E)) ? 0 : (L);\
  }\
}
