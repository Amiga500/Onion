#!/usr/bin/env python3
"""Controller regression for non-destructive A playback across Library views."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import tempfile, wave
from keyboard_keys import Keyboard
from test_collections_ui import Keys, run, paths, A, B, Y, X, SELECT
UP=103

def check(view, shuffle):
    with tempfile.TemporaryDirectory(prefix='redamp-queue-preserve-') as tmp:
        root=Path(tmp);data=root/'data';data.mkdir();music=root/'Media'/'Music';music.mkdir(parents=True)
        songs=[music/f'{name}.wav' for name in ('Alpha','Alpine','Beta')]
        for song in songs:
            with wave.open(str(song),'wb') as f:
                f.setnchannels(1);f.setsampwidth(2);f.setframerate(8000);f.writeframes(b'\0\0'*8000*30)
        queue=data/'playlist.m3u';fav=data/'favorites.m3u';lists=data/'playlists';lists.mkdir();saved=lists/'Test.m3u'
        def write(file,tracks):file.write_text('#EXTM3U\n'+'\n'.join(map(str,tracks))+'\n')
        write(queue,[songs[2],songs[0]]);write(fav,songs[:2]);write(saved,songs[:2])
        settings=data/'settings.ini';settings.write_text(f'vis_mode=0\nlast_view=1\ncur_index=-1\nshuffle={shuffle}\n')
        k=Keys().wait(1300)
        if view=='Folders':k.down(3).tap(A).down(1)
        elif view=='Favorites':k.down(5).tap(A)
        elif view=='Songs':k.down(2).tap(A)
        elif view=='Albums':k.down(1).tap(A).tap(A)
        elif view=='Saved playlist':k.down(4).tap(A).tap(A)
        elif view=='Search':
            k.down(6).tap(A);Keyboard(k).type('ALP');k.tap(Y)
        # Append Alpine, then select it again; select existing Alpha last.
        k.down(1).tap(A).tap(A).tap(UP).tap(A).tap(SELECT).exit()
        run(root,k)
        assert paths(queue)==list(map(str,[songs[2],songs[0],songs[1]])),(view,paths(queue))
        assert 'cur_index=1\n' in settings.read_text(),(view,settings.read_text())
        assert paths(fav)==paths(saved)==list(map(str,songs[:2]))
        run(root,Keys().exit());assert paths(queue)==list(map(str,[songs[2],songs[0],songs[1]]))
        print(f'PASS {view}: append, reuse, preserve, persist (shuffle={shuffle})',flush=True)

if __name__=='__main__':
    cases=[('Folders',0),('Favorites',1),('Songs',1),('Albums',0),('Saved playlist',0),('Search',1)]
    with ThreadPoolExecutor(max_workers=3) as pool:
        list(pool.map(lambda case:check(*case),cases))
