// Prepare only our disposable project for authoring a float-output test template.
(function(){
 var root='E:/Custom Apps/make-grain-now-premiere';
 if(app.project&&app.project.numItems>0)throw new Error('Use an empty project for float test setup.');
 app.open(new File(root+'/tests/host-projects/ae-stacking.aep'));
 var c=null;for(var i=1;i<=app.project.numItems;i++)if(app.project.item(i).name==='Reference upstream composite')c=app.project.item(i);
 if(!c)throw new Error('Own reference comp missing');
 var q=app.project.renderQueue.items.add(c);q.timeSpanStart=0;q.timeSpanDuration=c.frameDuration;
 q.outputModule(1).file=new File(root+'/tests/outputs/ae-float-template-[#####].exr');
 var f=new File(root+'/tests/outputs/ae-output-templates.txt');f.encoding='UTF-8';f.open('w');f.writeln(q.outputModule(1).templates.join('\n'));f.close();
 c.openInViewer();app.project.save(new File(root+'/tests/host-projects/ae-float-template.aep'));
 app.project.renderQueue.showWindow(true);
})();
