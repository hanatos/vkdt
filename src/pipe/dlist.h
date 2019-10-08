#pragma once
// doubly-linked list macros for all structs
// that have *prev and *next members.
// apologies for, uhm, every line of code in this file i think.

// O(1) prepend to head
#define DLIST_PREPEND(L,E) ({\
  (E)->prev = (L)?(L)->prev:0;\
  (E)->next = (L);\
  if(L && (L)->prev) (L)->prev->next = (E);\
  if(L) (L)->prev = (E);\
  (E);\
})

// O(1) append after L
#define DLIST_APPEND(L,E) ({\
  (E)->prev = (L);\
  (E)->next = (L)?(L)->next:0;\
  if(L && (L)->next) (L)->next->prev = (E);\
  if(L) (L)->next = (E);\
  (E);\
})

// O(1) remove specific element
#define DLIST_RM_ELEMENT(E) {\
  if((E)->prev) (E)->prev->next = (E)->next;\
  if((E)->next) (E)->next->prev = (E)->prev;\
  (E)->next = (E)->prev = 0;\
}

// O(n) remove from list
#define DLIST_REMOVE(L,E) ({\
__typeof(L) R = ((E)==(L))? (E)->next : (L);\
for(__typeof(L) I=(L);I!=0;I=I->next) {\
  if(I==(E)) {\
    if(I->prev) I->prev->next = I->next;\
    if(I->next) I->next->prev = I->prev;\
    I->prev = I->next = 0;\
    break;\
  }\
}\
R;\
})

// O(n) compute length
#define DLIST_LENGTH(L) ({\
int l = 0;\
for(__typeof(L) I=(L);I!=0;I=I->next) {\
  l++;\
}\
l;\
})
