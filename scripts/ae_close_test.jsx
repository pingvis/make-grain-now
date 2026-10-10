(function(){
 var root='E:/Custom Apps/make-grain-now-premiere';
 // This script may close only a saved project in our own fixture directory, or the known failed smoke project.
 var own=app.project&&app.project.file&&app.project.file.fsName.replace(/\\/g,'/').indexOf(root+'/tests/host-projects/')===0;
 var failedSmoke=app.project&&app.project.numItems===2&&app.project.item(2).name==='MGN smoke / Sony LUT baked preview';
 if(!own&&!failedSmoke)throw new Error('Refusing to close a project outside our tests.');
 if(failedSmoke)app.project.save(new File(root+'/tests/host-projects/ae-initial-load-failure.aep'));
 else app.project.save(app.project.file);
 app.quit();
})();
