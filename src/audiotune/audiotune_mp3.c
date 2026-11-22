/* Bounded, memory-backed MP3 decoder. No file I/O in the audio callback.
   Library: minimp3 (CC0-1.0, https://github.com/lieff/minimp3).
   This file is MIT; bundled third-party decoder retains its own notice. */
#define MINIMP3_IMPLEMENTATION
#include "audiotune_mp3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool rewind_decoder(AudioTuneMP3 *m) {
    if (!m->bytes || !m->length) return false;
    mp3dec_init(&m->decoder);
    m->pos=m->audio_start; /* Cached ID3v2 skip; no tag parsing on each loop. */
    m->decoded_at=0;
    m->decoded_frames=0;
    return true;
}

/* This function never calls allocation or the filesystem. */
static bool read_frame(AudioTuneMP3 *m,float sample[2]) {
    int misses=0;
    while (m->decoded_at >= m->decoded_frames) {
        mp3dec_frame_info_t info;
        int frames;
        size_t remain;
        if (m->pos >= m->length || ++misses > 32) return false;
        remain=m->length-m->pos;
        if (remain < 4u) return false;
        memset(&info,0,sizeof(info));
        frames=mp3dec_decode_frame(&m->decoder,m->bytes+m->pos,
                                   (int)remain,m->decoded,&info);
        if (info.frame_bytes <= 0 || (size_t)info.frame_bytes > remain)
            return false;
        m->pos+=(size_t)info.frame_bytes;
        if (frames <= 0) continue;
        if (info.layer!=3 || (info.channels!=1 && info.channels!=2) ||
            info.hz<16000 || info.hz>48000 ||
            frames*info.channels>MINIMP3_MAX_SAMPLES_PER_FRAME) return false;
        /* Do not produce garbage if a concatenated file changes sample rate. */
        if (m->rate && (info.hz!=m->rate || info.channels!=m->channels))
            return false;
        m->rate=info.hz;
        m->channels=info.channels;
        m->decoded_frames=frames;
        m->decoded_at=0;
    }
    sample[0]=(float)m->decoded[m->decoded_at*m->channels]/32768.0f;
    sample[1]=(float)m->decoded[m->decoded_at*m->channels+(m->channels==2 ? 1 : 0)]/32768.0f;
    ++m->decoded_at;
    return true;
}

void audiotune_mp3_close(AudioTuneMP3 *m) {
    free(m->bytes);
    memset(m,0,sizeof(*m));
}
void audiotune_mp3_rewind(AudioTuneMP3 *m) {
    if (!m->loaded) return;
    rewind_decoder(m);
    if (!read_frame(m,m->prev) || !read_frame(m,m->next)) {
        m->loaded=false;
        return;
    }
    m->fraction=0;
}

bool audiotune_mp3_open(AudioTuneMP3 *m,const char *path) {
    FILE *f;
    long file_size;
    if (!m || !path) return false;
    audiotune_mp3_close(m);
    f=fopen(path,"rb");
    if (!f) { fprintf(stderr,"audiotune: sample.mp3 not found/readable\n"); return false; }
    if (fseek(f,0,SEEK_END)!=0 || (file_size=ftell(f))<0) {
        fprintf(stderr,"audiotune: sample.mp3 size lookup failed\n");
        fclose(f); return false;
    }
    if (file_size < 16 || (unsigned long)file_size>AUDIOTUNE_MP3_LIMIT) {
        fprintf(stderr,"audiotune: sample.mp3 size %ld rejected (limit %lu bytes)\n",
                file_size,(unsigned long)AUDIOTUNE_MP3_LIMIT);
        fclose(f); return false;
    }
    if (fseek(f,0,SEEK_SET)!=0) {
        fprintf(stderr,"audiotune: sample.mp3 cannot seek to beginning\n");
        fclose(f); return false;
    }
    m->length=(size_t)file_size;
    m->bytes=malloc(m->length);
    if (!m->bytes) { fprintf(stderr,"audiotune: sample.mp3 memory allocation failed (%lu bytes)\n",(unsigned long)m->length); fclose(f); m->length=0; return false; }
    if (fread(m->bytes,1,m->length,f)!=m->length) {
        fprintf(stderr,"audiotune: sample.mp3 short read\n");
        fclose(f); audiotune_mp3_close(m); return false;
    }
    fclose(f);
    /* An ID3v2 header is ten bytes plus a synchsafe tag length. Ignore
       malformed lengths and never skip beyond the buffered data. */
    m->audio_start=0;
    if (m->length>=10 && memcmp(m->bytes,"ID3",3)==0 &&
        m->bytes[3]>=2 && m->bytes[3]<=4 &&
        !(m->bytes[6]&0x80) && !(m->bytes[7]&0x80) &&
        !(m->bytes[8]&0x80) && !(m->bytes[9]&0x80)) {
        size_t id3_size=((size_t)m->bytes[6]<<21) |
                        ((size_t)m->bytes[7]<<14) |
                        ((size_t)m->bytes[8]<<7) | (size_t)m->bytes[9];
        size_t skip=10+id3_size;
        /* ID3v2.4 may include a ten-byte footer. */
        if (m->bytes[3]==4 && (m->bytes[5]&0x10)) skip+=10;
        if (skip < m->length) m->audio_start=skip;
    }
    rewind_decoder(m);
    if (!read_frame(m,m->prev) || !read_frame(m,m->next)) {
        fprintf(stderr,"audiotune: sample.mp3 first frames could not be decoded\n");
        audiotune_mp3_close(m);return false;
    }
    m->loaded=true;
    m->fraction=0;
    return true;
}

bool audiotune_mp3_sample(AudioTuneMP3 *m,int output_rate,float *left,float *right) {
    if (!m->loaded || output_rate<=0 || !left || !right) return false;
    *left=m->prev[0]+(m->next[0]-m->prev[0])*m->fraction;
    *right=m->prev[1]+(m->next[1]-m->prev[1])*m->fraction;
    m->fraction+=(float)m->rate/(float)output_rate;
    while (m->fraction>=1.0f) {
        m->fraction-=1.0f;
        m->prev[0]=m->next[0];
        m->prev[1]=m->next[1];
        if (!read_frame(m,m->next)) {
            if (!rewind_decoder(m) || !read_frame(m,m->next)) {
                m->loaded=false;
                break;
            }
        }
    }
    return true;
}
