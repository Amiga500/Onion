#!/usr/bin/env python3
"""Test source transformation without requiring a cross-built FFmpeg tree."""
import importlib.util
from pathlib import Path
p = Path(__file__).parent / 'patch_ffmpeg_hls.py'
spec = importlib.util.spec_from_file_location('redamp_hls', p)
mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
source = '''typedef struct HLSContext {
    AVDictionary *avio_opts;
} HLSContext;
static int save_avio_options(AVFormatContext *s)
{
    HLSContext *c = s->priv_data;
    int ret = 0;
    return ret;
}
static int nested_io_open(AVFormatContext *s) {return 0;}
static const AVOption hls_options[] = {
    {NULL}
};
static const AVClass hls_class = {};
'''
result = mod.patch_hls(source)
assert mod.patch_hls(result) == result
assert 'AVERROR(EINVAL)' in result
assert 'av_dict_set(&c->avio_opts, "ca_file"' in result
assert 'c->redamp_tls_verify ? "1" : "0"' in result
assert '{.i64 = 1}, 0, 1, FLAGS' in result
assert '"redamp_ca_file"' in result
# Simulate the exact cached patch from older RedAmp versions.
legacy = result.replace(mod.NEW_TAIL, mod.OLD_TAIL).replace(mod.TLS_OPT, '')
legacy = legacy.replace('    int redamp_tls_verify;\n', '')
assert mod.patch_hls(legacy) == result
try:
    mod.patch_hls(legacy.replace(mod.OLD_TAIL, 'broken patch'))
except ValueError:
    pass
else:
    raise AssertionError('broken old patch accepted')
print('PASS: secure default, explicit insecure policy, legacy patch upgrade and idempotence')
