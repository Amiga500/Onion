#!/usr/bin/env python3
"""Exercise the production deadline scheduler with timed input on the host."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import wave

ROOT = Path(__file__).resolve().parent.parent
APP = Path(os.environ.get('REDAMP_APP_DIR', ROOT.parent/'App'/'RedAmp'))
EXE = ROOT/'build'/'redamp-host'


def run(case, music, vis, view, events, duration):
    case.mkdir(); data=case/'data'; data.mkdir(); keys=case/'keys'
    (data/'playlist.m3u').write_text('#EXTM3U\n'+str(music)+'\n')
    (data/'settings.ini').write_text(f'vis_mode={vis}\ndestination={view}\nclassic_player={1 if vis else 0}\n'
                                    'cur_index=0\nresume_playing=1\nskin_smooth=1\n')
    keys.write_text(''.join(f'{t} {k} {v}\n' for t,k,v in events+[
        (duration-100,1,1),(duration,1,0)]))
    env=dict(os.environ,MA_HOME=str(APP),MA_ROOT=str(case),MA_DATA=str(data),
             MA_EVENT_TEST='1',MA_NO_DSP='1',MA_PROFILE='1',M2D_INPUT=str(keys),
             MA_BATTERY_MARKER=str(case/'battery'),MA_STAY_AWAKE_FLAG=str(case/'flag'),
             MA_STAY_AWAKE_OWNER=str(case/'owner'))
    p=subprocess.run([str(EXE)],env=env,capture_output=True,text=True,timeout=duration/1000+6)
    assert p.returncode==0,p.stderr
    m=re.search(r'total loops (\d+) frames (\d+) elapsed',p.stderr)
    assert m,p.stderr
    assert not (case/'flag').exists() and not (case/'owner').exists()
    return tuple(map(int,m.groups()))


def main():
    with tempfile.TemporaryDirectory(prefix='redamp-event-') as temp:
        root=Path(temp); song=root/'A.wav'
        with wave.open(str(song),'wb') as wav:
            wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(8000)
            wav.writeframes(b'\0\0'*8000*120)
        idle=run(root/'idle',song,0,0,[],5000)
        assert idle[1]<35 and idle[0]<100,idle
        animated=run(root/'animated',song,1,1,[],3000)
        assert animated[1]>=65,animated
        # Enter Settings -> Power & Standby -> Turn screen off.
        off=run(root/'off',song,1,3,[(100,108,1),(140,108,0),(220,108,1),(260,108,0),
            (340,108,1),(380,108,0),(460,108,1),(500,108,0),(580,57,1),(620,57,0),
            (740,57,1),(780,57,0)],5000)
        assert off[1]<55 and off[0]<85,off
        print(f'PASS deadline scheduler: static loops/frames={idle}, mini vis={animated}, screen-off={off}')


if __name__=='__main__': main()
