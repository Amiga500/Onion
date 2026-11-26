#!/usr/bin/env python3
"""Route B controller integration: all four tabs, non-destructive A, actions, settings."""
import os, subprocess, tempfile, wave
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from test_collections_ui import Keys, paths, run, A, B, X, Y, SELECT
from keyboard_keys import Keyboard
UP,DOWN,LEFT,RIGHT=103,108,105,106

def fixture(root, dest=0):
    data=root/'data';data.mkdir();music=root/'Media'/'Music';music.mkdir(parents=True)
    songs=[music/f'{n}.wav' for n in ('Alpha','Beta','Gamma')]
    for song in songs:
        with wave.open(str(song),'wb') as f:
            f.setnchannels(1);f.setsampwidth(2);f.setframerate(8000);f.writeframes(b'\0\0'*8000*90)
    q=data/'playlist.m3u';q.write_text('#EXTM3U\n'+str(songs[2])+'\n')
    (data/'favorites.m3u').write_text('#EXTM3U\n'+str(songs[1])+'\n')
    (data/'settings.ini').write_text(f'destination={dest}\nvis_mode=0\ncur_index=-1\n')
    return data,songs,q

def library_case(root):
    data,songs,q=fixture(root)
    # Grid Songs is row 2, left. A appends and plays, X favorites, Y actions.
    k=Keys().wait(1200).tap(DOWN).tap(A).tap(A).tap(X).tap(DOWN).tap(A)
    # Cycle all four tabs, return to same track, then favorite it off.
    for _ in range(4):k.tap(SELECT)
    k.tap(X).tap(Y).down(5).tap(A).tap(B).exit() # Play all confirmation, cancel
    run(root,k)
    assert paths(q)==list(map(str,[songs[2],songs[0],songs[1]])),paths(q)
    assert paths(data/'favorites.m3u')==[str(songs[0])]
    assert 'destination=0\n' in (data/'settings.ini').read_text()
    print('PASS Library A, Favorites, tab return, cancelled queue replacement',flush=True)

def queue_case(root):
    data,songs,q=fixture(root,2)
    q.write_text('#EXTM3U\n'+'\n'.join(map(str,songs))+'\n')
    # Y menu: add playlist, favorite, move, remove, save, clear
    k=Keys().tap(Y).down(2).tap(A).tap(DOWN).tap(A).tap(Y).down(4).tap(A)
    Keyboard(k).type('Mix');k.tap(Y).exit();run(root,k)
    saved=data/'playlists'/'Mix.m3u';assert paths(q)==list(map(str,[songs[1],songs[0],songs[2]]));assert paths(saved)==paths(q)
    k=Keys().tap(Y).down(5).tap(A).tap(A).exit();run(root,k)
    assert paths(q)==[] and paths(saved)==list(map(str,[songs[1],songs[0],songs[2]]));assert paths(data/'favorites.m3u')==[str(songs[1])]
    print('PASS Queue move, save, confirmed clear, collection independence',flush=True)

def settings_case(root):
    data,songs,q=fixture(root,3)
    k=Keys().tap(DOWN).tap(A).tap(A).tap(B).down(2).tap(A).tap(A).tap(B).tap(SELECT).tap(SELECT).exit()
    run(root,k)
    ini=(data/'settings.ini').read_text();assert 'classic_player=1\n' in ini and 'insecure_https=1\n' in ini and 'destination=1\n' in ini,ini
    print('PASS grouped Settings, Winamp appearance, HTTPS opt-in and destination persistence',flush=True)

def radio_case(root):
    data,songs,q=fixture(root)
    k=Keys().wait(1200).down(3).tap(RIGHT).tap(A).tap(X).exit();run(root,k)
    fav=paths(data/'favorites.m3u');assert len(fav)==2 and fav[1].startswith('http'),fav
    assert paths(q)==[str(songs[2])]
    print('PASS direct radio browsing and favoriting without connecting',flush=True)

def search_case(root):
    data,songs,q=fixture(root)
    k=Keys().wait(1200).down(3).tap(A);Keyboard(k).type('beta');k.tap(Y).tap(A).exit();run(root,k)
    assert paths(q)==list(map(str,[songs[2],songs[1]])),paths(q)
    print('PASS Search keyboard and non-destructive result playback',flush=True)

def shortcut_case(root):
    data,songs,q=fixture(root)
    # Open Folders, then jump to Songs without returning through the root.
    k=Keys().wait(1200).tap(DOWN).tap(RIGHT).tap(A).tap(Y).tap(A).tap(UP).tap(A).tap(A).exit()
    run(root,k)
    assert paths(q)==list(map(str,[songs[2],songs[0]])),paths(q)
    print('PASS Library Views shortcut from Folders to Songs',flush=True)

def eq_quit_case(root):
    data,songs,q=fixture(root,1)
    # No current track: Equalizer is first Now Playing action.
    k=Keys().tap(Y).tap(A).tap(UP).tap(X).tap(DOWN).tap(A).tap(B).tap(B)
    k.tap(SELECT).tap(SELECT).tap(UP).tap(A)
    run(root,k)
    ini=(data/'settings.ini').read_text()
    assert 'eq_on=1\n' in ini,ini
    assert 'destination=3\n' in ini,ini
    print('PASS Modern EQ preset, return, and Quit RedAmp',flush=True)

def main():
    with tempfile.TemporaryDirectory(prefix='redamp-route-b-') as tmp:
        cases=[library_case,queue_case,settings_case,radio_case,search_case,shortcut_case,eq_quit_case]
        def check(fn):
            p=Path(tmp)/fn.__name__;p.mkdir();fn(p)
        with ThreadPoolExecutor(max_workers=5) as pool:list(pool.map(check,cases))
if __name__=='__main__':main()
