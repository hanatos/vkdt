// piece of code that can be used to traverse DAGs defined by any
// type that has a connector[] member.

// assume: index of start node is: int start_node_id
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
  int sp = 0;

  // TODO: init this with all sink nodes
  stack[0] = start_node_id;
  done[0] = 0;

  while(sp >= 0)
  {
    int curr = stack[sp];
    if(mark[curr])
    {
      // it seems another dependency has pulled in this node and deps before us.
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
      // pre traversal callback
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
          if(mark[el])
          {
            // in fact this is not enough to detect a cycle.
            // it just means that we already processed this part of a bifurcation.
            // no need to push dependency to stack, it's already done + marked.
          }
          else
          { // push to stack only unmarked:
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
