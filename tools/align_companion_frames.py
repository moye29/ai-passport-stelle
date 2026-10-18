"""Integrate current user frames without overwriting any original artwork.
Only bat high-resolution conversion and geometric registration are applied.
"""
from pathlib import Path
import json
from prepare_bat_frames import BAT_DURATIONS,prepare_bat
from prepare_sleep_frames import SLEEP_DURATIONS,prepare_sleep
ROOT=Path(__file__).resolve().parents[1]
DEVICE=ROOT/'assets/images/stelle-companion/device'
DURATIONS={'idle':[3000]+[200]*4,'greeting':[200]*5,'focus':[200]*9,
           'celebration':[200]*5,'bat_swing':BAT_DURATIONS,'sleep':SLEEP_DURATIONS}
def main():
    prepare_bat();prepare_sleep()
    path=DEVICE/'sprites.json';m=json.loads(path.read_text())
    for s in m['states']:
        name=s['name'];s['durations_ms']=DURATIONS[name]
        if name not in ('sleep','bat_swing'):
            s['frames']=[str(p.relative_to(DEVICE)).replace('\\','/') for p in sorted((DEVICE/'frames'/name).glob('*.png'))]
        if len(s['frames'])!=len(s['durations_ms']): raise ValueError(f'{name}: frame count mismatch')
    path.write_text(json.dumps(m,indent=2)+'\n')
if __name__=='__main__': main()
