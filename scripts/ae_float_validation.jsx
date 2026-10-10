// Run through File > Scripts from our setup project; uses built-in float PSD if EXR is unavailable.
(function(){
 var root='E:/Custom Apps/make-grain-now-premiere',template='MGN Float Test';
 var log=new File(root+'/tests/outputs/ae-float.txt');if(log.exists)log.rename('previous-'+new Date().getTime()+'-ae-float.txt');log=new File(root+'/tests/outputs/ae-float.txt');log.encoding='UTF-8';log.open('w');
 function say(s){log.writeln(s);log.close();log.open('a');}
 function comp(n){return app.project.items.addComp('Float / '+n,1920,1080,1,.2,50);}
 function texture(l,zero){var f=l.property('ADBE Effect Parade').addProperty('MGN Film Texture');f.property('Grain amount').setValue(zero?0:2.5);f.property('Halation amount').setValue(zero?0:40);f.property('Bloom amount').setValue(zero?0:20);return f;}
 function exposure(l){var f=l.property('ADBE Effect Parade').addProperty('ADBE Exposure2');f.property('Exposure').setValue(1);return f;}
 $.evalFile(new File(root+'/scripts/ae_render_frame.jsx'));
 function render(c,n,t){return MGN_renderFrame(c,t||0,'ae-float-'+n,say,template);}
 try{
  if(!app.project.file||!/^ae-float(-template)?\.aep$/.test(app.project.file.name)||app.project.file.fsName.replace(/\\/g,'/').indexOf(root+'/tests/host-projects/')!==0)throw new Error('Open only our saved float test or setup project.');
  var previous=app.project.renderQueue.item(app.project.renderQueue.numItems);
  var q=app.project.renderQueue.items.add(previous.comp),om=q.outputModule(1);
  var settings=om.getSettings(GetSettingsFormat.STRING);
  if(/OpenEXR/i.test(settings.Format))om.saveAsTemplate(template);
  else{template='_HIDDEN X-Factor 32';om.applyTemplate(template);settings=q.outputModule(1).getSettings(GetSettingsFormat.STRING);}
  if(!/Floating Point/.test(settings.Depth))throw new Error('Output template is not floating point: '+settings.Depth);
  say('Output settings: '+settings.toSource());
  app.project.bitsPerChannel=32;app.project.gpuAccelType=GpuAccelType.CUDA;
  say('Working space: '+app.project.workingSpace+'; linearized: '+app.project.linearizeWorkingSpace);
  app.project.save(new File(root+'/tests/host-projects/ae-float.aep'));
  var src=app.project.importFile(new ImportOptions(new File(root+'/tests/outputs/av_9232-709.png')));
  var plain=comp('source');plain.layers.add(src);render(plain,'source');
  var bypass=comp('bypass');texture(bypass.layers.add(src),true);render(bypass,'bypass');
  var upstream=comp('upstream');exposure(upstream.layers.add(src));var upFile=render(upstream,'upstream');
  var nested=comp('nested');nested.layers.add(upstream);render(nested,'nested');
  var clip=comp('clip'),l=clip.layers.add(src);var grade=exposure(l);texture(l,false);render(clip,'clip');render(clip,'repeat');render(clip,'next-frame',.02);
  var adj=comp('adjustment');adj.layers.add(upstream);var a=adj.layers.addSolid([1,1,1],'Our texture adjustment',1920,1080,1);a.adjustmentLayer=true;texture(a,false);render(adj,'adjustment');
  var ab=comp('adjustment bypass');ab.layers.add(upstream);a=ab.layers.addSolid([1,1,1],'Our bypass adjustment',1920,1080,1);a.adjustmentLayer=true;texture(a,true);render(ab,'adjustment-bypass');
  var imported=app.project.importFile(new ImportOptions(upFile));var pre=comp('prerender');texture(pre.layers.add(imported),false);render(pre,'prerender');
  app.project.gpuAccelType=GpuAccelType.SOFTWARE;render(clip,'cpu-visible');app.project.gpuAccelType=GpuAccelType.CUDA;
  var transparent=comp('alpha upstream');var sl=transparent.layers.addSolid([.9,.5,.2],'Our partial-alpha solid',960,1080,1);sl.property('ADBE Transform Group').property('ADBE Opacity').setValue(35);
  var alphaBase=comp('alpha baseline');alphaBase.layers.add(transparent);render(alphaBase,'alpha-source');
  var alphaFx=comp('alpha effect');texture(alphaFx.layers.add(transparent),false);render(alphaFx,'alpha-effect');
  // AE recreates indexed effect groups when addProperty adds the texture effect.
  clip.layer(1).property('ADBE Effect Parade').property(1).property('Exposure').setValue(-2);render(clip,'changed');
  app.project.save(app.project.file);say('Saved own float project');
 }catch(e){say('ERROR: '+e.toString()+' line '+e.line);}log.close();
})();
