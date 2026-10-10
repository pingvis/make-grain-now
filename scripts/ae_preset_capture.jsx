// Snapshot only our disposable UI test; stop periodic capture to avoid disturbing host menus.
(function(){
 if($.global.MGN_presetCaptureTask)app.cancelTask($.global.MGN_presetCaptureTask);
 if(!app.project.file||app.project.file.name!=='ae-presets.aep'||app.project.file.fsName.replace(/\\/g,'/').indexOf('E:/Custom Apps/make-grain-now-premiere/tests/host-projects/')!==0)throw new Error('Requires our preset test project.');
 $.global.MGN_presetCaptureCount=0;if($.global.MGN_presetCapture)$.global.MGN_presetCapture();
})();
