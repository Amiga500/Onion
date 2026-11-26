/* Route B frontend: shared destination bar, dedicated Now Playing and Queue,
 * and grouped settings. Included after the shared UI/audio action helpers. */
static const char *const tabs[]={"Library","Now Playing","Queue","Settings"};
static void draw_tabs(void){
    fill(0,0,640,34,C_PANEL);
    for(int i=0;i<4;i++){
        int x=i*160;uint32_t c=i==destination?accent():C_DIM;
        if(i==destination)fill(x+8,31,144,3,c);
        font_draw_mid(bb,f_mid,x+(160-font_width(f_mid,tabs[i]))/2,16,tabs[i],c);
    }
}
static void route_center(font*f,int x,int w,int cy,const char*str,uint32_t color){
    char fit[400];font_fit(f,str,w,fit,sizeof fit);
    font_draw_mid(bb,f,x+(w-font_width(f,fit))/2,cy,fit,color);
}
/* Fit a UTF-8 string into two lines, preferring a word boundary. */
static void route_wrap(font *f,int x,int w,int cy,int step,const char *str,uint32_t color){
    if(!str||!*str)return;
    if(font_width(f,str)<=w){font_draw_mid(bb,f,x,cy,str,color);return;}
    char first[512],last[512];const char *p=str,*end=str,*space=NULL;
    while(*p){const char *start=p;utf8_next(&p);size_t n=p-str;
        if(n>=sizeof first)break;
        memcpy(first,str,n);first[n]=0;
        if(font_width(f,first)>w)break;
        end=p;if(*start==' ')space=start;
    }
    if(space&&space>str)end=space;
    if(end==str){font_fit(f,str,w,first,sizeof first);font_draw_mid(bb,f,x,cy,first,color);return;}
    size_t n=end-str;memcpy(first,str,n);first[n]=0;
    while(*end==' ')end++;
    font_fit(f,end,w,last,sizeof last);
    font_draw_mid(bb,f,x,cy,first,color);font_draw_mid(bb,f,x,cy+step,last,color);
}
/* Cached, antialiased artwork fallback. The note is rasterized once per
 * accent colour, not on every frame. No external asset or font glyph needed. */
