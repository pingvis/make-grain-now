// Verify persistence of our current interactive state, without touching other projects.
(function(){
 var root='E:/Custom Apps/make-grain-now-premiere',path=root+'/tests/host-projects/ae-presets.aep';
 if(!app.project.file||app.project.file.fsName.replace(/\\/g,'/')!==path)throw new Error('Requires our preset test project.');
 if($.global.MGN_presetCaptureTask)app.cancelTask($.global.MGN_presetCaptureTask);
 function instance(){for(var i=1;i<=app.project.numItems;i++)if(app.project.item(i).name==='Preset UI test')return app.project.item(i);throw new Error('Missing test comp.');}
 function state(){var f=instance().layer(1).property('ADBE Effect Parade').property(1),v=[];for(var j=1;j<=f.numProperties;j++)try{if(typeof f.property(j).value==='number')v.push(f.property(j).name+'='+f.property(j).value);}catch(ignored){}return v.join('\n');}
 var before=state();app.project.save(app.project.file);app.open(new File(path));var after=state();
 var log=new File(root+'/tests/outputs/ae-preset-reopen.txt');log.open('w');log.writeln(before===after?'PASS: all numeric UI/preset/model/strength/lock/seed settings survive actual save and reopen.':'FAIL: settings changed');log.close();
 if(before!==after)throw new Error('Persistence mismatch.');
 var c=instance();c.openInViewer();c.layer(1).selected=true;c.layer(1).property('ADBE Effect Parade').property(1).selected=true;
})();
