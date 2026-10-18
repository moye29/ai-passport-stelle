"""Copy eight current device-ready sleep frames without changing pixels."""
from pathlib import Path
import argparse, hashlib, json, shutil
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
DEVICE=ROOT/'assets/images/stelle-companion/device'
SOURCE=DEVICE/'frames/sleep'
OUTPUT=DEVICE/'prepared/sleep'
SLEEP_DURATIONS=[3000]+[200]*7
def prepare_sleep(check=False):
    paths=sorted(SOURCE.glob('*.png'))
    if [p.name for p in paths]!=[f'{i:02d}.png' for i in range(8)]: raise ValueError('Expected eight sleep poses')
    if not check: OUTPUT.mkdir(parents=True,exist_ok=True)
    for p in paths:
        im=Image.open(p)
        if im.size!=(160,160) or im.mode!='RGBA': raise ValueError(f'{p}: expected 160px RGBA')
        dest=OUTPUT/p.name
        if check:
            if not dest.exists() or dest.read_bytes()!=p.read_bytes(): raise ValueError(f'Stale prepared frame {dest}')
        else: shutil.copy2(p,dest)
    state=dict(name='sleep',frames=[f'prepared/sleep/{p.name}' for p in paths],durations_ms=SLEEP_DURATIONS)
    if not check:
        mp=DEVICE/'sprites.json';m=json.loads(mp.read_text());m['states']=[state if x['name']=='sleep' else x for x in m['states']]
        mp.write_text(json.dumps(m,indent=2)+'\n')
        (OUTPUT/'conversion.json').write_text(json.dumps(dict(operation='byte-identical copy',source_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},durations_ms=SLEEP_DURATIONS),indent=2)+'\n')
    return state
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true')
    args=p.parse_args();print(json.dumps(prepare_sleep(args.check)))
