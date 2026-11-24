#!/usr/bin/env python3
"""Verify default, intentional opt-in, persisted restart and opt-out in the UI."""
import os
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parent.parent
APP=Path(os.getenv('REDAMP_APP_DIR',ROOT.parent/'App/RedAmp'))
with tempfile.TemporaryDirectory() as tmp:
    base=Path(tmp);data=base/'data';data.mkdir();keys=base/'keys'
    env=dict(os.environ,MA_HOME=str(APP),MA_ROOT=str(base),MA_DATA=str(data),
             M2D_INPUT=str(keys),MA_MAX_FRAMES='76',MA_NO_DSP='1',
             MA_BATTERY_MARKER=str(base/'marker'),REDAMP_TEST_OFFLINE='1')
    def run(events,expected):
        keys.write_text(''.join(f'{f} {k} {v}\n' for f,k,v in events))
        p=subprocess.run([str(ROOT/'build/redamp-host')],env=env,capture_output=True,timeout=12)
        assert p.returncode==0,p.stderr
        assert f'insecure_https={expected}\n' in (data/'settings.ini').read_text()
    run([],0)
    # Hold SELECT, wrap up three rows (Quit, Clear, Allow insecure HTTPS), press A.
    change=[(3,97,1),(36,97,0),(40,103,1),(41,103,0),
            (44,103,1),(45,103,0),(48,103,1),(49,103,0),(52,57,1),(53,57,0)]
    run(change,1);run([],1);run(change,0)
print('PASS HTTPS menu default, toggle, persistence and re-enable verification')
