// Supported render-queue export; only called by our disposable host tests.
function MGN_renderFrame(comp,time,base,say,requestedTemplate){
 var root='E:/Custom Apps/make-grain-now-premiere';
 if(!app.project.file||app.project.file.fsName.replace(/\\/g,'/').indexOf(root+'/tests/host-projects/')!==0)throw new Error('Frame export requires our saved disposable project.');
 var queue=app.project.renderQueue;
 for(var i=1;i<=queue.numItems;i++)if(queue.item(i).status===RQItemStatus.QUEUED)queue.item(i).render=false;
 var item=queue.items.add(comp);item.timeSpanStart=time;item.timeSpanDuration=comp.frameDuration;
 var om=item.outputModule(1),templates=om.templates,chosen='';
 // Lossless AVI is confirmed working on this machine; image sequences require explicit frame tokens.
 if(requestedTemplate){for(var j=0;j<templates.length;j++)if(templates[j]===requestedTemplate){chosen=templates[j];break;}if(!chosen)throw new Error('Requested template missing: '+requestedTemplate);}
 if(!chosen)for(var j=0;j<templates.length;j++)if(templates[j]==='Lossless'){chosen=templates[j];break;}
 if(!chosen)for(var j=0;j<templates.length;j++)if(/PNG/i.test(templates[j])){chosen=templates[j];break;}
 if(!chosen)for(var j=0;j<templates.length;j++)if(/TIFF/i.test(templates[j])){chosen=templates[j];break;}
 if(!chosen)for(var j=0;j<templates.length;j++)if(templates[j]==='Lossless'){chosen=templates[j];break;}
 if(!chosen)throw new Error('No lossless output template: '+templates.join(' | '));
 om.applyTemplate(chosen);om=item.outputModule(1);
 var format=om.getSettings(GetSettingsFormat.STRING).Format,ext;
 if(/OpenEXR/i.test(format))ext='.exr';else if(/Photoshop/i.test(format))ext='.psd';else if(/PNG/i.test(format))ext='.png';else if(/TIFF/i.test(format))ext='.tiff';else if(/AVI/i.test(format))ext='.avi';else if(/QuickTime/i.test(format))ext='.mov';else throw new Error('Unsupported test output format: '+format);
 if(requestedTemplate&&ext!=='.exr'&&ext!=='.psd')throw new Error('Precision template must export EXR or float PSD; found '+format);
 var folder=new Folder(root+'/tests/outputs');
 // Preserve only this test's previous export before rerunning, avoiding overwrite dialogs.
 var previous=folder.getFiles(function(f){return f instanceof File&&f.name.indexOf(base)===0&&f.name.slice(-ext.length).toLowerCase()===ext;});
 for(var k=0;k<previous.length;k++)if(!previous[k].rename('previous-'+new Date().getTime()+'-'+previous[k].name))throw new Error('Cannot preserve previous test export '+base);
 om.file=new File(root+'/tests/outputs/'+base+(/Sequence/i.test(format)?'-[#####]':'')+ext);om.includeSourceXMP=false;
 say('Rendering '+base+' using '+chosen+' / '+format);
 queue.render();
 if(item.status!==RQItemStatus.DONE)throw new Error('Render queue did not finish '+base+'; status '+item.status);
 var files=new Folder(root+'/tests/outputs').getFiles(function(f){return f instanceof File&&f.name.indexOf(base)===0&&f.name.slice(-ext.length).toLowerCase()===ext;});
 if(files.length!==1||files[0].length===0)throw new Error('Expected one nonempty render for '+base+'; found '+files.length);
 say('Exported '+files[0].name+' ('+files[0].length+' bytes)');
 return files[0];
}
