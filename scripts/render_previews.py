from pathlib import Path
import subprocess,json
import numpy as np
from PIL import Image,ImageDraw,ImageFont
from core_tests import ROOT,Params,render
OUT=ROOT/'tests/outputs';OUT.mkdir(exist_ok=True)
lut=ROOT/'.deps/slog3_sgamut3_cine_to_rec709.cube'
presets=[('Fine & gentle',1.2,.8,.08,8,4,6,24),('Soft color',2.5,1.2,.35,15,8,8,32),('Coarse mono',4,2.6,0,10,3,7,24),('Warm lights',2,1,.15,32,12,12,40),('Dream bloom',1.8,1.3,.2,12,35,10,64),('Gritty color',5,2,.5,18,5,8,28)]
def sony_frame(clip,w=1920,h=1080,time=2):
    # LUT paths in FFmpeg filters need escaped drive separator; use relative path with cwd fixed.
    cmd=['C:/ffmpeg/bin/ffmpeg.exe','-v','error','-ss',str(time),'-i',str(clip),'-vf',f"lut3d=file='.deps/slog3_sgamut3_cine_to_rec709.cube':interp=tetrahedral,scale={w}:{h}",'-frames:v','1','-f','rawvideo','-pix_fmt','gbrpf32le','pipe:1']
    raw=subprocess.run(cmd,cwd=ROOT,check=True,stdout=subprocess.PIPE).stdout
    planes=np.frombuffer(raw,np.float32).reshape(3,h,w)
    rgba=np.ones((h,w,4),np.float32);rgba[:,:,0]=planes[2];rgba[:,:,1]=planes[0];rgba[:,:,2]=planes[1];return rgba
def image(a):return Image.fromarray((np.clip(a[:,:,:3],0,1)*255+.5).astype('uint8'))
def main():
    report=[]
    for clip in sorted((ROOT/'tests/fixtures/input').glob('*.MP4')):
        a=sony_frame(clip);np.save(OUT/f'{clip.stem}-709.npy',a);image(a).save(OUT/f'{clip.stem}-709.png')
        tiles=[('Sony LUT / no texture',image(a))]
        for name,grain,size,color,hal,bloom,hr,br in presets:
            p=Params(grain=grain*.01,size=size,color=color,halation=hal*.01,bloom=bloom*.01,halRadius=hr,bloomRadius=br,scale=1)
            b,ms=render(a,p);im=image(b);im.save(OUT/f'{clip.stem}-{name.replace(" ","-").replace("&","and")}.png');tiles.append((name,im));report.append(dict(clip=clip.name,preset=name,kernel_ms=ms))
        canvas=Image.new('RGB',(1440,4*430),(22,22,22));d=ImageDraw.Draw(canvas)
        for i,(title,im) in enumerate(tiles):
            x=(i%2)*720;y=(i//2)*430;canvas.paste(im.resize((720,405)),(x,y+25));d.text((x+10,y+6),title,fill='white')
        canvas.save(OUT/f'{clip.stem}-preset-sheet.jpg',quality=92)
    # Full-resolution crop makes grain differences easier to see than scaled contact sheets.
    a=np.load(OUT/'av_9228-709.npy');canvas=Image.new('RGB',(1200,700),(20,20,20));d=ImageDraw.Draw(canvas)
    for i,(name,grain,size,color,hal,bloom,hr,br) in enumerate(presets):
        b,_=render(a,Params(grain=grain*.01,size=size,color=color,halation=hal*.01,bloom=bloom*.01,halRadius=hr,bloomRadius=br));im=image(b).crop((720,280,1120,605));x=(i%3)*400;y=(i//3)*350;canvas.paste(im,(x,y+25));d.text((x+8,y+6),name,fill='white')
    canvas.save(OUT/'grain-crops.png')
    (OUT/'preview-report.json').write_text(json.dumps({'conversion':'User supplied Sony S-Log3 S-Gamut3.Cine -> Rec.709 cube, FFmpeg tetrahedral interpolation','frames':report},indent=2))
    print('Saved four preset sheets and grain-crops.png')
if __name__=='__main__':main()
