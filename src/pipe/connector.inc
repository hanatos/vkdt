{
  if(m0 < 0 && m0 >= graph->num_elements) return 1;
  if(m1 > 0 && m1 >= graph->num_elements) return 2;
  // TODO: connect the two in the graph
  dt_connector_t *cn0 = 0, *cn1 = 0;
  for(int k=0;k<10;k++) if(graph->element[m0].connector[k].name == c0)
  {
    cn0 = graph->element[m0].connector+k;
    break;
  }
  if(!cn0) return 3;
  for(int k=0;k<10;k++) if(graph->element[m1].connector[k].name == c1)
  {
    cn1 = graph->element[m1].connector+k;
    break;
  }
  if(!cn1) return 4;

  // check buffer config for compatibility
  if(cn1->type != cn0->type) return 5;
  if(cn1->chan != cn0->chan) return 6;
  if(cn1->format != cn0->format) return 7;

  // connect input id
  cn1->connected_mid = m0;
  cn1->connected_cid = cn0-graph->element[m0].connector;
  // TODO: ??
  cn0->connected_mid = 0; // in general these could be many, so we can't connect like this
  cn0->connected_cid = 0;

  return 0;
}

