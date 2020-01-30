// piece of code that can be used to traverse DAGs defined by any
// type that has a connector[] member.

// assume: number of nodes/modules is int arr_cnt
// assume: array of nodes is node_type arr[]

// setup all callbacks to do nothing
#ifndef TRAVERSE_POST
#define TRAVERSE_POST
#endif

{ // scope
  uint32_t stack[1000];         // node ids to work on
  uint32_t feedback_stack[100]; // feedback node ids
  uint8_t done[1000];           // size is stack size
  uint8_t mark[4000] = {0};     // size is number of nodes/modules
  // mark = 0: not touched
  // mark = 1: pushed to stack
  // mark = 2: ran pre
  // mark = 3: ran post
  assert(arr_cnt < sizeof(mark));
  int sp = -1;
  int sp2 = -1;

  // init this with all sink nodes/modules. invert order so the one last
  // in the list will be the last in post-order traversal and the first in
  // pre-order traversal. this is important for ROI computations.
  for(int i=arr_cnt-1;i>=0;i--) if(arr[i].connector[0].type == dt_token("sink"))
  {
    mark[i] = 1;
    stack[++sp] = i;
    done[sp] = 0;
  }

  while(1) {
  while(sp >= 0)
  {
    int curr = stack[sp];
    if(mark[curr] == 3)
    {
      sp--;
    }
    else if(done[sp])
    { // have worked on this node before, now the subtree is done, too.
      // this is also the time when all the inputs are ready
      // execute post callback, if any
      TRAVERSE_POST
      mark[curr] = 3;
      sp--;
    }
    else
    { // see it for the first time, push children
      // but do not remove the node yet. we still need it for the post traversal callback.
      // exec pre traversal callback before pushing children:
      if(mark[curr] < 2)
        mark[curr] = 2; // mark this node such that we only traverse the children once
      done[sp] = 1; // once we'll come back to this stack entry, deps will be done
      for(int i=0;i<arr[curr].num_connectors;i++)
      {
        const int el = arr[curr].connector[i].connected_mi;
        if(el < 0)
        {
          // not connected
        }
        else if(arr[curr].connector[i].type == dt_token("read") ||
                arr[curr].connector[i].type == dt_token("sink"))
        { // need to recurse all inputs
          if((mark[el] != 3) && // only if not completely finished (ran post callback)
            // feedback connectors only push their deps if not pushed before:
            !(arr[curr].connector[i].flags & s_conn_feedback))
          { // push to stack only unmarked
            assert((int64_t)sp < (int64_t)sizeof(stack)/sizeof(stack[0])-1);
            stack[++sp] = el;
            done[sp] = 0;
            if(mark[el] < 1) mark[el] = 1;
          }
          if(arr[curr].connector[i].flags & s_conn_feedback)
          {
            assert((int64_t)sp2 < (int64_t)(sizeof(feedback_stack)/sizeof(feedback_stack[0])-1));
            feedback_stack[++sp2] = el;
            mark[el] = 1;
          }
        }
      }
    }
  }
  if(sp2 >= 0)
  {
    memcpy(stack, feedback_stack, sizeof(stack[0])*(sp2+1));
    memset(done, 0, sizeof(done[0])*(sp2+1));
    sp = sp2;
    sp2 = -1;
  }
  else break;
  }
} // end scope
// clean up
#undef TRAVERSE_POST
