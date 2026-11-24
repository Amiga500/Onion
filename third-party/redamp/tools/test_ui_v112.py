#!/usr/bin/env python3
"""End-to-end host smoke checks for saved view and non-exiting EQ preset application."""
import os
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
APP = pathlib.Path(os.getenv('REDAMP_APP_DIR', ROOT.parent/'App'/'RedAmp'))
EXE = ROOT/'build'/'redamp-host'


def run(root, name, events, frames=35):
    root.mkdir(exist_ok=True)
    events_file=root/f'{name}.events'
    events_file.write_text(''.join(f'{f} {key} {v}\n' for f,key,v in events))
    dump=root/name
    dump.mkdir(exist_ok=True)
    env=dict(os.environ, MA_HOME=str(APP), MA_ROOT=str(root), MA_DATA=str(root/'data'),
             M2D_INPUT=str(events_file),M2D_DUMP=str(dump),M2D_DUMP_EVERY='5',
             MA_MAX_FRAMES=str(frames),MA_BATTERY_MARKER=str(root/'battery-marker'))
    p=subprocess.run([str(EXE)],env=env,capture_output=True,timeout=15)
    assert p.returncode==0, f'{name}: {p.stderr[-700:].decode(errors="replace")}'
    return dump


def pixel(path,x,y):
    with path.open('rb') as f:
        assert f.readline().strip()==b'P6'
        w,h=map(int,f.readline().split())
        assert (w,h)==(640,480)
        assert f.readline().strip()==b'255'
        f.seek((y*w+x)*3,1)
        return f.read(3)


def main():
    assert EXE.is_file(), 'build host player first'
    with tempfile.TemporaryDirectory(prefix='redamp-v112-ui-') as t:
        root=pathlib.Path(t)
        switch=run(root,'switch-to-library',[(3,97,1),(5,97,0)],25)
        ini=(root/'data'/'settings.ini').read_text()
        assert 'last_view=1\n' in ini, 'library mode not saved'
        restored=run(root,'restored-library',[],20)
        # Pixel in header, rather than ephemeral scan toast or cover status.
        assert pixel(switch/'f000015.ppm',20,25)==pixel(restored/'f000015.ppm',20,25), 'library not restored'
        run(root,'switch-to-player',[(3,97,1),(5,97,0)],22)
        ini=(root/'data'/'settings.ini').read_text()
        assert 'last_view=0\n' in ini, 'player mode not saved'
        print('PASS saved and restored Player/Library view')

        eqroot=root/'eq'
        shots=run(eqroot,'presets',[(3,56,1),(4,56,0),
                                    (8,42,1),(9,42,0),
                                    (14,57,1),(15,57,0),
                                    (24,29,1),(25,29,0)],40)
        ini=(eqroot/'data'/'settings.ini').read_text()
        assert 'eq_on=1\n' in ini and 'eq_preset=Flat\n' in ini, 'EQ preset not applied'
        # While A is pressed, presets card must remain visible.
        assert pixel(shots/'f000010.ppm',171,41)==pixel(shots/'f000020.ppm',171,41), 'A left presets card'
        assert pixel(shots/'f000020.ppm',171,41)!=pixel(shots/'f000030.ppm',171,41), 'B did not leave presets'
        print('PASS A applies an EQ preset without leaving menu; B returns')

if __name__=='__main__':
    main()
