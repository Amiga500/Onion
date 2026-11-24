#!/usr/bin/env python3
"""Exercise real curl worker against local MusicBrainz/CAA stubs without public network."""
import http.server, threading, tempfile, pathlib, subprocess, os, time, json, sys
from PIL import Image
from io import BytesIO
from urllib.parse import urlparse
BASE=pathlib.Path(__file__).resolve().parent.parent
EXE=BASE/'build/test_covers'
cmd=['gcc','-O1','-g','-DM2D_HOST','-D_DEFAULT_SOURCE','-I.','-Ithird_party',
     'tools/test_covers.c','covers.c','netclock.c','-o',str(EXE),'-Wl,--wrap=write','-lcurl','-lpthread','-lm']
subprocess.run(cmd,check=True,cwd=BASE,stdout=subprocess.DEVNULL)
im=Image.new('RGB',(32,32),(150,50,70)); buf=BytesIO();im.save(buf,format='JPEG'); JPEG=buf.getvalue()
ID='11111111-2222-3333-4444-555555555555'
class State:
    scenario='good'
    hits=[]
    retries=0
state=State()
class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self,*args):pass
    def do_GET(self):
        state.hits.append((time.monotonic(),self.path))
        if self.path.startswith('/ws/2/release-group') and state.scenario in ('mb503','busy') and (state.retries<1 or state.scenario=='busy'):
            state.retries+=1
            self.send_response(503)
            self.send_header('Retry-After','6' if state.scenario=='mb503' else '121')
            self.send_header('Content-Length','0');self.end_headers();return
        if self.path.startswith('/ws/2/release-group'):
            score=70 if state.scenario=='low' else 99
            artist='Other' if state.scenario=='artist' else 'Sample Artist'
            data=json.dumps({'release-groups':[{'artist-credit':[{'artist':{'id':'aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee'},'name':artist}],
                'id':ID,'score':score,'title':('Wrong Album' if state.scenario=='title' else 'Another Album' if 'Another%20Album' in self.path or 'Another+Album' in self.path else 'Sample Album')}]}).encode()
            typ='application/json'; status=200
        elif state.scenario=='redirect' and self.path.startswith('/release-group'):
            assert ID in self.path
            self.send_response(307);self.send_header('Location','/image.jpg');self.end_headers()
            self.wfile.write(b'redirect body');return
        elif state.scenario=='server': status=403;data=b'';typ='text/plain'
        elif state.scenario=='404': status=404;data=b'';typ='image/jpeg'
        elif state.scenario=='503' and state.retries<1:
            state.retries+=1;status=503;data=b'';typ='image/jpeg'
        elif state.scenario=='broken':status=200;data=b'bad image';typ='image/jpeg'
        elif state.scenario=='cancel':time.sleep(.4);status=200;data=JPEG;typ='image/jpeg'
        else:status=200;data=JPEG;typ='image/jpeg'
        self.send_response(status)
        self.send_header('Content-Length',str(len(data)))
        self.send_header('Content-Type',typ)
        self.end_headers()
        try:self.wfile.write(data)
        except BrokenPipeError:pass
srv=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
threading.Thread(target=srv.serve_forever,daemon=True).start()
addr=f'http://127.0.0.1:{srv.server_port}'
for scenario in ('good','low','artist','title','404','503','mb503','busy','broken','priority','cancel','full','enospc','server','redirect'):
    state.scenario=scenario;state.hits=[];state.retries=0
    with tempfile.TemporaryDirectory() as tmp:
        base=pathlib.Path(tmp)
        (base/'Music').mkdir(); folder=base/'Music'/'sample.mp3';folder.write_bytes(b'audio')
        data=base/'data';data.mkdir()
        if scenario=='priority':(folder.parent/'cover.png').write_bytes(b'preexisting cover')
        if scenario=='full':(data/'covers').write_bytes(b'not a directory')
        env=os.environ.copy();env.update(MA_DATA=str(data),REDAMP_MB_URL=addr+'/ws/2/release-group',
            REDAMP_CAA_URL=addr+'/release-group',MA_VERSION='1.1.0')
        command=[str(EXE),str(folder),'Sample Artist','Sample Album','0']
        if scenario=='cancel':command+=['cancel']
        if scenario=='enospc':env['REDAMP_TEST_ENOSPC']='1'
        out=subprocess.run(command,cwd=BASE,env=env,capture_output=True,text=True,timeout=25)
        # A failed mkdir is acceptable for the synthetic full-disk test.
        assert out.returncode==0 or scenario=='full' and out.returncode==3, (scenario,out.stderr)
        saved=list((data/'covers').glob('*.jpg')) if (data/'covers').is_dir() else []
        expected=scenario in ('good','503','mb503','redirect')
        assert bool(saved)==expected, (scenario,saved,out.stdout)
        if scenario=='good':assert 'result saved=1' in out.stdout
        if scenario=='priority':assert 'existing=1' in out.stdout
        if scenario in ('404','low','artist','title'):assert 'missing=1' in out.stdout
        if scenario=='broken':assert 'failed=1 status=6' in out.stdout
        if scenario=='enospc':assert 'failed=1 status=5' in out.stdout
        if scenario=='server':assert 'status=4' in out.stdout
        if scenario=='busy':assert 'status=8' in out.stdout and len(state.hits)==1
        if scenario=='mb503':assert state.hits[1][0]-state.hits[0][0]>=5.95
        if scenario=='503':assert state.hits[2][0]-state.hits[1][0]>=4.95
        if scenario=='cancel':assert 'status=7' in out.stdout
        if scenario in ('server','busy','broken','enospc'):assert not (data/'covers/missing-v2.txt').exists()
        if scenario=='priority':assert not state.hits
        if scenario in ('low','artist','title'):assert len(state.hits)==1
        for (a,_),(b,path) in zip(state.hits,state.hits[1:]):
            if path!='/image.jpg':assert b-a>=1.45,(scenario,state.hits)
        print('PASS',scenario,'requests',len(state.hits),'saved',len(saved))
