// Single disposable validation run; the launcher can wait for AE to finish.
(function(){
 var root='E:/Custom Apps/make-grain-now-premiere';
 if(app.project&&app.project.numItems>0)throw new Error('Refusing to run validation in a nonempty project.');
 function ownProject(){return app.project&&app.project.file&&app.project.file.fsName.replace(/\\/g,'/').indexOf(root+'/tests/host-projects/')===0;}
 function read(path){var f=new File(path);if(!f.exists||!f.open('r'))return '';var s=f.read();f.close();return s;}
 $.evalFile(new File(root+'/scripts/ae_smoke.jsx'));
 var smoke=read(root+'/tests/outputs/ae-smoke.txt');
 if(smoke.indexOf('Native render completed')>=0&&smoke.indexOf('Frame 1 completed')>=0&&ownProject()){
  $.evalFile(new File(root+'/scripts/ae_stacking.jsx'));
 }
 // Preserve failure evidence in our fixture project as well as successful runs.
 if(!ownProject()&&app.project&&app.project.numItems===2&&app.project.item(2).name==='MGN smoke / Sony LUT baked preview'){
  app.project.save(new File(root+'/tests/host-projects/ae-validation-failure.aep'));
 }
 if(ownProject()){app.project.save(app.project.file);app.quit();}
})();
