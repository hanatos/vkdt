
// piece of code that can be used to traverse DAGs defined by any
// type that has a connector[] member.

// assume: index of start node is: int start_node_id
// assume: array of nodes is node_type arr[]

{ // scope
  uint32_t stack[256];
  uint8_t done[256];
  int sp = 0;

  stack[0] = start_node_id;
  done[0] = 0;

  while(sp >= 0)
  {
    int curr = stack[sp];
    if(done[sp])
    { // have worked on this node before, now the subtree is done, too.
      // TODO: post callback
      sp--;
    }
    else
    { // see it for the first time, push children
      // TODO: pre callback
      for(int i=0;i<arr[curr].num_connectors;i++)
      {
        if(arr[curr].connector[i].type == dt_token_t("read"))
        { // need to recurse all inputs
          stack[++sp] = arr[curr].connector[i].connected_mid;
          done[sp] = 0;
        }
        else if(arr[curr].connector[i].type == dt_token_t("source"))
        {
          // TODO: special source callback?
        }
        // TODO: if sink report error
        // TODO: if marked node, report cycle and error out
      }
    }
  }
} // end scope