static float note_ellipse(float x,float y,float cx,float cy,float rx,float ry){
    return (sqrtf((x-cx)*(x-cx)/(rx*rx)+(y-cy)*(y-cy)/(ry*ry))-1)*fminf(rx,ry);
}
static float note_box(float x,float y,float cx,float cy,float hx,float hy,float r){
    float a=fabsf(x-cx)-hx+r,b=fabsf(y-cy)-hy+r;
    return hypotf(fmaxf(a,0),fmaxf(b,0))+fminf(fmaxf(a,b),0)-r;
}
static void artwork_placeholder(int x,int y,int size){
    static m2d_surf *art;static uint32_t cached;
    uint32_t acc=accent();
    if(!art)art=m2d_surface_new(236,236);
    if(!art)return;
    if(cached!=acc){cached=acc;
        for(int j=0;j<236;j++)for(int i=0;i<236;i++){
            float u=i+0.5f,v=j+0.5f;
            float glow=fmaxf(0,1-hypotf(u-65,v-42)/230.f);
            uint32_t bg=mix(C_PANEL,acc,0.07f+glow*0.23f);
            float disc=note_ellipse(u,v,118,118,81,81);
            bg=mix(bg,mix(C_PANEL,acc,0.26f),fmaxf(0,fminf(1,0.5f-disc)));
            float d=fminf(note_ellipse(u,v,91,150,22,17),note_ellipse(u,v,145,137,22,17));
            d=fminf(d,note_box(u,v,108,113,6,36,5));
            d=fminf(d,note_box(u,v,162,100,6,36,5));
            /* sloping beam, with rounded ends */
            d=fminf(d,note_box(u,v+(u-108)*0.24f,135,80,33,8,5));
            art->px[j*(art->stride/4)+i]=mix(bg,mix(C_TEXT,acc,0.28f),fmaxf(0,fminf(1,0.5f-d)));
        }
    }
    m2d_blit(art,0,0,236,236,bb,x,y,size,size,0xffffffffu,M2D_REPLACE,0);
}
static void draw_modern_eq(void){
    draw_tabs();fill(0,34,640,48,C_PANEL);
    font_draw_mid(bb,f_title,20,58,"EQUALIZER",accent());
    const char *state=S.eq_on?"On":"Off";pill(620,58,state,S.eq_on);
    char preset[100];snprintf(preset,sizeof preset,"Preset: %s",S.eq_preset[0]?S.eq_preset:"Custom");
    font_draw_mid(bb,f_hint,24,106,preset,C_MUTED);
    static const char *labels[]={"Pre","60","170","310","600","1k","3k","6k","12k","14k","16k"};
    int top=164,bot=350,mid=(top+bot)/2;
    font_draw_mid(bb,f_small,10,top,"+12",C_DIM);font_draw_mid(bb,f_small,21,mid,"0",C_DIM);font_draw_mid(bb,f_small,10,bot,"-12",C_DIM);
    fill(48,mid,574,1,C_LINE);
    for(int i=0;i<11;i++){
        float value=i?S.eq[i-1]:S.eq_pre;int cx=64+i*54,yy=(int)(mid-value*(bot-top)/24.f);
        uint32_t c=i==eq_sel?accent():C_MUTED;
        if(i==eq_sel)rrect(cx-24,132,48,270,8,C_PANEL);
        rrect(cx-4,top,8,bot-top,4,C_LINE);
        fill(cx-3,yy<mid?yy:mid,6,abs(yy-mid)+1,mix(C_LINE,c,.65f));
        rrect(cx-15,yy-7,30,14,5,c);
        char v[20];snprintf(v,sizeof v,"%+.0f",value);route_center(f_mid,cx-25,50,148,v,c);
        route_center(f_hint,cx-25,50,375,labels[i],c);
    }
    route_center(f_mid,24,592,420,"Preamp and 10 frequency bands · dB",C_DIM);
}
static void draw_now(void){
    m2d_clear(bb,C_BG,0);draw_tabs();
    pl_item it;int valid=pl_get(current,&it);const char *path=valid?it.path:NULL;
    const char *title,*artist,*album;playing_metadata(valid?&it:NULL,&title,&artist,&album);
    int loading=audio_buffering();
    const char *status=loading?"Loading radio...":audio_state()==A_PLAYING?"Playing":audio_state()==A_PAUSED?"Paused":"Stopped";
    if(S.classic_player){
        draw_main_window_at(34);
        m2d_surf *art=valid&&S.cover_player&&!radio_is_url(path)?album_art_player(path,112):NULL;
        int x=art?178:45,w=595-x;
        if(art)m2d_blit(art,0,0,art->w,art->h,bb,45,291,112,112,0xffffffffu,M2D_REPLACE,0);
        char fit[400];font_fit(f_now_title,title,w,fit,sizeof fit);font_draw_mid(bb,f_now_title,x,308,fit,C_TEXT);
        font_fit(f_now_meta,artist,w,fit,sizeof fit);font_draw_mid(bb,f_now_meta,x,341,fit,C_MUTED);
        font_fit(f_now_meta,album,w,fit,sizeof fit);font_draw_mid(bb,f_now_meta,x,368,fit,C_DIM);
        font_draw_mid(bb,f_hint,x,400,status,accent());
        if(valid&&col_find(&favorites,path)>=0)draw_heart(580,400,accent());
    }else{
        int size=236,x=24,y=62;
        if(S.cover_player){
            rrect(x,y,size,size,12,C_PANEL);
            m2d_surf*art=valid&&!radio_is_url(path)?album_art_player(path,size):NULL;
            if(art)m2d_blit(art,0,0,art->w,art->h,bb,x,y,size,size,0xffffffffu,M2D_REPLACE,0);
            else artwork_placeholder(x,y,size);
        }
        int tx=S.cover_player?282:28,tw=SCREEN_W-tx-24;
        route_wrap(f_now_title,tx,tw,82,30,title,C_TEXT);
        route_wrap(f_now_meta,tx,tw,151,25,artist,C_MUTED);
        route_wrap(f_now_meta,tx,tw,213,25,album,C_DIM);
        font_draw_mid(bb,f_hint,tx,280,status,accent());
        if(valid&&col_find(&favorites,path)>=0)draw_heart(tx+font_width(f_hint,status)+12,280,accent());
        char flags[100];snprintf(flags,sizeof flags,"Shuffle %s   ·   Repeat %s   ·   Volume %d%%",S.shuffle?"On":"Off",S.repeat==2?"One":S.repeat==1?"All":"Off",S.volume);
        route_center(f_hint,24,592,322,flags,C_MUTED);
        char a[32],b[32];int len=audio_len_ms();if(len<=0&&valid)len=it.dur_ms;
        fmt_time(a,sizeof a,audio_pos_ms());if(len>0)fmt_time(b,sizeof b,len);else snprintf(b,sizeof b,"--:--");
        font_draw_mid(bb,f_row,24,366,a,C_TEXT);font_draw_mid(bb,f_row,616-font_width(f_row,b),366,b,C_TEXT);
        draw_track_progress(24,350,592,C_LINE);
        route_center(f_mid,24,592,402,"L  Previous     START  Pause/Resume     R  Next",C_MUTED);
    }
    hint h[]={{"A","Play/Pause"},{"X","Favorite"},{"Y","Actions"},{"SELECT","View"}};hint_bar(h,4);
}
static int queue_rows(void){return (419-84)/LM->lib_h;}
static void draw_queue(void){
    m2d_clear(bb,C_BG,0);draw_tabs();fill(0,34,640,48,C_PANEL);
    font_draw_mid(bb,f_title,20,58,screen==SCR_MOVE?"MOVE TRACK":"PLAY QUEUE",accent());
    int n=pl_count(),rows=queue_rows();char summary[100];snprintf(summary,sizeof summary,"%d tracks%s",n,S.shuffle?" · Shuffle on":"");
    font_draw_mid(bb,f_hint,620-font_width(f_hint,summary),58,summary,C_DIM);
    int cursor=screen==SCR_MOVE?move_target:sel;if(cursor>=n)cursor=n?n-1:0;if(cursor<0)cursor=0;
    if(screen==SCR_PLAYLIST)sel=cursor;
    if(cursor<scroll)scroll=cursor;
    if(cursor>=scroll+rows)scroll=cursor-rows+1;
    if(scroll>n-rows)scroll=n-rows;
    if(scroll<0)scroll=0;
    pl_set_hint(scroll);
    for(int r=0;r<rows&&scroll+r<n;r++){
        int i=scroll+r,y=84+r*LM->lib_h,cy=y+LM->lib_h/2;pl_item it;if(!pl_get(i,&it))break;
        if(i==cursor)rrect(12,y,614,LM->lib_h-2,7,C_SEL);
        if(screen==SCR_MOVE&&i==move_original)fill(12,y,3,LM->lib_h-2,accent());
        if(i==current)draw_play_marker(20,cy,audio_state()!=A_PLAYING,accent());
        char label[400],num[20],d[20]="";snprintf(num,sizeof num,"%d",i+1);if(it.dur_ms>0)fmt_time(d,sizeof d,it.dur_ms);
        font_draw_mid(bb,LM->aux,42,cy,num,C_DIM);int fav=col_find(&favorites,it.path)>=0;
        font_fit(LM->lib,it.name,470-(fav?20:0),label,sizeof label);font_draw_mid(bb,LM->lib,86,cy,label,i==current?accent():C_TEXT);
        if(fav)draw_heart(552,cy,accent());
        font_draw_mid(bb,LM->aux,614-font_width(LM->aux,d),cy,d,C_DIM);
    }
    if(!n){route_center(f_mid,20,600,185,"Your play queue is empty",C_TEXT);route_center(f_small,20,600,218,"Browse the Library and play or add music",C_DIM);}
    draw_library_playback_strip();
    if(screen==SCR_MOVE){hint h[]={{"↑/↓","Position"},{"A","Move"},{"B","Cancel"}};hint_bar(h,3);}
    else{hint h[]={{"A","Play"},{"X","Favorite"},{"Y","Actions"},{"SELECT","View"}};hint_bar(h,4);}
}
/* Option IDs are shared with the existing setters to preserve system integration. */
#define DISP(i) (100+(i))
#define SET_MAINT 200
static const char *const groups[]={"Audio & Equalizer","Display & Skins","Album Covers","Internet & Radio","Power & Standby","Music in Games","Library Management","Quit RedAmp"};
static const int group_items[][10]={
    {OPT_VOLUME,OPT_BALANCE,OPT_EQ,OPT_SHUFFLE,OPT_REPEAT,-1},
    {DISP(D_APPEARANCE),DISP(D_SKIN),DISP(D_SMOOTH),DISP(D_VIS),DISP(D_TIME),DISP(D_LIST),-1},
    {DISP(D_FETCH),DISP(D_FOLDER),DISP(D_PLAYER),DISP(D_LIBRARY),-1},
    {OPT_INSECURE,-1}, {OPT_SCREEN,OPT_SLEEP,OPT_STANDBY,-1},
    {OPT_GAMEMUSIC,OPT_GAMEVOL,-1},{SET_MAINT,-1},{OPT_QUIT,-1}
};
static const char *const opt_labels[OPT_N]={"Turn screen off","Music in games","Game music level","Shuffle","Repeat","Sleep timer","Volume","Balance","Display","Play during standby","Allow insecure HTTPS","Clear play queue","Quit RedAmp","Equalizer"};
static const char *const disp_labels[D_N]={"Winamp skin","Smooth scaling","Classic mini visualizer","Classic time display","List text size","Download album covers","Save covers in album folders","Show cover on player","Show covers in Library","Playing appearance"};
static int group_count(void){int n=0;while(group_items[settings_group][n]>=0)n++;return n;}
static void route_setting_value(int id,char *v,int n,int *on){
    *on=0;v[0]=0;if(id<100){opt_value(id,v,n,on);return;}if(id>=200)return;
    switch(id-100){case D_APPEARANCE:snprintf(v,n,"%s",S.classic_player?"Winamp":"Modern");break;
    case D_SKIN:snprintf(v,n,"%s",sk->name);break;case D_SMOOTH:snprintf(v,n,"%s",S.skin_smooth?"Smooth":"Sharp");break;
    case D_VIS:snprintf(v,n,"%s",S.vis_mode==1?"Spectrum":S.vis_mode==2?"Scope":"Off");break;
    case D_TIME:snprintf(v,n,"%s",S.remaining?"Remaining":"Elapsed");break;case D_LIST:snprintf(v,n,"%s",S.list_size==0?"Small":S.list_size==1?"Medium":"Large");break;
    case D_FETCH:*on=S.cover_fetch;break;case D_FOLDER:*on=S.cover_save_to_folder;break;case D_PLAYER:*on=S.cover_player;break;case D_LIBRARY:*on=S.cover_library;break;}
    if(id>=DISP(D_FETCH)&&id<=DISP(D_LIBRARY))snprintf(v,n,"%s",*on?"On":"Off");
}
static void draw_settings_route(void){
    m2d_clear(bb,C_BG,0);draw_tabs();fill(0,34,640,48,C_PANEL);
    font_draw_mid(bb,f_title,20,58,screen==SCR_SETTINGS?"SETTINGS":groups[settings_group],accent());
    int n=screen==SCR_SETTINGS?8:group_count();
    for(int i=0;i<n;i++){
        int y=90+i*39,cy=y+19,selected=i==(screen==SCR_SETTINGS?settings_group:settings_row);
        if(selected)rrect(12,y,616,37,8,C_SEL);
        int id=screen==SCR_SETTINGS?-1:group_items[settings_group][i];
        const char *label=id<0?groups[i]:id<100?opt_labels[id]:id<200?disp_labels[id-100]:"Rescan / fetch covers";
        font_draw_mid(bb,f_mid,28,cy,label,selected?accent():C_TEXT);
        char v[96];int on;if(id>=0){route_setting_value(id,v,sizeof v,&on);if(v[0]){char fit[96];font_fit(f_key,v,168,fit,sizeof fit);pill(610,cy,fit,on);}}
        else if(i!=7)font_draw_mid(bb,f_mid,596,cy,"›",C_DIM);
    }
    draw_library_playback_strip();
    if(screen==SCR_SETTINGS){hint h[]={{"A","Open"},{"SELECT","View"}};hint_bar(h,2);}else{hint h[]={{"A","Select"},{"←/→","Change"},{"B","Back"},{"SELECT","View"}};hint_bar(h,4);}
}
static int route_key(int k,int repeat){
    if(screen==SCR_SETTINGS||screen==SCR_OPTIONS){
        int root=screen==SCR_SETTINGS,n=root?8:group_count(),*row=root?&settings_group:&settings_row;
        if(k==K_UP||k==K_DOWN)*row=(*row+(k==K_UP?n-1:1))%n;
        else if(k==K_B&&!repeat){if(!root)screen=SCR_SETTINGS;}
        else if(k==K_START&&!repeat)toggle_pause();
        else if((k==K_A&&!repeat)||k==K_LEFT||k==K_RIGHT){
            if(root){if(k==K_A){if(settings_group==7)quit=1;else{settings_row=0;screen=SCR_OPTIONS;}}return 1;}
            int id=group_items[settings_group][settings_row],dir=k==K_LEFT?-1:1,activate=k==K_A;
            if(id==OPT_EQ){if(activate){eq_return=SCR_OPTIONS;screen=SCR_EQ;}}
            else if(id==SET_MAINT){if(activate){collection_return=SCR_OPTIONS;library_menu_sel=0;screen=SCR_LIBRARY_MENU;}}
            else if(id<100){opt_sel=id;options_return_screen=SCR_OPTIONS;option_change(dir,activate);}
            else{display_sel=id-100;display_change(dir,activate);}
        }else if(k==K_L&&!repeat)skip(-1);else if(k==K_R&&!repeat)skip(1);
        return 1;
    }
    if(screen==SCR_MOVE){int n=pl_count();if(k==K_UP&&move_target>0)move_target--;else if(k==K_DOWN&&move_target+1<n)move_target++;
        else if(!repeat&&k==K_A){move_queue(move_original,move_target);if(S.shuffle)build_shuffle();save_all();screen=SCR_PLAYLIST;}
        else if(!repeat&&k==K_B)screen=SCR_PLAYLIST;
        return 1;}
    if(screen==SCR_NOW||screen==SCR_PLAYLIST){
        int queue=screen==SCR_PLAYLIST,n=pl_count();
        if(queue&&n&&(k==K_UP||k==K_DOWN))sel=(sel+(k==K_UP?n-1:1))%n;
        else if(queue&&k==K_L2){sel-=queue_rows();if(sel<0)sel=0;}
        else if(queue&&k==K_R2){sel+=queue_rows();if(sel>=n)sel=n?n-1:0;}
        else if(k==K_LEFT||k==K_RIGHT){if(!repeat)held_since=now_t;int step=now_t-held_since>2?30000:now_t-held_since>0.8?10000:5000;seek_by(k==K_LEFT?-step:step);}
        else if(!repeat){if(k==K_A){if(queue&&n){play_index(sel,0,0);if(S.shuffle)build_shuffle();}else if(!queue)toggle_pause();}
            else if(k==K_START)toggle_pause();else if(k==K_L)skip(-1);else if(k==K_R)skip(1);
            else if(k==K_X&&selected_collection_track())toggle_favorite();else if(k==K_Y){open_collection_actions();move_target=sel;}}
        return 1;
    }
    return 0;
}
