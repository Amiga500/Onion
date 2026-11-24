#!/usr/bin/env python3
"""Production event loop: persistent standby mode, marker changes, pause and SIGCONT."""
import os
import http.server
import threading
from pathlib import Path
import signal
import subprocess
import tempfile
import time
import wave

ROOT=Path(__file__).resolve().parent.parent
APP=Path(os.getenv('REDAMP_APP_DIR',ROOT.parent/'App/RedAmp'))
EXE=ROOT/'build/redamp-host'


def until(predicate, desc, timeout=5):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        if predicate(): return
        time.sleep(.025)
    raise AssertionError(desc)


with tempfile.TemporaryDirectory(prefix='redamp-standby-') as tmp:
    base=Path(tmp);data=base/'data';data.mkdir();keys=base/'keys';log=base/'log'
    flag=base/'flag';video=base/'video';owner=base/'owner';vowner=base/'vowner'
    song=base/'Song.wav'
    with wave.open(str(song),'wb') as wav:
        wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(8000)
        wav.writeframes(b'\0\0'*8000*90)
    (data/'playlist.m3u').write_text('#EXTM3U\n'+str(song)+'\n')
    (data/'settings.ini').write_text('vis_mode=0\ncur_index=0\nresume_playing=1\n')
    env=dict(os.environ,MA_HOME=str(APP),MA_ROOT=str(base),MA_DATA=str(data),
             M2D_INPUT=str(keys),MA_EVENT_TEST='1',MA_NO_DSP='1',
             MA_BATTERY_MARKER=str(base/'battery'),REDAMP_TEST_OFFLINE='1',
             MA_STAY_AWAKE_FLAG=str(flag),MA_STAY_AWAKE_OWNER=str(owner),
             MA_VIDEO_PLAYING_FLAG=str(video),MA_VIDEO_PLAYING_OWNER=str(vowner))
    # Hold SELECT, wrap to standby (Quit, Clear, HTTPS, Standby), toggle.
    # Then leave Options, pause/resume, hold X across suspend, release X,
    # and finally exit normally. X must not toggle the expanded playlist.
    events=[(200,97,1),(1150,97,0)]
    for t in (1250,1350,1450,1550): events += [(t,103,1),(t+20,103,0)]
    events += [(1700,57,1),(1720,57,0),(1850,29,1),(1870,29,0),
               (2200,28,1),(2220,28,0),(3000,28,1),(3020,28,0),
               (3700,42,1),(5100,42,0),(5900,1,1),(5950,1,0)]
    keys.write_text(''.join(f'{t} {k} {v}\n' for t,k,v in events))
    with log.open('w') as output:
        p=subprocess.Popen([str(EXE)],env=env,stdout=output,stderr=output)
        try:
            until(lambda:flag.exists() and not video.exists(),'default stay_awake')
            until(lambda:video.exists() and not flag.exists(),'switch to video_playing')
            until(lambda:not video.exists(),'pause releases video_playing')
            until(video.exists,'resume reclaims video_playing')
            time.sleep(.85) # X down, but not held long enough to open visualization
            p.send_signal(signal.SIGSTOP)
            time.sleep(.75)
            p.send_signal(signal.SIGCONT)
            until(lambda:'[standby] resumed' in log.read_text(),'resume handler')
            assert p.wait(timeout=8)==0,log.read_text()
        finally:
            if p.poll() is None:p.kill();p.wait()
    ini=(data/'settings.ini').read_text()
    assert 'play_standby=0\n' in ini,ini
    assert 'playlist_expanded=0\n' in ini,ini
    assert not any(x.exists() for x in (flag,owner,video,vowner)),log.read_text()
    # Restart from persisted mode and play; no stay_awake should be created.
    (data/'settings.ini').write_text(ini.replace('resume_playing=0','resume_playing=1'))
    keys.write_text('1300 1 1\n1350 1 0\n')
    with log.open('a') as output:
        p=subprocess.Popen([str(EXE)],env=env,stdout=output,stderr=output)
        try:
            until(lambda:video.exists() and not flag.exists(),'persisted standby off')
            assert p.wait(timeout=6)==0,log.read_text()
        finally:
            if p.poll() is None:p.kill();p.wait()
    assert not any(x.exists() for x in (flag,owner,video,vowner))
print('PASS standby menu, persistence, pause/resume, SIGSTOP/SIGCONT and marker cleanup')

# A live stream must reopen after wake rather than reuse a stale socket.
with tempfile.TemporaryDirectory(prefix='redamp-radio-resume-') as tmp:
    base=Path(tmp);data=base/'data';data.mkdir();keys=base/'keys';log=base/'log'
    song=base/'station.mp3'
    subprocess.run(['ffmpeg','-nostdin','-loglevel','error','-f','lavfi','-i',
                    'sine=frequency=440:duration=2','-c:a','libmp3lame',str(song)],check=True)
    pcm=song.read_bytes()
    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self,*args):pass
        def do_GET(self):
            self.send_response(200);self.send_header('Content-Type','audio/mpeg');self.end_headers()
            try:
                while True:
                    self.wfile.write(pcm);self.wfile.flush();time.sleep(.15)
            except (BrokenPipeError,ConnectionResetError):pass
    server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
    threading.Thread(target=server.serve_forever,daemon=True).start()
    url=f'http://127.0.0.1:{server.server_port}/live.mp3'
    (data/'playlist.m3u').write_text('#EXTM3U\n'+url+'\n')
    (data/'settings.ini').write_text('vis_mode=0\ncur_index=0\nresume_playing=1\nplay_standby=0\ninsecure_https=1\n')
    keys.write_text('5000 1 1\n5050 1 0\n')
    env=dict(os.environ,MA_HOME=str(APP),MA_ROOT=str(base),MA_DATA=str(data),
             M2D_INPUT=str(keys),MA_EVENT_TEST='1',MA_NO_DSP='1',
             MA_BATTERY_MARKER=str(base/'battery'),MA_STAY_AWAKE_FLAG=str(base/'flag'),
             MA_STAY_AWAKE_OWNER=str(base/'owner'),MA_VIDEO_PLAYING_FLAG=str(base/'video'),
             MA_VIDEO_PLAYING_OWNER=str(base/'vowner'),NO_PROXY='127.0.0.1',no_proxy='127.0.0.1')
    env.pop('REDAMP_TEST_OFFLINE',None)
    with log.open('w') as output:
        p=subprocess.Popen([str(EXE)],env=env,stdout=output,stderr=output)
        try:
            until(lambda:log.read_text().count('decoder ready: MP3')>=1,'initial radio open')
            p.send_signal(signal.SIGSTOP);time.sleep(.35);p.send_signal(signal.SIGCONT)
            until(lambda:log.read_text().count('decoder ready: MP3')>=2,'radio reconnected')
            assert p.wait(timeout=8)==0,log.read_text()
        finally:
            if p.poll() is None:p.kill();p.wait()
            server.shutdown();server.server_close()
    assert not (base/'video').exists(),log.read_text()
print('PASS live MP3 radio reconnect after SIGCONT')
