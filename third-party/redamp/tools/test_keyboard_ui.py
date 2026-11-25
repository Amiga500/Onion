#!/usr/bin/env python3
"""Three keyboard pages, UTF-8 edit/save/rename, and casefolded library search."""
from pathlib import Path
import tempfile, wave
from test_collections_ui import Keys, run, paths, A,B,X,Y
from keyboard_keys import Keyboard

def main():
    with tempfile.TemporaryDirectory(prefix='redamp-keyboard-') as tmp:
        root=Path(tmp);data=root/'data';data.mkdir();music=root/'Media'/'Music';music.mkdir(parents=True)
        songs=[music/(x+'.wav') for x in ('Åsa','Björk','Beta')]
        for song in songs:
            with wave.open(str(song),'wb') as f:
                f.setnchannels(1);f.setsampwidth(2);f.setframerate(8000);f.writeframes(b'\0\0'*8000*15)
        settings=data/'settings.ini';settings.write_text('vis_mode=0\nlast_view=1\ncur_index=-1\n')
        # Create using Shift, international, Space, Symbols. Erase a multibyte char.
        k=Keys().wait(900).down(4).tap(A).tap(A);kb=Keyboard(k);kb.type('Åxé');k.tap(X);kb.type(' &');k.tap(Y).exit()
        run(root,k);file=data/'playlists'/'Åx &.m3u';assert file.is_file(),list((data/'playlists').iterdir())
        # Rename using on-screen Backspace, then a symbol and international letter.
        k=Keys().wait(700).down(4).tap(A).hold(X).tap(A);kb=Keyboard(k);kb.goto(4,2);k.tap(A);kb.type('ö!');k.tap(Y).exit()
        run(root,k);renamed=data/'playlists'/'Åx ö!.m3u';assert renamed.is_file() and not file.exists()
        # Search upper/lower case across UTF-8 filenames; play only the result.
        k=Keys().wait(900).down(6).tap(A);Keyboard(k).type('åS');k.tap(Y).tap(A).exit()
        run(root,k);assert paths(data/'playlist.m3u')==[str(songs[0])],paths(data/'playlist.m3u')
        print('PASS keyboard pages, Shift, UTF-8 erase, playlist create/rename, Unicode Search',flush=True)
if __name__=='__main__':main()
