#!/usr/bin/env python3
"""Controller-driven saved playlists, favorites and queue isolation regression."""
import os, pathlib, subprocess, tempfile, wave
from keyboard_keys import Keyboard
ROOT=pathlib.Path(__file__).resolve().parent.parent
APP=pathlib.Path(os.environ.get('REDAMP_APP_DIR',ROOT.parent/'App'/'RedAmp'))
EXE=ROOT/'build'/'redamp-host'
A,B,X,Y,DOWN,RIGHT,SELECT,MENU=57,29,42,56,108,106,97,1
class Keys:
    def __init__(self): self.t=500;self.events=[]
    def tap(self,key): self.events.extend([(self.t,key,1),(self.t+70,key,0)]);self.t+=220;return self
    def hold(self,key): self.events.extend([(self.t,key,1),(self.t+950,key,0)]);self.t+=1200;return self
    def wait(self,ms):self.t+=ms;return self
    def down(self,n):
        for _ in range(n):self.tap(DOWN)
        return self
    def exit(self):return self.tap(MENU)
def run(root,keys):
    data=root/'data'; f=root/'keys';f.write_text(''.join(f'{t} {k} {v}\n' for t,k,v in keys.events))
    env=dict(os.environ,MA_HOME=str(APP),MA_ROOT=str(root),MA_DATA=str(data),MA_EVENT_TEST='1',MA_NO_DSP='1',M2D_INPUT=str(f),MA_BATTERY_MARKER=str(root/'battery'),MA_STAY_AWAKE_FLAG=str(root/'flag'),MA_STAY_AWAKE_OWNER=str(root/'owner'))
    p=subprocess.run([str(EXE)],env=env,capture_output=True,text=True,timeout=keys.t/1000+8);assert p.returncode==0,p.stderr
    return p.stderr

def paths(file):return [x for x in file.read_text().splitlines() if x and not x.startswith('#')]

def main():
    with tempfile.TemporaryDirectory(prefix='redamp-collections-ui-') as d:
        root=pathlib.Path(d);data=root/'data';data.mkdir();music=root/'Media'/'Music';music.mkdir(parents=True)
        songs=[music/f'{x}.wav' for x in 'ABC']
        for song in songs:
            with wave.open(str(song),'wb') as f:f.setnchannels(1);f.setsampwidth(2);f.setframerate(8000);f.writeframes(b'\0\0'*8000*20)
        queue=data/'playlist.m3u';queue.write_text('#EXTM3U\n'+ '\n'.join(map(str,songs[:2]))+'\n');settings=data/'settings.ini';settings.write_text('vis_mode=0\nlast_view=0\ncur_index=-1\n')
        # Save current queue under A, remove first track, then reopen library and favorite a song.
        k=Keys().hold(Y).down(3).tap(A);Keyboard(k).type("A");k.tap(Y).wait(200).hold(Y).down(2).tap(A).exit()
        run(root,k)
        saved=data/'playlists'/'A.m3u';assert paths(saved)==list(map(str,songs[:2]));assert paths(queue)==[str(songs[1])]
        print('PASS save queue as named playlist and remove queue track without modifying saved list',flush=True)
        settings.write_text('vis_mode=0\nlast_view=1\ncur_index=-1\n')
        # Allow scan to complete; Songs -> third track -> favorite and add to A.
        k=Keys().wait(1200).down(2).tap(A).down(2).hold(Y).hold(X).tap(DOWN).tap(A).tap(A).exit()
        run(root,k)
        fav=data/'favorites.m3u';assert paths(fav)==[str(songs[2])];assert paths(saved)==list(map(str,songs));assert paths(queue)==[str(songs[1])]
        print('PASS favorite and add to saved playlist independently of play queue',flush=True)
        # Playlists -> A -> play third song; remove it from saved list, queue must remain intact.
        k=Keys().wait(1000).down(4).tap(A).tap(A).tap(Y).down(2).tap(A).wait(150).hold(X).down(2).tap(A).exit()
        run(root,k)
        assert paths(saved)==list(map(str,songs[:2])), paths(saved);assert paths(queue)==list(map(str,songs)), paths(queue)
        print('PASS play saved list and remove saved track independently of queue',flush=True)
        # Rename A -> AB, delete with confirmation; queue and favorites survive.
        k=Keys().wait(800).down(4).tap(A).hold(X).tap(A);Keyboard(k).type("B");k.tap(Y).exit()
        run(root,k)
        renamed=data/'playlists'/'AB.m3u';assert not saved.exists() and paths(renamed)==list(map(str,songs[:2]))
        k=Keys().wait(800).down(4).tap(A).hold(X).down(1).tap(A).tap(A).exit()
        run(root,k)
        assert not saved.exists() and not renamed.exists();assert paths(queue)==list(map(str,songs));assert paths(fav)==[str(songs[2])]
        print('PASS rename/delete playlist without deleting files, queue or favorites',flush=True)
        assert all(s.is_file() for s in songs)
        # Create an empty list from the always-present Create playlist row.
        k=Keys().wait(800).down(4).tap(A).tap(A);Keyboard(k).type("A");k.tap(Y).exit()
        run(root,k);assert paths(saved)==[]
        # Library cover setting is independent of Player artwork, defaults On.
        assert 'cover_library=1' in settings.read_text()
        k=Keys().hold(SELECT).down(8).tap(A).down(8).tap(A).tap(B).tap(B).exit()
        run(root,k);assert 'cover_library=0' in settings.read_text() and 'cover_player=1' in settings.read_text()
        run(root,Keys().exit());assert 'cover_library=0' in settings.read_text()
        print('PASS empty playlist creation and independent persisted Library cover toggle',flush=True)
if __name__=='__main__':main()
