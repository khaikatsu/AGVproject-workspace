/* Display geometry for S1 simulation, following ADR-033 bay conventions. */
(function(root,factory){
  if(typeof module==='object'&&module.exports)module.exports=factory();
  else root.AgvSimMotion=factory();
})(globalThis,function(){
  const normalize=angle=>(angle%360+360)%360;
  const node=(map,id)=>(map.nodes||[]).find(n=>n.id===id);
  function bearing(map,from,to){
    const a=node(map,from),b=node(map,to);
    if(!a||!b||a.x===b.x&&a.y===b.y)return null;
    return normalize(Math.atan2(b.y-a.y,b.x-a.x)*180/Math.PI);
  }
  function gate(map,id){
    const bay=node(map,id);if(!['station','charging'].includes(bay?.type))return null;
    const neighbors=new Set((map.edges||[]).flatMap(e=>e.from===id?[e.to]:e.to===id?[e.from]:[]));
    return neighbors.size===1?[...neighbors][0]:null;
  }
  function parkedHeading(map,id){
    const door=gate(map,id);if(!door)return null;
    return node(map,id).type==='charging'?bearing(map,id,door):bearing(map,door,id);
  }
  function edgeHeading(map,from,to){
    // Warehouses exit backwards, with the nose still facing into the bay.
    return node(map,from)?.type==='station'&&gate(map,from)===to
      ?bearing(map,to,from):bearing(map,from,to);
  }
  function describe(robot,map,orders=[],jobs=[],lastHeading=null){
    const from=robot.edge?.from||robot.node,to=robot.edge?.to;
    const heading=robot.edge?edgeHeading(map,from,to)
      :parkedHeading(map,robot.node)??robot.headingDeg??lastHeading;
    const travel=robot.edge?bearing(map,from,to):null;
    const item=orders.filter(o=>o.status==='active').flatMap(o=>o.items||[])
      .find(i=>i.robot===robot.id&&!['queued','delivered','interrupted'].includes(i.status));
    const job=jobs.find(j=>j.robot===robot.id&&!j.ended);
    const phase=item?.status||job?.phase;
    let label='';
    if(robot.edge){
      const source=node(map,from)?.type,target=node(map,to)?.type;
      label=source==='station'?'Lùi ra kho':source==='charging'?'Tiến ra trạm'
        :target==='station'?'Tiến vào kho':target==='charging'?'Tiến vào trạm':'';
    }else if(phase==='pickup_service')label='Bốc hàng';
    else if(phase==='dropoff_service')label='Dỡ hàng';
    return {heading,travel,label,phase:phase||'',reverse:travel!==null&&Math.abs(normalize(heading-travel)-180)<.001};
  }
  return {normalize,bearing,gate,parkedHeading,edgeHeading,describe};
});
