#!/usr/bin/env python3
"""Controller regression: search, single-file Folder playback, and clear queue."""
from pathlib import Path
import tempfile, wave
from keyboard_keys import Keyboard
from test_collections_ui import Keys, run, paths, A,B,X,Y,DOWN,RIGHT,SELECT
UP=103

def main():
    with tempfile.TemporaryDirectory(prefix='redamp-search-queue-') as tmp:
        root=Path(tmp);data=root/'data';data.mkdir();music=root/'Media'/'Music';music.mkdir(parents=True)
        songs=[music/f'{name}.wav' for name in ('Alpha','Alpine','Beta')]
        for song in songs:
            with wave.open(str(song),'wb') as f:
                f.setnchannels(1);f.setsampwidth(2);f.setframerate(8000);f.writeframes(b'\0\0'*8000*30)
        queue=data/'playlist.m3u';fav=data/'favorites.m3u';lists=data/'playlists';lists.mkdir();saved=lists/'Test.m3u'
        for file in (queue,fav,saved):file.write_text('#EXTM3U\n'+'\n'.join(map(str,songs))+'\n')
        settings=data/'settings.ini';settings.write_text('vis_mode=0\nlast_view=0\ncur_index=-1\nshuffle=1\n')
        # Cancel once, then confirm. Hold-Y menu wraps up to its last action.
        k=Keys().hold(Y).tap(UP).tap(A).tap(B).tap(A).tap(A).exit()
        run(root,k);assert paths(queue)==[];assert paths(saved)==paths(fav)==list(map(str,songs))
        assert 'cur_index=-1' in settings.read_text()
        print('PASS confirmed clear queue preserves favorites and saved playlists',flush=True)
        # Library > Folders: Internet Radio sorts ahead of individual files.
        settings.write_text('vis_mode=0\nlast_view=1\ncur_index=-1\nshuffle=1\n')
        k=Keys().wait(900).down(3).tap(A).down(1).tap(A).exit()
        run(root,k);assert paths(queue)==[str(songs[0])],paths(queue)
        print('PASS Folder A queues only selected file with shuffle enabled',flush=True)
        settings.write_text('vis_mode=0\nlast_view=1\ncur_index=-1\n')
        k=Keys().wait(900).down(3).tap(A).down(1).tap(Y).exit()
        run(root,k);assert paths(queue)==list(map(str,songs)),paths(queue)
        print('PASS Folder Y still queues all tracks',flush=True)
        # Search for uppercase ALP, matching mixed-case Alpha and Alpine.
        settings.write_text('vis_mode=0\nlast_view=1\ncur_index=-1\n')
        k=Keys().wait(1100).down(6).tap(A);Keyboard(k).type("ALP");k.tap(Y).wait(100).down(1).tap(SELECT).tap(SELECT).tap(A).exit()
        run(root,k);assert paths(queue)==list(map(str,songs)),paths(queue)
        assert 'cur_index=1' in settings.read_text(),settings.read_text()
        print('PASS case-insensitive Search, existing queue preserved, selection survives Player switch',flush=True)
        # Search from quick views, query Z (no matches); A opens keyboard again,
        # erase, type A, then search, append selected track to existing queue.
        settings.write_text('vis_mode=0\nlast_view=1\ncur_index=-1\n')
        k=Keys().wait(1000).hold(B).tap(UP).tap(A);Keyboard(k).type("Z");k.tap(Y).tap(A).tap(X);Keyboard(k).type("A");k.tap(Y).tap(X).exit()
        run(root,k);assert paths(queue)==list(map(str,songs))+[str(songs[0])],paths(queue)
        print('PASS quick views Search, empty results, edit query, append track',flush=True)
        assert all(s.is_file() for s in songs)
if __name__=='__main__':main()
