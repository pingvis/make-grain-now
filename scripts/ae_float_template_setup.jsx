// Minimal disposable project: inspect output templates without loading our effect.
(function(){
 var root='E:/Custom Apps/make-grain-now-premiere';
 if(app.project.numItems)throw new Error('Start with an empty project.');
 app.project.bitsPerChannel=32;
 var c=app.project.items.addComp('Our float template setup',1920,1080,1,.02,50);
 c.layers.addSolid([.5,.25,.1],'Our test solid',1920,1080,1);
 var q=app.project.renderQueue.items.add(c);q.timeSpanStart=0;q.timeSpanDuration=.02;
 var names=q.outputModule(1).templates;
 var f=new File(root+'/tests/outputs/ae-template-details.txt');f.encoding='UTF-8';f.open('w');
 for(var i=0;i<names.length;i++){
  try{q.outputModule(1).applyTemplate(names[i]);f.writeln(names[i]+' => '+q.outputModule(1).getSettings(GetSettingsFormat.STRING).toSource());}catch(e){f.writeln(names[i]+' => ERROR '+e);}
 }
 f.close();q.outputModule(1).applyTemplate('Lossless');
 app.project.save(new File(root+'/tests/host-projects/ae-float-template.aep'));
 app.project.renderQueue.showWindow(true);
})();