# Two albums in the same physical directory must not share a cache entry or
# be suppressed by a generic folder cover. Folder saves also stay album-specific.
with tempfile.TemporaryDirectory() as tmp:
    base=pathlib.Path(tmp);music=base/'Music';music.mkdir();data=base/'data';data.mkdir()
    first=music/'first.mp3';second=music/'second.mp3'
    first.write_bytes(b'audio');second.write_bytes(b'audio');(music/'cover.jpg').write_bytes(JPEG)
    cache=data/'covers';cache.mkdir()
    state.scenario='good';state.hits=[]
    env=dict(os.environ,MA_DATA=str(data),REDAMP_MB_URL=addr+'/ws/2/release-group',
             REDAMP_CAA_URL=addr+'/release-group',REDAMP_TEST_MIXED='1')
    p=subprocess.run([str(EXE),str(first),'Sample Artist','Sample Album','1',str(second)],
        env=env,capture_output=True,text=True,check=True,timeout=20)
    assert 'saved=2' in p.stdout,(p.stdout,p.stderr,state.hits)
    assert len(list(cache.glob('album-*.jpg')))==2
    assert len(list(music.glob('redamp-*.jpg')))==2
    assert (music/'cover.jpg').read_bytes()==JPEG
    print('PASS mixed albums: separate cache and folder copies, user folder cover preserved')
# Recent misses should skip both network requests; expired and duplicate rows are compacted.
with tempfile.TemporaryDirectory() as tmp:
    base=pathlib.Path(tmp); music=base/'Music';music.mkdir()
    track=music/'sample.mp3';track.write_bytes(b'audio')
    data=base/'data';cache=data/'covers';cache.mkdir(parents=True)
    h=14695981039346656037
    for byte in b'sample artist\0sample album\0': h=((h^byte)*1099511628211)&((1<<64)-1)
    key=f'{h:016x}';now=int(time.time());missing=cache/'missing-v2.txt'
    missing.write_text(f'{key} {now-5}\n{key} {now-1}\n0123456789abcdef {now-31*86400}\n')
    state.scenario='good';state.hits=[]
    env=dict(os.environ,MA_DATA=str(data),REDAMP_MB_URL=addr+'/ws/2/release-group',
             REDAMP_CAA_URL=addr+'/release-group',MA_VERSION='1.1.7')
    args=[str(EXE),str(track),'Sample Artist','Sample Album','0']
    subprocess.run(args,cwd=BASE,env=env,check=True,capture_output=True,timeout=15)
    assert not state.hits and missing.read_text()==f'{key} {now-1}\n'
    # Manual fetch overrides a recent negative cache record.
    subprocess.run(args,cwd=BASE,env=dict(env,REDAMP_TEST_MANUAL='1'),check=True,capture_output=True,timeout=15)
    assert len(state.hits)==2 and list(cache.glob('*.jpg'))
    for image in cache.glob('*.jpg'):image.unlink()
    state.hits=[]
    missing.write_text(f'{key} {now-31*86400}\n')
    subprocess.run(args,cwd=BASE,env=env,check=True,capture_output=True,timeout=15)
    assert len(state.hits)==2 and not missing.read_text() and list(cache.glob('*.jpg'))
    print('PASS negative cache: recent hit, duplicate/expired pruning, expired retry')
