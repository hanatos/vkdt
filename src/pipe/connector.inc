{
  // connect source|write|modify (m0,c0) -> sink|read|modify (m1,c1)
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
  if(cn1->connected.i == m0 && cn1->connected.c == c0) return 0; // already connected
  if(cn1->type != dt_token("read") &&
     cn1->type != dt_token("sink") &&
     cn1->type != dt_token("modify")) return 3;

  int old_mod = cn1->connected.i;
  if(old_mod >= 0)
  {
    int old_con = cn1->connected.c;
    if(old_mod >= graph->num_elements) { assert(0 && "sink inconsistent!"); return 4; }
    if(graph->element[old_mod].name && // skip deleted modules
       graph->element[old_mod].connector[old_con].type != dt_token("write") &&
       graph->element[old_mod].connector[old_con].type != dt_token("source") &&
       graph->element[old_mod].connector[old_con].type != dt_token("modify"))
    { assert(0 && "sink inconsistent!"); return 5; }

    // disconnect cn1
    cn1->connected.i = cn1->connected.c = -1;
#ifdef connect_module
    // this is the sink. the connector has a format that has been previously negotiated with the
    // previous source. reset to the generic module->so version:
    cn1->format = graph->element[m1].so->connector[c1].format;
    cn1->frames = graph->element[m1].so->connector[c1].frames;
    cn1->chan   = graph->element[m1].so->connector[c1].chan;
#endif

    // decrement previous source's reference counter
    if(graph->element[old_mod].connector[old_con].connected.i > 0)
    {
      graph->element[old_mod].connector[old_con].connected.i--;
      if(graph->element[old_mod].connector[old_con].connected.i < 0)
      { assert(0 && "sink inconsistent!"); return 6; }
#ifdef connect_module
      else if(graph->element[old_mod].connector[old_con].connected.i == 0)
      { // this is the old source, now disconnected.
        // replace the format of this connector by the (potentially generic) module->so version
        graph->element[old_mod].connector[old_con].format = graph->element[old_mod].so->connector[old_con].format;
        graph->element[old_mod].connector[old_con].frames = graph->element[old_mod].so->connector[old_con].frames;
        graph->element[old_mod].connector[old_con].chan   = graph->element[old_mod].so->connector[old_con].chan;
        graph->element[old_mod].connector[old_con].flags &= ~s_conn_feedback;
      }
#endif
    }
    else
    { assert(0 && "sink inconsistent!"); return 6; }
  }

  // cn0 is the source. it has either previously negotiated a specialised format with a different
  // sink, in which case we need to respect it. or it didn't yet, in which case it will still
  // have the wildcard format "*" which means we don't have to touch it.
  // the only case in which we'll have to reset is when an explicit disconnect takes place.
  // note that this only happens with (-1, -1, sink, sink), so this case is covered above
  // by resetting the previously connected input (old source).

  // to be in a consistent state need to point out that we are an explicit
  // connection, not created by copying a module connection to a node:
  graph->element[m1].connector[c1].associated = s_cid_unset;

  // actually just disconnect requested? we're done now, then:
  if(c0 < 0 || m0 < 0) return 0;

  graph->element[m0].connector[c0].associated = s_cid_unset;

  // we're now in a consistent state to fail, leaving everything
  // disconnected but intact.

  if(m0 >= graph->num_elements) return 7;
  if(c0 >= graph->element[m0].num_connectors) return 8;

  // connect the two in the graph
  dt_connector_t *cn0 = graph->element[m0].connector+c0;
  if(cn0->type != dt_token("write") && cn0->type != dt_token("source") && cn0->type != dt_token("modify")) return 9;

  // check buffer config for compatibility
  // if output is already connected to another input, use this format as mandatory (can't be "*" any more)
  // if any one is "*" use the other
  // if both are "*" fall back to default "f16"
  // this should happen on the module level and is done just before the buffers are created on the graph
  int c0ref = dt_token_str(cn0->chan)[0] == '&';
  if(cn1->chan == dt_token("*") && !c0ref) cn1->chan = cn0->chan;
  if(cn0->chan == dt_token("*")) cn0->chan = cn1->chan;
  if(cn1->chan == dt_token("*")) cn1->chan = dt_token("rgba");
  if(cn0->chan == dt_token("*")) cn0->chan = dt_token("rgba");
  if(!c0ref && cn1->chan != cn0->chan) return 10;
  if(cn1->format == dt_token("*")) cn1->format = cn0->format;
  if(cn0->format == dt_token("*")) cn0->format = cn1->format;
  if(cn1->format == dt_token("*")) cn1->format = dt_token("f16");
  if(cn0->format == dt_token("*")) cn0->format = dt_token("f16");
  if(cn1->format != cn0->format) return 11;

  // now we checked everything will be alright, so we can proceed and execute
  // the connection in one go, leaving us with a consintent state after.

  // connect input id
  cn1->connected = (dt_cid_t){m0, c0};
  cn1->array_length = cn0->array_length; // copy from what is written to output
  cn1->flags = cn0->flags;
  cn1->flags &= ~s_conn_feedback; // not a feedback connector by default

  cn1->roi = cn0->roi; // copy the buffer extents

  // back connections are not unique, so we're just using this
  // as a reference counter, so we know there is *some* connection.
  // cn0->type == "write"|"source"|"modify" at this point, but "modify" types don't allocate/own a buffer
  if(cn0->type == dt_token("write") || cn0->type == dt_token("source"))
    cn0->connected.i++;

  // reset to common default. this is rather an esoteric use case that usually requires
  // custom create_nodes code to do the right thing (useful for concatenations of buffers as in unets).
  // such code has to *first* connect, then overwrite the offset manually on the node connectors.
  cn0->ssbo_offset = cn1->ssbo_offset = 0;

  return 0;
}
