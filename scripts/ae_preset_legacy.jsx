// Create only our own disposable legacy project while the previous plugin is installed.
(function(){
 var root='E:/Custom Apps/make-grain-now-premiere';
 var log=new File(root+'/tests/outputs/ae-preset-legacy.txt');log.encoding='UTF-8';log.open('w');
 function say(s){log.writeln(s);log.close();log.open('a');}
 try{
  if(app.project.file)throw new Error('Requires an empty unsaved project.');
  if(app.project.numItems!==0){
   var own=false;for(var i=1;i<=app.project.numItems;i++)if(app.project.item(i) instanceof CompItem&&app.project.item(i).name==='Legacy fast'&&app.project.item(i).layer(1).name==='Our legacy texture')own=true;
   if(!own)throw new Error('Refusing to replace an unrelated project.');
   app.project.close(CloseOptions.DO_NOT_SAVE_CHANGES);app.newProject();
  }
  $.evalFile(new File(root+'/scripts/ae_render_frame.jsx'));
  app.project.bitsPerChannel=32;app.project.gpuAccelType=GpuAccelType.CUDA;
  var c=app.project.items.addComp('Legacy fast',640,360,1,.2,50);
  var l=c.layers.addSolid([.3,.4,.5],'Legacy source',640,360,1);
  c.layers.addSolid([1,.6,.2],'Bright detail',80,80,1).property('ADBE Transform Group').property('ADBE Position').setValue([180,120]);
  var adj=c.layers.addSolid([1,1,1],'Our legacy texture',640,360,1);adj.adjustmentLayer=true;
  var f=adj.property('ADBE Effect Parade').addProperty('MGN Film Texture');
  var values={'Grain amount':3.1,'Grain size':2.3,'Grain color':23,'Halation amount':27,'Halation radius':11,'Bloom amount':13,'Bloom radius':37,'Highlight threshold':83,'Highlight softness':19,'Grain in shadows':97,'Grain in highlights':21,'Seed':9137,'Input encoding':1,'Grain renderer':1};
  for(var n in values){if(!f.property(n))throw new Error('Missing old property '+n);f.property(n).setValue(values[n]);}
  f.property('Grain amount').setValueAtTime(0,3.1);f.property('Grain amount').setValueAtTime(.02,4.7);
  var particle=c.duplicate();particle.name='Legacy simulated';particle.layer(1).property('ADBE Effect Parade').property(1).property('Grain renderer').setValue(2);
  app.project.save(new File(root+'/tests/host-projects/ae-preset-legacy.aep'));
  MGN_renderFrame(c,0,'ae-preset-old-fast',say,'_HIDDEN X-Factor 32');
  MGN_renderFrame(c,.02,'ae-preset-old-fast-next',say,'_HIDDEN X-Factor 32');
  MGN_renderFrame(particle,0,'ae-preset-old-simulated',say,'_HIDDEN X-Factor 32');
  app.project.save(app.project.file);say('PASS: saved legacy project and three actual float exports.');
  log.close();app.quit();
 }catch(e){say('ERROR: '+e.toString()+' line '+e.line);log.close();}
})();
