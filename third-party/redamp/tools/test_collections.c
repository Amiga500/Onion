#include "collections.h"
#include "utf8_text.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
int main(void){
    assert(text_contains("Björk – JÓGA", "bjÖRK"));
    assert(text_contains("Straße", "STRASSE"));
    assert(text_contains("МОСКВА", "москва"));
    assert(!text_contains("Björk", "Bjork"));
    assert(!text_contains("abc", "abcd"));
    assert(text_casecmp("Ångström", "ångström")==0);
    assert(text_casecmp("abc", "abd")<0);
    assert(text_count("åé€😀")==4 && text_count("\xc0\x80")==-1);
    char utf[COL_NAME+1]="Åä😀";text_erase(utf);assert(!strcmp(utf,"Åä"));
    text_erase(utf);assert(text_append(utf,sizeof utf,40,"é")&&!strcmp(utf,"Åé"));
    assert(!text_append(utf,sizeof utf,2,"x"));
    assert(col_valid_name("Björk & Åsa (2026)!"));
    assert(!col_valid_name("foo/bar")&&!col_valid_name("a:b")&&!col_valid_name("AUX.txt"));
    assert(!col_valid_name("hidden.")&&!col_valid_name(".hidden")&&!col_valid_name("COM¹"));
    assert(!col_valid_name("bad\xc0\x80"));
    char longname[COL_NAME+1]="";for(int i=0;i<40;i++)assert(text_append(longname,sizeof longname,40,"å"));
    assert(col_valid_name(longname)&&!text_append(longname,sizeof longname,40,"å"));
    char dir[]="/tmp/redamp-collections-XXXXXX";assert(mkdtemp(dir));
    char file[256],bad[256];snprintf(file,sizeof file,"%s/Åsa & Mix.m3u",dir);snprintf(bad,sizeof bad,"%s/no-dir/fail.m3u",dir);
    collection c={0},r={0};assert(col_valid_name("My Mix 1"));assert(!col_valid_name("../bad"));assert(!col_valid_name("CON"));assert(!col_valid_name(" bad"));
    assert(col_add(&c,"/music/Track 1.mp3","Track one"));assert(col_add(&c,"https://example.org/radio","Radio station"));
    assert(!col_add(&c,"/music/bad\npath",NULL));assert(col_write(&c,file));assert(col_read(&r,file));assert(r.count==2);assert(!strcmp(r.items[1].title,"Radio station"));
    assert(col_find(&r,"/music/Track 1.mp3")==0);col_remove(&r,0);assert(r.count==1);assert(!col_write(&r,bad));
    assert(col_read(&r,file)&&r.count==2);char names[COL_NAMES][COL_NAME+1];assert(col_list(dir,names)==1&&!strcmp(names[0],"Åsa & Mix"));
    col_clear(&c);col_clear(&r);unlink(file);rmdir(dir);puts("PASS collections persistence, names, radio titles, failed write and removal");return 0;
}
