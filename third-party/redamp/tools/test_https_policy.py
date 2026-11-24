#!/usr/bin/env python3
"""Real libcurl HTTPS policy test using a local self-signed wrong-host cert."""
import http.server
import io
import json
import os
from pathlib import Path
import ssl
import subprocess
import tempfile
import threading
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent

def run(args, **kwargs):
    return subprocess.run(args, cwd=ROOT, check=True, **kwargs)

with tempfile.TemporaryDirectory(prefix='redamp-https-') as tmp:
    root = Path(tmp)
    cert, key = root/'cert.pem', root/'key.pem'
    run(['openssl','req','-x509','-newkey','rsa:2048','-nodes','-days','1',
         '-subj','/CN=wrong.invalid','-keyout',str(key),'-out',str(cert)],
        capture_output=True)
    radio_c = root/'radio.c'
    radio_c.write_text('''#include "radio.h"
#include "netclock.h"
#include <assert.h>
#include <stdlib.h>
int main(int argc,char **argv){
    assert(argc==3);int insecure=atoi(argv[2]);netclock_set_insecure(insecure);
    radio_stream *r=radio_open(argv[1]);assert(r);
    unsigned char data[64];int n=radio_read(r,data,sizeof data);
    if(insecure)assert(n==64);else assert(n==0&&radio_take_network_error()==4);
    radio_close(r);return 0;
}
''')
    radio, covers = root/'radio', root/'covers'
    common = ['gcc','-O1','-DM2D_HOST','-I.','-Ithird_party']
    run(common + [str(radio_c),'radio.c','netclock.c','-o',str(radio),'-lcurl','-lpthread'])
    run(common + ['tools/test_covers.c','covers.c','netclock.c','-o',str(covers),
                  '-Wl,--wrap=write','-lcurl','-lpthread','-lm'])
    image = io.BytesIO(); Image.new('RGB',(32,32),(80,100,120)).save(image,format='JPEG')
    jpeg=image.getvalue()
    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self,*args):pass
        def do_GET(self):
            if self.path=='/redirect':
                self.send_response(302);self.send_header('Location',secure+'/radio')
                self.end_headers();return
            if self.path.startswith('/mb'):
                data=json.dumps({'release-groups':[{'id':'11111111-2222-3333-4444-555555555555',
                    'title':'Sample Album','score':100,'artist-credit':[{'name':'Sample Artist'}]}]}).encode()
                mime='application/json'
            elif self.path.startswith('/caa'):
                self.send_response(302);self.send_header('Location',secure+'/cover.jpg')
                self.end_headers();return
            elif self.path=='/cover.jpg':data=jpeg;mime='image/jpeg'
            else:data=b'\xff\xfb\x90\x64'*4096;mime='audio/mpeg'
            self.send_response(200);self.send_header('Content-Type',mime)
            self.send_header('Content-Length',str(len(data)));self.end_headers()
            try:self.wfile.write(data)
            except (BrokenPipeError,ConnectionResetError,ssl.SSLError):pass
    class TestServer(http.server.ThreadingHTTPServer):
        def handle_error(self,request,client_address):
            import sys
            if not isinstance(sys.exc_info()[1], (BrokenPipeError, ConnectionResetError, ssl.SSLError)):
                super().handle_error(request,client_address)
    tls=TestServer(('127.0.0.1',0),Handler)
    ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);ctx.load_cert_chain(cert,key)
    tls.socket=ctx.wrap_socket(tls.socket,server_side=True)
    http=TestServer(('127.0.0.1',0),Handler)
    secure=f'https://127.0.0.1:{tls.server_port}'
    plain=f'http://127.0.0.1:{http.server_port}'
    for server in (tls,http):threading.Thread(target=server.serve_forever,daemon=True).start()
    env=dict(os.environ,MA_HOME=str(root),NO_PROXY='127.0.0.1',no_proxy='127.0.0.1')
    # Default rejects a self-signed cert, then a trusted but wrong-host cert.
    run([str(radio),secure+'/radio','0'],env=env,timeout=10,capture_output=True)
    (root/'cacert.pem').write_bytes(cert.read_bytes())
    run([str(radio),secure+'/radio','0'],env=env,timeout=10,capture_output=True)
    (root/'cacert.pem').write_text('not a valid CA bundle')
    for url in (secure+'/radio',plain+'/redirect'):
        run([str(radio),url,'1'],env=env,timeout=10,capture_output=True)
    print('PASS radio: default rejects cert/hostname; opt-in ignores broken CA and redirects')
    (root/'cacert.pem').unlink()
    track=root/'track.mp3';track.write_bytes(b'audio')
    for insecure in (False,True):
        data=root/('insecure' if insecure else 'verified');data.mkdir()
        cenv=dict(env,MA_DATA=str(data),REDAMP_MB_URL=secure+'/mb',REDAMP_CAA_URL=secure+'/caa')
        cenv.pop('REDAMP_TEST_INSECURE',None)
        if insecure:cenv['REDAMP_TEST_INSECURE']='1'
        result=run([str(covers),str(track),'Sample Artist','Sample Album','0'],env=cenv,
                   timeout=15,capture_output=True,text=True)
        assert ('saved=1' in result.stdout)==insecure,(result.stdout,result.stderr)
        assert bool(list((data/'covers').glob('*.jpg')))==insecure
        if not insecure:assert 'status=3' in result.stdout or 'status=1' in result.stdout
    print('PASS covers: verified rejects self-signed server; opt-in downloads through HTTPS redirect')
    tls.shutdown();http.shutdown()
