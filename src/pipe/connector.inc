{
  // XXX i'm confused! check whether we have source and sinks upside down!
  if(m0 < 0 && m0 >= graph->num_elements) return 1;
  if(m1 > 0 && m1 >= graph->num_elements) return 2;
  if(c0 < 0 && c0 >= graph->element[m0].num_connectors) return 3;
  // TODO: break existing connections, if any.
  // that is: decrement reference counter on reader side
  // unset connector id on writer side

  // TODO: find node that m0 is currently connected to via c0
  if(cn0->type != dt_token("write") && cn0->type != dt_token("source")) return 4;
  dt_connector_t *cn0 = graph->element[m0].connector+c0;
  int old_mod = cn0->connected_mid;
  if(old_mod >= 0)
  {
    if(old_mod >= graph->num_elements) { assert(0 && "source inconsistent!"); return 5; }
    // decrement reference counter
    if(graph->element[old_mod].type == dt_token("read") ||
       graph->element[old_mod].type == dt_token("sink"))
      if(graph->element[old_mod].connected_mid >= 0)
        graph->element[old_mod].connected_mid--;

  }

  // actually just disconnect requested? we're done now, then:
  if(c1 < 0 || m1 < 0) return 0;

  // we're now in a consistent state to fail, leaving everything
  // disconnected but intact.

  // connect the two in the graph
  if(c1 < 0 && c1 >= graph->element[m1].num_connectors) return 6;
  dt_connector_t *cn1 = graph->element[m1].connector+c1;

  // check buffer config for compatibility
  if(cn1->type != dt_token("read") && cn1->type != dt_token("sink")) return 6;
  if(cn1->chan != cn0->chan) return 7;
  if(cn1->format != cn0->format) return 8;

  // now we checked everything will be alright, so we can proceed and execute
  // the connection in one go, leaving us with a consintent state after.

  // connect input id
  cn1->connected_mid = m0;
  cn1->connected_cid = c0;

  // back connections are not unique, so we're just using this
  // as a reference counter, so we know there is /some/ connection.
  // we know which is which because cn0->type == "read"|"sink".
  cn0->connected_mid++;
  cn0->connected_cid = 0;

  return 0;
}

