{
  // connect source|write (m0,c0) -> sink|read (m1,c1)
  // reads are unique, writes can be connected to multiple others
  // we only support disconnecting via (-1,-1,module,connector), i.e. the
  // reading side always needs to be known to us, because it allows us to
  // reference back to the source uniquely.
  if(m1 < 0 && m1 >= graph->num_elements) return 1;
  if(c1 < 0 && c1 >= graph->element[m1].num_connectors) return 2;

  // m1 can only read one connection. i.e. we need to disconnect any existing
  // connection over there: break existing connections, if any. that is:
  // decrement reference counter on write/source side unset connector id on
  // read/sink side

  // find node that m1 is currently connected to via c1 (where is the data currently coming from?)
  dt_connector_t *cn1 = graph->element[m1].connector+c1;
  if(cn1->type != dt_token("read") && cn1->type != dt_token("sink")) return 3;

  int old_mod = cn1->connected_mi;
  if(old_mod >= 0)
  {
    int old_con = cn1->connected_mc;
    if(old_mod >= graph->num_elements) { assert(0 && "sink inconsistent!"); return 4; }
    if(graph->element[old_mod].connector[old_con].type != dt_token("write") &&
       graph->element[old_mod].connector[old_con].type == dt_token("source"))
    { assert(0 && "sink inconsistent!"); return 5; }

    // disconnect cn1
    cn1->connected_mi = cn1->connected_mc = -1;

    // decrement previous source's reference counter
    if(graph->element[old_mod].connector[old_con].connected_mi > 0)
    {
      graph->element[old_mod].connector[old_con].connected_mi--;
      if(graph->element[old_mod].connector[old_con].connected_mi < 0)
      { assert(0 && "sink inconsistent!"); return 6; }
    }
    else
    { assert(0 && "sink inconsistent!"); return 6; }
  }

  // actually just disconnect requested? we're done now, then:
  if(c0 < 0 || m0 < 0) return 0;

  // we're now in a consistent state to fail, leaving everything
  // disconnected but intact.

  if(m0 >= graph->num_elements) return 7;
  if(c0 >= graph->element[m0].num_connectors) return 8;

  // connect the two in the graph
  dt_connector_t *cn0 = graph->element[m0].connector+c0;
  if(cn0->type != dt_token("write") && cn0->type != dt_token("source")) return 9;

  // check buffer config for compatibility
  if(cn1->chan != cn0->chan) return 10;
  if(cn1->format != cn0->format) return 11;

  // now we checked everything will be alright, so we can proceed and execute
  // the connection in one go, leaving us with a consintent state after.

  // connect input id
  cn1->connected_mi = m0;
  cn1->connected_mc = c0;

  // back connections are not unique, so we're just using this
  // as a reference counter, so we know there is /some/ connection.
  // we know which is which because cn0->type == "write"|"source".
  cn0->connected_mi++;
  cn0->connected_mc = 0;

  return 0;
}