# Full app test where assets are available: the worker must survive SELECT and the
# previously missing player artwork must appear without restarting RedAmp.
app=pathlib.Path(os.environ.get('REDAMP_APP_DIR',BASE.parent/'App/RedAmp'))
player=BASE/'build/redamp-host'
if app.joinpath('skins').is_dir() and player.is_file():
    with tempfile.TemporaryDirectory() as tmp:
        root=pathlib.Path(tmp);music=root/'Media/Music/Sample Album';music.mkdir(parents=True)
        track=music/'sample.mp3'
        subprocess.run(['ffmpeg','-nostdin','-loglevel','error','-f','lavfi','-i','sine=frequency=440:duration=12',
            '-metadata','artist=Sample Artist','-metadata','album=Sample Album','-metadata','title=Sample Song',
            '-c:a','libmp3lame',str(track)],check=True)
        data=root/'data';data.mkdir();shots=root/'shots';shots.mkdir()
        (data/'settings.ini').write_text('last_view=1\ncover_fetch=1\ncover_player=1\ncur_index=0\nresume_playing=0\nvis_mode=0\nplaylist_expanded=0\n')
        (data/'playlist.m3u').write_text('#EXTM3U\n'+str(track)+'\n')
        keys=root/'keys';keys.write_text('8 97 1\n10 97 0\n')
        state.scenario='good';state.hits=[]
        env=dict(os.environ,MA_HOME=str(app),MA_ROOT=str(root),MA_DATA=str(data),
            REDAMP_MB_URL=addr+'/ws/2/release-group',REDAMP_CAA_URL=addr+'/release-group',
            M2D_INPUT=str(keys),M2D_DUMP=str(shots),M2D_DUMP_EVERY='5',MA_MAX_FRAMES='75',
            MA_NO_DSP='1',MA_BATTERY_MARKER=str(root/'marker'))
        proc=subprocess.run([str(player)],env=env,capture_output=True,text=True,timeout=20)
        assert proc.returncode==0,proc.stderr
        assert list((data/'covers').glob('*.jpg')),proc.stderr
        assert 'saved=1' in proc.stderr,proc.stderr
        frames=sorted(shots.glob('*.ppm'));assert frames
        def color(frame):return Image.open(frame).convert('RGB').getpixel((510,350))
        pixel=color(frames[-1]);assert all(abs(a-b)<8 for a,b in zip(pixel,(150,50,70))), (pixel,proc.stderr)
        print('PASS downloaded cover visible on Player after switching away from Library')
    # A saved station and automatic cover fetching must not contact a server
    # on an offline launch. Also verify a paused station is not opened online.
    for offline in (True,False):
        with tempfile.TemporaryDirectory() as tmp:
            root=pathlib.Path(tmp);data=root/'data';data.mkdir()
            (root/'Media/Music').mkdir(parents=True)
            if offline:
                subprocess.run(['ffmpeg','-nostdin','-loglevel','error','-f','lavfi','-i',
                    'sine=frequency=440:duration=1','-metadata','artist=Sample Artist',
                    '-metadata','album=Sample Album','-c:a','libmp3lame',
                    str(root/'Media/Music/song.mp3')],check=True)
            (data/'playlist.m3u').write_text('#EXTM3U\n'+addr+'/radio.mp3\n')
            (data/'settings.ini').write_text('last_view=1\ncover_fetch=1\ncur_index=0\nvis_mode=0\nresume_playing='+str(int(offline))+'\n')
            state.hits=[]
            env=dict(os.environ,MA_HOME=str(app),MA_ROOT=str(root),MA_DATA=str(data),
                MA_MAX_FRAMES='30',MA_NO_DSP='1',MA_BATTERY_MARKER=str(root/'marker'),
                REDAMP_MB_URL=addr+'/ws/2/release-group',REDAMP_CAA_URL=addr+'/release-group')
            if offline:env['REDAMP_TEST_OFFLINE']='1'
            proc=subprocess.run([str(player)],env=env,capture_output=True,text=True,timeout=15)
            assert proc.returncode==0,proc.stderr
            assert not state.hits, (offline,state.hits,proc.stderr)
            assert '[clock]' not in proc.stderr and '[play] could not open' not in proc.stderr
            assert 'cur_index=0' in (data/'settings.ini').read_text()
            print('PASS silent startup: '+('offline saved radio' if offline else 'online paused radio'))
srv.shutdown()
