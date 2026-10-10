// Disposable, repeatable host cases. Run after ae_smoke.jsx succeeds, from its own test project.
(function(){
 var root='E:/Custom Apps/make-grain-now-premiere';
 var log=new File(root+'/tests/outputs/ae-stacking.txt');log.encoding='UTF-8';log.open('w');
 function say(s){log.writeln(s);log.close();log.open('a');}
 function texture(layer){var p=layer.property('ADBE Effect Parade').addProperty('MGN Film Texture');p.property('Grain amount').setValue(0);p.property('Halation amount').setValue(40);p.property('Bloom amount').setValue(20);return p;}
 function comp(name){return app.project.items.addComp(name,1920,1080,1,.2,50);}
 $.evalFile(new File(root+'/scripts/ae_render_frame.jsx'));
 function save(c,name){var file=MGN_renderFrame(c,0,name,say);say('Rendered '+name);return file;}
 function exposure(layer){var fx=layer.property('ADBE Effect Parade').addProperty('ADBE Exposure2');var p=fx.property('Exposure');if(!p)throw new Error('Exposure control unavailable; inspect host effect properties.');p.setValue(1);return fx;}
 try {
  if(!app.project.file || app.project.file.fsName.replace(/\\/g,'/').indexOf(root+'/tests/host-projects/')!==0)throw new Error('Open only our disposable AE test project first.');
  app.project.bitsPerChannel=32;app.project.gpuAccelType=GpuAccelType.CUDA;
  var src=app.project.importFile(new ImportOptions(new File(root+'/tests/outputs/av_9232-709.png')));
  say('Working space: '+app.project.workingSpace+'; linearized: '+app.project.linearizeWorkingSpace);
  var plain=comp('No effect baseline');plain.layers.add(src);save(plain,'ae-source-baseline');
  var graded=comp('Reference upstream composite');exposure(graded.layers.add(src));var upstreamFile=save(graded,'ae-upstream-composite');
  var nested=comp('Nested upstream without texture');nested.layers.add(graded);save(nested,'ae-nested-baseline');
  var ordered=comp('Exposure then texture on clip');var l=ordered.layers.add(src);exposure(l);texture(l);save(ordered,'ae-grade-then-texture');
  var adj=comp('Texture on adjustment layer');adj.layers.add(graded);var al=adj.layers.addSolid([1,1,1],'Texture adjustment',1920,1080,1);al.adjustmentLayer=true;texture(al);save(adj,'ae-adjustment-texture');
  var reversed=comp('Texture then exposure');l=reversed.layers.add(src);texture(l);exposure(l);save(reversed,'ae-texture-then-grade');
  var baked=app.project.importFile(new ImportOptions(upstreamFile));
  var pre=comp('Texture on upstream prerender');texture(pre.layers.add(baked));save(pre,'ae-prerender-texture');
  // Change upstream exposure after caching: no stale extraction should survive.
  l=ordered.layer(1);l.property('ADBE Effect Parade').property(1).property('Exposure').setValue(-2);save(ordered,'ae-upstream-changed');
  var bypass=comp('Bypass');var bl=bypass.layers.add(src),fx=texture(bl);fx.property('Halation amount').setValue(0);fx.property('Bloom amount').setValue(0);save(bypass,'ae-bypass');
  var ab=comp('Adjustment bypass on upstream');ab.layers.add(graded);var abl=ab.layers.addSolid([1,1,1],'Bypass adjustment',1920,1080,1);abl.adjustmentLayer=true;var abf=texture(abl);abf.property('Halation amount').setValue(0);abf.property('Bloom amount').setValue(0);save(ab,'ae-adjustment-bypass');
  app.project.save(new File(root+'/tests/host-projects/ae-stacking.aep'));say('Saved own project');
 }catch(e){say('ERROR: '+e.toString()+' line '+e.line);}log.close();
})();
