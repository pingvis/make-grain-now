"""Short 50 fps comparison on a held Sony-LUT frame: isolates temporal grain."""
import subprocess
import numpy as np
from PIL import Image,ImageDraw
from core_tests import ROOT,Params,render
def main():
    a=np.load(ROOT/'tests/outputs/av_9232-709.npy');a=a[::2,::2].copy()
    out=ROOT/'tests/outputs/animated-grain-comparison.mp4'
    enc=subprocess.Popen(['C:/ffmpeg/bin/ffmpeg.exe','-v','error','-y','-f','rawvideo','-pix_fmt','rgb24','-s','1920x570','-r','50','-i','pipe:0','-an','-c:v','libx264','-crf','16','-preset','fast','-pix_fmt','yuv420p',str(out)],stdin=subprocess.PIPE)
    try:
        for f in range(100):
            b,_=render(a,Params(grain=.04,size=2.6,color=0,halation=.1,bloom=.03,scale=.5,frame=f))
            c=Image.new('RGB',(1920,570),(20,20,20));d=ImageDraw.Draw(c)
            c.paste(Image.fromarray((np.clip(a[:,:,:3],0,1)*255+.5).astype('uint8')),(0,30));c.paste(Image.fromarray((np.clip(b[:,:,:3],0,1)*255+.5).astype('uint8')),(960,30))
            d.text((15,8),'Sony LUT / held frame',fill='white');d.text((975,8),'Coarse monochrome / evolving frame seed / 50 fps',fill='white')
            enc.stdin.write(np.asarray(c).tobytes())
    finally:enc.stdin.close()
    if enc.wait():raise RuntimeError('Preview encoding failed')
    print(out)
if __name__=='__main__':main()
