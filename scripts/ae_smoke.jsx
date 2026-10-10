(function(){
 var root = 'E:/Custom Apps/make-grain-now-premiere';
 var log = new File(root+'/tests/outputs/ae-smoke.txt');log.encoding='UTF-8';log.open('w');
 function say(s){log.writeln(s);log.close();log.open('a');}
 try {
  if(app.project && app.project.numItems>0) throw new Error('Refusing to replace a project with existing items.');
  if(!app.project)app.newProject();app.project.bitsPerChannel=32;
  try{app.project.gpuAccelType=GpuAccelType.CUDA;say('CUDA project selected');}catch(e){say('GPU selection: '+e.toString());}
  var footage=app.project.importFile(new ImportOptions(new File(root+'/tests/outputs/av_9228-709.png')));
  var comp=app.project.items.addComp('MGN smoke / Sony LUT baked preview',1920,1080,1,1,50);
  var layer=comp.layers.add(footage);
  var fx=layer.property('ADBE Effect Parade').addProperty('MGN Film Texture');
  if(!fx)throw new Error('Native effect unavailable');say('Effect loaded: '+fx.name);
  for(var i=1;i<=fx.numProperties;i++){var p=fx.property(i);var readable=p.propertyType===PropertyType.PROPERTY&&p.propertyValueType!==PropertyValueType.NO_VALUE;say(i+' '+p.name+' '+p.matchName+' '+(readable?p.value:'no value / group'));}
  app.project.save(new File(root+'/tests/host-projects/ae-smoke.aep'));
  $.evalFile(new File(root+'/scripts/ae_render_frame.jsx'));
  comp.openInViewer();comp.time=0;
  MGN_renderFrame(comp,0,'ae-smoke-frame-0',say);say('Native render completed');
  MGN_renderFrame(comp,.02,'ae-smoke-frame-1',say);say('Frame 1 completed');
  app.project.save(new File(root+'/tests/host-projects/ae-smoke.aep'));
 } catch(e){say('ERROR: '+e.toString()+' line '+e.line);}
 log.close();
})();
