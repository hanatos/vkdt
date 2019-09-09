// piece of code that can be used to traverse DAGs defined by any
// type that has a connector[] member.

// assume: number of nodes/modules is int arr_cnt
// assume: array of nodes is node_type arr[]

// setup all callbacks to do nothing
#ifndef TRAVERSE_POST
#define TRAVERSE_POST
#endif
#ifndef TRAVERSE_PRE
#define TRAVERSE_PRE
#endif
#ifndef TRAVERSE_CYCLE
#define TRAVERSE_CYCLE
#endif
#ifndef TRAVERSE_UNCONNECTED
#define TRAVERSE_UNCONNECTED
#endif

{ // scope
  uint32_t stack[256];
  uint8_t done[256];
  uint8_t mark[256] = {0};
  assert(arr_cnt < sizeof(mark));
  int sp = -1;

  // init this with all sink nodes/modules
  for(int i=0;i<arr_cnt;i++)
  {
    if(arr[i].connector[0].type == dt_token("sink"))
    {
      stack[++sp] = i;
      done[sp] = 0;
    }
  }

  while(sp >= 0)
  {
    int curr = stack[sp];
    if(mark[curr])
    {
      sp--;
    }
    else if(done[sp])
    { // have worked on this node before, now the subtree is done, too.
      // this is also the time when all the inputs are ready
      // execute post callback, if any
      TRAVERSE_POST
      mark[curr] = 1;
      sp--;
    }
    else
    { // see it for the first time, push children
      // but do not remove the node yet. we still need it for the post traversal callback.
      // exec pre traversal callback before pushing children:
      TRAVERSE_PRE
      done[sp] = 1; // mark this node such that we only traverse the children once
      for(int i=0;i<arr[curr].num_connectors;i++)
      {
        const int el = arr[curr].connector[i].connected_mi;
        if(el < 0)
        {
          TRAVERSE_UNCONNECTED
        }
        else if(arr[curr].connector[i].type == dt_token("read") ||
                arr[curr].connector[i].type == dt_token("sink"))
        { // need to recurse all inputs
          if(!mark[el])
          { // push to stack only unmarked
            assert(sp < sizeof(done)-1);
            stack[++sp] = el;
            done[sp] = 0;
          }
        }
      }
    }
  }
} // end scope
// clean up
#undef TRAVERSE_POST
#undef TRAVERSE_PRE
#undef TRAVERSE_CYCLE
#undef TRAVERSE_UNCONNECTED
