// Run with the updated plugin and an empty project. Uses synthetic media only.
(function(){
 var root='E:/Custom Apps/make-grain-now-premiere';
 var log=new File(root+'/tests/outputs/ae-preset-validation.txt');log.encoding='UTF-8';log.open('w');
 function say(s){log.writeln(s);log.close();log.open('a');}
 function check(f,n,v,t){var a=f.property(n).valueAtTime(t||0,false);if(Math.abs(a-v)>.0001)throw new Error(n+': '+a+' expected '+v);}
 try{
  if(app.project.numItems!==0||app.project.file)throw new Error('Requires an empty unsaved project.');
  $.evalFile(new File(root+'/scripts/ae_render_frame.jsx'));
  app.open(new File(root+'/tests/host-projects/ae-preset-legacy.aep'));
  var fast=null,sim=null;for(var i=1;i<=app.project.numItems;i++){var item=app.project.item(i);if(item.name==='Legacy fast')fast=item;if(item.name==='Legacy simulated')sim=item;}
  if(!fast||!sim)throw new Error('Legacy comps missing.');
  var expected={'Grain amount':3.1,'Grain size':2.3,'Grain color':23,'Halation amount':27,'Halation radius':11,'Bloom amount':13,'Bloom radius':37,'Highlight threshold':83,'Highlight softness':19,'Grain in shadows':97,'Grain in highlights':21,'Grain seed':9137,'Input encoding':1,'Overall strength':100};
  for(var name in expected){check(fast.layer(1).property('ADBE Effect Parade').property(1),name,expected[name]);check(sim.layer(1).property('ADBE Effect Parade').property(1),name,expected[name]);}
  var oldFast=fast.layer(1).property('ADBE Effect Parade').property(1),oldSim=sim.layer(1).property('ADBE Effect Parade').property(1);
  check(oldFast,'Grain type',1);check(oldSim,'Grain type',2);check(oldFast,'Grain amount',4.7,.02);
  if(oldFast.property('Grain amount').numKeys!==2)throw new Error('Old animation lost.');
  say('PASS: actual legacy migration preserves both models, 11 style values, encoding, seed, two animation keys and neutral strength.');
  MGN_renderFrame(fast,0,'ae-preset-new-fast',say,'_HIDDEN X-Factor 32');
  MGN_renderFrame(fast,.02,'ae-preset-new-fast-next',say,'_HIDDEN X-Factor 32');
  MGN_renderFrame(sim,0,'ae-preset-new-simulated',say,'_HIDDEN X-Factor 32');
  var c=fast.duplicate();c.name='Preset UI test';c.layer(1).property('ADBE Effect Parade').property(1).remove();
  var source=c.duplicate();source.name='Preset strength source';
  var f=c.layer(1).property('ADBE Effect Parade').addProperty('MGN Film Texture');
  check(f,'Grain type',2);check(f,'Overall strength',100);check(f,'Look',2);
  say('PASS: new Adobe instance defaults to Simulated (slow), Soft color and strength 100.');
  app.project.save(new File(root+'/tests/host-projects/ae-presets.aep'));
  MGN_renderFrame(source,0,'ae-preset-strength-source',say,'_HIDDEN X-Factor 32');
  MGN_renderFrame(c,0,'ae-preset-strength-100',say,'_HIDDEN X-Factor 32');
  f.property('Overall strength').setValue(50);MGN_renderFrame(c,0,'ae-preset-strength-50',say,'_HIDDEN X-Factor 32');
  f.property('Overall strength').setValue(0);MGN_renderFrame(c,0,'ae-preset-strength-0',say,'_HIDDEN X-Factor 32');
  f.property('Overall strength').setValue(100);
  app.project.save(app.project.file);c.openInViewer();c.layer(1).selected=true;f.selected=true;
  say('PASS: actual host renders complete; project ready for UI interaction.');
 }catch(e){say('ERROR: '+e.toString()+' line '+e.line);}
 log.close();
})();
// Read only our test instance on demand through ae_preset_capture.jsx.
if(app.project.file&&app.project.file.name==='ae-presets.aep'){
 $.global.MGN_presetCaptureCount=0;
 $.global.MGN_presetCapture=function(){
  try{
   if(++$.global.MGN_presetCaptureCount>900){app.cancelTask($.global.MGN_presetCaptureTask);return;}
   if(!app.project.file||app.project.file.name!=='ae-presets.aep')return;
   var c=null;for(var i=1;i<=app.project.numItems;i++)if(app.project.item(i).name==='Preset UI test')c=app.project.item(i);
   if(!c)return;var f=c.layer(1).property('ADBE Effect Parade').property(1),pairs=[];
   for(var j=1;j<=f.numProperties;j++){var p=f.property(j);try{if(typeof p.value==='number')pairs.push('"'+p.name+'":'+p.value);}catch(ignored){}}
   var state='{'+pairs.join(',')+'}',path='E:/Custom Apps/make-grain-now-premiere/tests/outputs/ae-preset-ui-state.json';
   var out=new File(path);out.open('w');out.write(state);out.close();
   if(state!==$.global.MGN_presetLastState){var history=new File(path.replace('state.json','history.txt'));history.open('a');history.writeln(new Date().getTime()+' '+state);history.close();$.global.MGN_presetLastState=state;}
  }catch(ignored){}
 };
 // Do not poll: scheduled scripts can disturb popup menus and modal file pickers.
 $.global.MGN_presetCapture();
}
