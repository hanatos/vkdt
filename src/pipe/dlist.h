#pragma once

// doubly-linked list macros for all structs
// that have *prev and *next members.

// O(1) append to head
#define DLIST_PREPEND(L,E)\
{\
  (E)->prev = 0;  \
  (E)->next = (L);\
  if(L) (L)->next = (E);\
  return (E);\
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
