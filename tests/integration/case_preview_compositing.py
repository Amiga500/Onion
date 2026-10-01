# SPDX-License-Identifier: GPL-3.0-only
"""Verify text/background/cover layering and theme-dependent preview geometry."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
from PIL import Image
from env import BUILD, ONION_THEME, require_onion_theme  # noqa: E402

require_onion_theme()
ROOT = Path(__file__).resolve().parents[2]
OUT = Path(tempfile.mkdtemp(prefix='preview-compositing-', dir=BUILD))
SD = OUT/'sd'
THEME = OUT/'theme'
FALLBACK = ONION_THEME
for directory in ['Emu/FC','Roms/FC/Imgs','.tmp_update/config']:
    (SD/directory).mkdir(parents=True)
(THEME/'skin').mkdir(parents=True)
(THEME/'config.json').write_bytes((FALLBACK/'config.json').read_bytes())
(SD/'Emu/FC/config.json').write_text(json.dumps(dict(label='NES',rompath='../../Roms/FC',imgpath='../../Roms/FC/Imgs',extlist='nes')))
name='M'*22
(SD/'Roms/FC'/(name+'.nes')).write_bytes(b'')
background=THEME/'skin/preview-bg.png'
Image.new('RGBA',(340,360),(0,0,0,0)).save(background)
def capture(name,elapsed=0):
    path=OUT/(name+'.bmp')
    subprocess.run([os.environ.get('MAINUI_TEST_EXE',str(BUILD / "MainUI-dev")),
                   '--sd-root',str(SD),'--theme',str(THEME),'--fallback',str(FALLBACK),
                   '--system','NES','--elapsed',str(elapsed),'--snapshot',str(path)],cwd=ROOT,check=True,timeout=20)
    return Image.open(path).convert('RGB')
plain=capture('plain')
Image.new('RGBA',(10,10),(255,0,0,255)).save(SD/'Roms/FC/Imgs'/(name+'.png'))
transparent=capture('transparent')
# Text and selection remain visible beneath transparent preview pixels.
assert plain.crop((0,60,640,120)).tobytes()==transparent.crop((0,60,640,120)).tobytes()
assert all(abs(a-b)<=2 for a,b in zip(transparent.getpixel((425,240)),(255,0,0))) # x=640-340+(250-10)/2
bg=Image.new('RGBA',(340,360),(0,0,0,0))
bg.paste((0,255,0,128),(100,0,200,360))
bg.paste((0,0,255,255),(200,0,340,360))
bg.save(background)
layered=capture('layered')
assert layered.crop((300,60,400,120)).tobytes()==plain.crop((300,60,400,120)).tobytes()
assert all(abs(a-b)<=2 for a,b in zip(layered.getpixel((600,80)),(0,0,255)))
assert all(abs(a-b)<=2 for a,b in zip(layered.getpixel((425,240)),(255,0,0))) # Cover is composited after the background.
source=plain.getpixel((450,110))
actual=layered.getpixel((450,110))
expected=tuple((source[i]+(255 if i==1 else 0))//2 for i in range(3))
assert all(abs(a-b)<=2 for a,b in zip(actual,expected)),(actual,expected)
# A title hidden by the preview activates scrolling even if it fits the full row.
(SD/'.tmp_update/config/.romListTitleScroll').write_text('700,120')
idle=capture('scroll-idle',0)
scroll=capture('scroll-active',1000)
assert idle.crop((0,60,300,120)).tobytes()!=scroll.crop((0,60,300,120)).tobytes()
print('Preview alpha compositing, full-width labels, theme pane edge and overflow activation passed:',OUT)
