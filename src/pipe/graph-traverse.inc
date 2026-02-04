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
  assert((int64_t)arr_cnt < (int64_t)sizeof(mark));
  int sp = -1;
  int sp2 = -1;

  // init this with all sink nodes/modules. invert order so the one last
  // in the list will be the last in post-order traversal and the first in
  // pre-order traversal. this is important for ROI computations.
  // also, put ones in module instances named "main" first:
#ifndef __cplusplus
#define get_instance(X) _Generic(X, \
    dt_node_t*   : dt_node_get_instance,\
    dt_module_t* : dt_module_get_instance)
#else
  typedef dt_token_t (*retfn)(dt_node_t*);
  typedef dt_token_t (*retfm)(dt_module_t*);
  struct cplusplussucks {
    retfn operator()(dt_node_t* n)   { return &dt_node_get_instance; }
    retfm operator()(dt_module_t* m) { return &dt_module_get_instance; }
  } get_instance;
#endif

  for(int i=0;i<arr_cnt;i++)
    if(arr[i].connector[0].type == dt_token("sink") &&
        dt_connected(arr[i].connector) &&
        get_instance(arr+i)(arr+i) == dt_token("main"))
  {
    mark[i] = 1;
    stack[++sp] = i;
    done[sp] = 0;
  }
#undef get_instance
  for(int i=arr_cnt-1;i>=0;i--)
    if(arr[i].connector[0].type == dt_token("sink") &&
       dt_connected(arr[i].connector) &&
       !mark[i])
  {
    mark[i] = 1;
    stack[++sp] = i;
    done[sp] = 0;
  }
  for(int i=0;i<arr_cnt;i++) if(arr[i].name == dt_token("bvh"))
  { // bvh need to be constructed first
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
        const int el = arr[curr].connector[i].connected.i;
        if(el < 0)
        {
          // not connected
        }
        else if(arr[curr].connector[i].type == dt_token("read") ||
                arr[curr].connector[i].type == dt_token("sink") ||
                arr[curr].connector[i].type == dt_token("modify"))
        { // need to recurse all inputs
          if((mark[el] != 3) && // only if not completely finished (ran post callback)
            // feedback connectors only push their deps if not pushed before:
            !(arr[curr].connector[i].flags & s_conn_feedback))
          { // push to stack only unmarked
#ifndef __cplusplus
#if 0
            if(((int64_t)sp >= (int64_t)(sizeof(stack)/sizeof(stack[0])-1)))
              fprintf(stderr, "graph cyclic at node %d\n", dt_graph_nodes_are_cyclic(graph));
#endif
#endif
            // assert((int64_t)sp < (int64_t)(sizeof(stack)/sizeof(stack[0])-1));
            if((int64_t)sp >= (int64_t)(sizeof(stack)/sizeof(stack[0])-1))
            { sp = sp2 = -1; break; }
            stack[++sp] = el;
#ifndef __cplusplus
#if 0
            if((size_t)arr == (size_t)graph->node) fprintf(stderr, "node %02d %8" PRItkn " %8" PRItkn
                " <- %02d %8" PRItkn " %8" PRItkn "\n",
                el,
                dt_token_str(graph->node[el].name),
                dt_token_str(graph->node[el].kernel),
                curr,
                dt_token_str(graph->node[curr].kernel),
                dt_token_str(graph->node[curr].connector[i].name));
#endif
#endif
            done[sp] = 0;
            if(mark[el] < 1) mark[el] = 1;
          }
          if((mark[el] != 3) && (arr[curr].connector[i].flags & s_conn_feedback)) // only push unfinished feedback connectors
          {
            // assert((int64_t)sp2 < (int64_t)(sizeof(feedback_stack)/sizeof(feedback_stack[0])-1));
            if((int64_t)sp2 >= (int64_t)(sizeof(feedback_stack)/sizeof(feedback_stack[0])-1))
            { sp = sp2 = -1; break; }
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
