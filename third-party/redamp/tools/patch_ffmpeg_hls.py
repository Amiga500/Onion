#!/usr/bin/env python3
"""Patch only RedAmp's private FFmpeg 4.4.8 hls.c.

Seed TLS policy for nested playlist, segment and AES key requests. Verified
TLS is the default. Upgrade the earlier RedAmp patch in place, without
recopying or changing the shared upstream FFmpeg source.
"""
from pathlib import Path
import sys


def replace_one(s, old, new, label):
    if s.count(old) != 1:
        raise ValueError(f'{label}: expected 1 anchor, got {s.count(old)}')
    return s.replace(old, new, 1)


OLD_TAIL = '''    /* HTTP does not reliably expose TLS child options to av_opt_get.
     * Seed the nested requests explicitly, including AES-128 key URLs. */
    if (!c->redamp_ca_file || !c->redamp_ca_file[0])
        return AVERROR(EINVAL);
    ret = av_dict_set(&c->avio_opts, "ca_file", c->redamp_ca_file, 0);
    if (ret < 0)
        return ret;
    ret = av_dict_set(&c->avio_opts, "tls_verify", "1", 0);
    if (ret < 0)
        return ret;
    return ret;
}'''
NEW_TAIL = '''    /* RedAmp: propagate explicit TLS policy to every nested URL. */
    if (c->redamp_tls_verify && (!c->redamp_ca_file || !c->redamp_ca_file[0]))
        return AVERROR(EINVAL);
    ret = av_dict_set(&c->avio_opts, "ca_file",
                      c->redamp_tls_verify ? c->redamp_ca_file : NULL, 0);
    if (ret < 0)
        return ret;
    ret = av_dict_set(&c->avio_opts, "tls_verify", c->redamp_tls_verify ? "1" : "0", 0);
    if (ret < 0)
        return ret;
    return ret;
}'''
CA_OPT = '''    {"redamp_ca_file", "RedAmp trusted CA for nested HLS requests",
     OFFSET(redamp_ca_file), AV_OPT_TYPE_STRING, {.str = NULL}, 0, 0, FLAGS},
'''
TLS_OPT = '''    {"redamp_tls_verify", "RedAmp verify TLS for nested HLS requests",
     OFFSET(redamp_tls_verify), AV_OPT_TYPE_INT, {.i64 = 1}, 0, 1, FLAGS},
'''


def patch_hls(s):
    if 'redamp_tls_verify' in s:
        if NEW_TAIL not in s or TLS_OPT not in s or CA_OPT not in s:
            raise ValueError('Unexpected existing RedAmp TLS patch')
        return s
    if 'redamp_ca_file' not in s:
        s = replace_one(s, '    AVDictionary *avio_opts;\n',
                        '    AVDictionary *avio_opts;\n    char *redamp_ca_file;\n', 'HLSContext')
        beg = s.index('static int save_avio_options(AVFormatContext *s)')
        end = s.index('static int nested_io_open(', beg)
        s = s[:beg] + replace_one(s[beg:end], '    return ret;\n}', OLD_TAIL,
                                  'save_avio_options') + s[end:]
        beg = s.index('static const AVOption hls_options[]')
        end = s.index('static const AVClass hls_class', beg)
        s = s[:beg] + replace_one(s[beg:end], '    {NULL}\n', CA_OPT + '    {NULL}\n',
                                  'hls_options') + s[end:]
    s = replace_one(s, '    char *redamp_ca_file;\n',
                    '    char *redamp_ca_file;\n    int redamp_tls_verify;\n', 'TLS policy field')
    s = replace_one(s, OLD_TAIL, NEW_TAIL, 'nested TLS policy')
    return replace_one(s, CA_OPT, CA_OPT + TLS_OPT, 'TLS policy option')


def main():
    if len(sys.argv) != 2:
        raise SystemExit('Usage: patch_ffmpeg_hls.py PRIVATE_SOURCE/libavformat/hls.c')
    p = Path(sys.argv[1]); before = p.read_text(); after = patch_hls(before)
    if before != after:
        p.write_text(after)
        print('Applied private FFmpeg 4.4.8 HLS TLS policy patch')
    else:
        print('Private FFmpeg HLS TLS policy patch already applied')


if __name__ == '__main__':
    main()
