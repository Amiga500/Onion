/* MiniCinema thumbnail PNG reader (GPL-3.0-or-later).
 * Tiny, bounded decoder for non-interlaced PNG, 8-bit RGB/RGBA/gray/palette.
 * Intentionally rejects oversize images, malformed deflate and unusual PNG
 * modes instead of depending on a native SDL/PNG runtime in our static browser.
 * All data is stored in static buffers: no malloc or libc dependency.
 */
#ifndef MC_PNG_H
#define MC_PNG_H
#define MC_PNG_MAX_FILE (2u * 1024u * 1024u)
#define MC_PNG_MAX_RAW (4u * 1024u * 1024u)
#define MC_PNG_MAX_ZLIB (2u * 1024u * 1024u)
static u8 mc_png_file[MC_PNG_MAX_FILE];
static u8 mc_png_zlib[MC_PNG_MAX_ZLIB];
static u8 mc_png_raw[MC_PNG_MAX_RAW];
struct mc_png_info {
    unsigned width,height,bpp,ctype,stride;
    u8 palette[256*4];
    unsigned palette_size;
};
static struct mc_png_info mc_png;
struct mc_bitreader { const u8 *src; unsigned count,pos; int failed; };
struct mc_huff { unsigned short counts[16], symbols[320]; };
static unsigned mc_be32(const u8 *s) {
    return ((unsigned)s[0]<<24)|((unsigned)s[1]<<16)|((unsigned)s[2]<<8)|s[3];
}
static unsigned mc_bit(struct mc_bitreader *b, unsigned n) {
    unsigned result=0;
    if (n>16 || (unsigned long)b->pos+n > (unsigned long)b->count*8u) {
        b->failed=1;return 0;
    }
    for (unsigned i=0;i<n;i++) {
        result|=((b->src[b->pos>>3]>>(b->pos&7))&1u)<<i;
        b->pos++;
    }
    return result;
}
static int mc_huff_build(struct mc_huff *h, const u8 *lens, unsigned n) {
    unsigned next[16],sum=0,left=1;
    if (n>320)return -1;
    for(unsigned i=0;i<16;i++)h->counts[i]=0;
    for(unsigned i=0;i<n;i++) {
        if (lens[i]>15)return -1;
        h->counts[lens[i]]++;
    }
    h->counts[0]=0;
    for(unsigned i=1;i<=15;i++) {
        left=(left<<1)-h->counts[i];
        if ((int)left<0 || left>65536)return -1;
    }
    for(unsigned i=1;i<=15;i++) { next[i]=sum; sum+=h->counts[i]; }
    for(unsigned i=0;i<n;i++)if(lens[i]) h->symbols[next[lens[i]]++]=(unsigned short)i;
    return 0;
}
static int mc_huff_get(struct mc_bitreader *b,const struct mc_huff *h) {
    unsigned code=0,first=0,index=0;
    for(unsigned len=1;len<=15;len++) {
        code|=mc_bit(b,1);
        if(b->failed)return -1;
        if(code-first<h->counts[len])return h->symbols[index+(code-first)];
        index+=h->counts[len];
        first=(first+h->counts[len])<<1;
        code<<=1;
    }
    return -1;
}
static int mc_inflate(const u8 *z,unsigned nz,u8 *out,unsigned cap,unsigned *got) {
    static const unsigned short lbase[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const u8 lextra[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const unsigned short dbase[30]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const u8 dextra[30]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    static const u8 corder[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    struct mc_bitreader br;
    struct mc_huff lit,dist,clen;
    u8 lengths[320], codelens[19];
    unsigned written=0;
    if(nz<6 || (z[0]&15)!=8 || ((z[0]<<8)|z[1])%31 || (z[1]&32))return -1;
    br.src=z+2;br.count=nz-6;br.pos=0;br.failed=0;
    for(;;) {
        unsigned last=mc_bit(&br,1),type=mc_bit(&br,2);
        if(br.failed)return -1;
        if(type==0) {
            br.pos=(br.pos+7u)&~7u;
            unsigned size=mc_bit(&br,16), nsize=mc_bit(&br,16);
            if(br.failed || ((size^nsize)&65535)!=65535 || size>cap-written)return -1;
            for(unsigned i=0;i<size;i++)out[written++]=(u8)mc_bit(&br,8);
            if(br.failed)return -1;
        } else if(type==1 || type==2) {
            unsigned nl=288,nd=32;
            if(type==1) {
                for(unsigned i=0;i<288;i++)lengths[i]=(i<=143?8:i<=255?9:i<=279?7:8);
                if(mc_huff_build(&lit,lengths,288)<0)return -1;
                for(unsigned i=0;i<32;i++)lengths[i]=5;
                if(mc_huff_build(&dist,lengths,32)<0)return -1;
            } else {
                nl=mc_bit(&br,5)+257;nd=mc_bit(&br,5)+1;
                unsigned nc=mc_bit(&br,4)+4;
                if(br.failed||nl>286||nd>32)return -1;
                for(unsigned i=0;i<19;i++)codelens[i]=0;
                for(unsigned i=0;i<nc;i++)codelens[corder[i]]=(u8)mc_bit(&br,3);
                if(br.failed||mc_huff_build(&clen,codelens,19)<0)return -1;
                unsigned i=0;
                while(i<nl+nd) {
                    int sym=mc_huff_get(&br,&clen);
                    if(sym<0)return -1;
                    if(sym<=15)lengths[i++]=(u8)sym;
                    else {
                        unsigned count=0,value=0;
                        if(sym==16){if(!i)return -1;count=mc_bit(&br,2)+3;value=lengths[i-1];}
                        else if(sym==17){count=mc_bit(&br,3)+3;}
                        else if(sym==18){count=mc_bit(&br,7)+11;}
                        else return -1;
                        if(br.failed||count>nl+nd-i)return -1;
                        while(count--)lengths[i++]=(u8)value;
                    }
                }
                if(mc_huff_build(&lit,lengths,nl)<0 || mc_huff_build(&dist,lengths+nl,nd)<0)return -1;
            }
            for(;;) {
                int sym=mc_huff_get(&br,&lit);
                if(sym<0)return -1;
                if(sym<256) {if(written>=cap)return -1;out[written++]=(u8)sym;}
                else if(sym==256)break;
                else {
                    if(sym>285)return -1;
                    unsigned i=(unsigned)sym-257;
                    unsigned count=lbase[i]+mc_bit(&br,lextra[i]);
                    int d=mc_huff_get(&br,&dist);
                    if(d<0||d>=30)return -1;
                    unsigned back=dbase[d]+mc_bit(&br,dextra[d]);
                    if(br.failed||back>written||count>cap-written)return -1;
                    while(count--) {out[written]=out[written-back];written++;}
                }
            }
        } else return -1;
        if(last)break;
    }
    if(br.failed)return -1;
    *got=written;
    return 0;
}
static int mc_png_decode(const u8 *buf,unsigned n) {
    unsigned off=8,zlen=0,w=0,h=0,bpp=0,ctype=0,rawlen=0;
    int seen_header=0,seen_end=0;
    if(n<45 || buf[0]!=137 || buf[1]!=80 || buf[2]!=78 || buf[3]!=71 ||
       buf[4]!=13 || buf[5]!=10 || buf[6]!=26 || buf[7]!=10)return -1;
    for(unsigned i=0;i<256;i++){mc_png.palette[i*4]=mc_png.palette[i*4+1]=mc_png.palette[i*4+2]=0;mc_png.palette[i*4+3]=255;}
    mc_png.palette_size=0;
    while(off+12<=n) {
        unsigned clen=mc_be32(buf+off);
        const u8 *type=buf+off+4;
        const u8 *data=buf+off+8;
        if(clen>n-off-12)return -1;
        if(type[0]=='I'&&type[1]=='H'&&type[2]=='D'&&type[3]=='R') {
            if(seen_header || clen!=13)return -1;
            w=mc_be32(data);h=mc_be32(data+4);ctype=data[9];
            if(data[8]!=8||data[10]||data[11]||data[12]||w==0||h==0||w>1024||h>1024)return -1;
            bpp=ctype==6?4:ctype==2?3:ctype==0||ctype==3?1:ctype==4?2:0;
            if(!bpp || (unsigned long)w*bpp>4096)return -1;
            mc_png.stride=w*bpp;
            if(((unsigned long)mc_png.stride+1)*h>MC_PNG_MAX_RAW)return -1;
            seen_header=1;
        } else if(type[0]=='P'&&type[1]=='L'&&type[2]=='T'&&type[3]=='E') {
            if(clen%3 || clen>768)return -1;
            mc_png.palette_size=clen/3;
            for(unsigned i=0;i<mc_png.palette_size;i++) {
                mc_png.palette[4*i]=data[i*3];mc_png.palette[4*i+1]=data[i*3+1];mc_png.palette[4*i+2]=data[i*3+2];
            }
        } else if(type[0]=='t'&&type[1]=='R'&&type[2]=='N'&&type[3]=='S'&&ctype==3) {
            if(clen>mc_png.palette_size)return -1;
            for(unsigned i=0;i<clen;i++)mc_png.palette[4*i+3]=data[i];
        } else if(type[0]=='I'&&type[1]=='D'&&type[2]=='A'&&type[3]=='T') {
            if(!seen_header||clen>MC_PNG_MAX_ZLIB-zlen)return -1;
            for(unsigned i=0;i<clen;i++)mc_png_zlib[zlen+i]=data[i];
            zlen+=clen;
        } else if(type[0]=='I'&&type[1]=='E'&&type[2]=='N'&&type[3]=='D') {
            seen_end=1;break;
        }
        off+=clen+12;
    }
    if(!seen_header||!seen_end||zlen<6||(ctype==3&&!mc_png.palette_size))return -1;
    if(mc_inflate(mc_png_zlib,zlen,mc_png_raw,MC_PNG_MAX_RAW,&rawlen)<0)return -1;
    if(rawlen!=(mc_png.stride+1)*h)return -1;
    for(unsigned y=0;y<h;y++) {
        unsigned row=y*(mc_png.stride+1);
        u8 mode=mc_png_raw[row];
        if(mode>4)return -1;
        for(unsigned x=0;x<mc_png.stride;x++) {
            unsigned p=row+1+x;
            unsigned left=x>=bpp?mc_png_raw[p-bpp]:0;
            unsigned up=y?mc_png_raw[p-(mc_png.stride+1)]:0;
            unsigned ul=y&&x>=bpp?mc_png_raw[p-(mc_png.stride+1)-bpp]:0;
            int add=0;
            if(mode==1)add=left;
            else if(mode==2)add=up;
            else if(mode==3)add=(left+up)/2;
            else if(mode==4) {
                int t=(int)left+(int)up-(int)ul;
                int pa=t-(int)left;if(pa<0)pa=-pa;
                int pb=t-(int)up;if(pb<0)pb=-pb;
                int pc=t-(int)ul;if(pc<0)pc=-pc;
                add=pa<=pb&&pa<=pc?left:pb<=pc?up:ul;
            }
            mc_png_raw[p]=(u8)(mc_png_raw[p]+add);
        }
    }
    mc_png.width=w;mc_png.height=h;mc_png.ctype=ctype;mc_png.bpp=bpp;
    return 0;
}
static void mc_png_pixel(unsigned x,unsigned y,u8 *rgba) {
    const u8 *p=mc_png_raw+y*(mc_png.stride+1)+1+x*mc_png.bpp;
    rgba[3]=255;
    if(mc_png.ctype==6){rgba[0]=p[0];rgba[1]=p[1];rgba[2]=p[2];rgba[3]=p[3];}
    else if(mc_png.ctype==2){rgba[0]=p[0];rgba[1]=p[1];rgba[2]=p[2];}
    else if(mc_png.ctype==0||mc_png.ctype==4){rgba[0]=rgba[1]=rgba[2]=p[0];if(mc_png.ctype==4)rgba[3]=p[1];}
    else if(p[0]<mc_png.palette_size){for(unsigned i=0;i<4;i++)rgba[i]=mc_png.palette[4*p[0]+i];}
    else rgba[3]=0;
}
#endif
