"""Prepare eight current user bat poses using geometry only; preserve originals.
Device-sized poses only translate. High-res poses use nearest-neighbour scaling,
head/sole landmarks and established floor cleanup. No body splices or AI edits.
"""
from pathlib import Path
import argparse
import hashlib
import json
from PIL import Image, ImageDraw


ROOT=Path(__file__).resolve().parents[1]
DEVICE=ROOT/'assets/images/stelle-companion/device'
SOURCE=DEVICE/'frames/bat_swing'
OUTPUT=DEVICE/'prepared/bat_swing'
SHADOW=(16,17,24)
BAT_DURATIONS=[100]*8

def character(image):
    out=image.convert('RGBA')
    out.putdata([(0,0,0,0) if p[:3]==SHADOW or (p[3]<230 and max(p[:3])<80) or p[3]<16 else p
                 for p in out.get_flattened_data()])
    return out

def soles(image):
    points=[(i%image.width,i//image.width) for i,p in enumerate(image.get_flattened_data()) if p[3]>=230]
    bottom=max(y for x,y in points)
    row=[x for x,y in points if y>=bottom-3]
    return (min(row)+max(row))/2,bottom,(min(row),max(row))

def hair_top(image):
    w,h=image.size
    return min(i//w for i,(r,g,b,a) in enumerate(image.get_flattened_data())
        if a>=230 and .25*w<i%w<.85*w and i//w<.65*h
        and 65<=min(r,g,b)<=max(r,g,b)<=235 and max(r,g,b)-min(r,g,b)<65)

def prepare_bat(check=False):
    paths=sorted(SOURCE.glob('*.png'))
    if [p.name for p in paths]!=[f'{i:02d}.png' for i in range(8)]:
        raise ValueError('Expected eight user poses 00.png through 07.png')
    frames=[]; report=[]
    for path in paths:
        raw=Image.open(path).convert('RGBA'); high=raw.size!=(160,160)
        char=character(raw) if high else raw.copy(); scale=1.0
        if high:
            _,bottom,_=soles(char); scale=122/(bottom-hair_top(char))
            char=char.resize((round(raw.width*scale),round(raw.height*scale)),Image.Resampling.NEAREST)
        fx,bottom,span=soles(character(char)); dx,dy=round(80-fx),153-bottom
        out=Image.new('RGBA',(160,160))
        if high:
            ImageDraw.Draw(out).ellipse((36,145,124,153),fill=(*SHADOW,190))
            out.alpha_composite(char,(dx,dy))
        else: out.paste(char,(dx,dy))
        feet=soles(character(out)); top=hair_top(out)
        if abs(feet[0]-80)>1 or feet[1]!=153 or not 120<=153-top<=125:
            raise ValueError(f'{path.name}: inconsistent support/body {feet}, top={top}')
        frames.append(out)
        report.append(dict(frame=path.name,source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
            source_size=list(raw.size),scale=scale,translation=[dx,dy],feet=list(feet[:2]),
            sole_span=list(feet[2]),hair_top=top,bounds=out.getchannel('A').getbbox()))
    if not check: OUTPUT.mkdir(parents=True,exist_ok=True)
    for i,out in enumerate(frames):
        dest=OUTPUT/f'{i:02d}.png'
        if check:
            if not dest.exists() or Image.open(dest).convert('RGBA').tobytes()!=out.tobytes():
                raise ValueError(f'Stale prepared pose {dest}')
        else: out.save(dest)
    state=dict(name='bat_swing',frames=[f'prepared/bat_swing/{i:02d}.png' for i in range(8)],durations_ms=BAT_DURATIONS)
    if not check:
        mp=DEVICE/'sprites.json'; m=json.loads(mp.read_text());m['states']=[state if x['name']=='bat_swing' else x for x in m['states']]
        mp.write_text(json.dumps(m,indent=2)+'\n')
        (OUTPUT/'conversion.json').write_text(json.dumps(dict(operation='geometry only; no pose replacement or AI edits',durations_ms=BAT_DURATIONS,frames=report),indent=2)+'\n')
    return state

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true')
    args=p.parse_args();print(json.dumps(prepare_bat(args.check)))
