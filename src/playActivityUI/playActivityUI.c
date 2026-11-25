#include "./playActivityUI.h"

static bool quit = false;

static void sigHandler(int sig)
{
    signal_handler_quit(&quit, sig);
}

static SDL_Surface *video;
static SDL_Surface *screen;

static SDL_Surface *background;

static TTF_Font *font40;
static TTF_Font *font30;
static TTF_Font *fontCJKRomName25;
static TTF_Font *font18;

static PlayActivities *play_activities;

static SDL_Color color_white = {255, 255, 255};
// Ranking numbers. The default is Onion's purple; a theme can change it
// through res/accent.txt ("R G B"), which system_skin.sh swaps with the
// rest of the Activity Tracker artwork when the theme changes.
static SDL_Color color_accent = {136, 97, 252};
static SDL_Color color_grey = {117, 123, 156};
static SDL_Color color_lightgrey = {214, 223, 246};

static bool show_raw_names = false;

static void loadAccentColor(void)
{
    FILE *fp = fopen("./res/accent.txt", "r");
    if (fp == NULL)
        return;
    int r, g, b;
    if (fscanf(fp, "%d %d %d", &r, &g, &b) == 3 && r >= 0 && r <= 255 &&
        g >= 0 && g <= 255 && b >= 0 && b <= 255)
        color_accent = (SDL_Color){r, g, b};
    fclose(fp);
}

// Cache of the 4 ROM images shown on the current page. Reloading + rescaling
// 4 images on every page change is the most expensive part of renderPage().
static SDL_Surface *rom_image_cache[4] = {NULL, NULL, NULL, NULL};
static int rom_image_cache_page = -1;

static void clearRomImageCache(void)
{
    for (int i = 0; i < 4; i++) {
        if (rom_image_cache[i] != NULL) {
            SDL_FreeSurface(rom_image_cache[i]);
            rom_image_cache[i] = NULL;
        }
    }
    rom_image_cache_page = -1;
}

void init(void)
{
    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    SDL_Init(SDL_INIT_VIDEO);
    SDL_ShowCursor(SDL_DISABLE);
    SDL_EnableKeyRepeat(300, 50);
    TTF_Init();

    video = SDL_SetVideoMode(640, 480, 32, SDL_HWSURFACE);
    screen = SDL_CreateRGBSurface(SDL_HWSURFACE, 640, 480, 32, 0, 0, 0, 0);

    background = IMG_Load("./res/background.png");
    loadAccentColor();

    font40 = TTF_OpenFont("/customer/app/Exo-2-Bold-Italic.ttf", 40);
    font30 = TTF_OpenFont("/customer/app/Exo-2-Bold-Italic.ttf", 30);
    fontCJKRomName25 = TTF_OpenFont("/customer/app/wqy-microhei.ttc", 25);
    font18 = TTF_OpenFont("/customer/app/wqy-microhei.ttc", 18);
}

void free_resources(void)
{
    clearRomImageCache();

    TTF_CloseFont(font40);
    TTF_CloseFont(font30);
    TTF_CloseFont(fontCJKRomName25);
    TTF_CloseFont(font18);

    TTF_Quit();

    SDL_FreeSurface(background);

    SDL_FreeSurface(screen);
    SDL_FreeSurface(video);
    SDL_Quit();

    free_play_activities(play_activities);
}

int _renderText(const char *text, TTF_Font *font, SDL_Color color, SDL_Rect *rect, bool right_align)
{
    int text_width = 0;
    SDL_Surface *textSurface = TTF_RenderUTF8_Blended(font, text, color);
    if (textSurface != NULL) {
        text_width = textSurface->w;
        if (right_align)
            SDL_BlitSurface(textSurface, NULL, screen, &(SDL_Rect){rect->x - textSurface->w, rect->y, rect->w, rect->h});
        else
            SDL_BlitSurface(textSurface, NULL, screen, rect);
        SDL_FreeSurface(textSurface);
    }
    return text_width;
}

int renderText(const char *text, TTF_Font *font, SDL_Color color, SDL_Rect *rect)
{
    return _renderText(text, font, color, rect, false);
}

int renderTextAlignRight(const char *text, TTF_Font *font, SDL_Color color, SDL_Rect *rect)
{
    return _renderText(text, font, color, rect, true);
}

SDL_Surface *loadRomImage(const char *image_path)
{
    SDL_Surface *img = IMG_Load(is_file(image_path) ? image_path : "/mnt/SDCARD/miyoo/app/skin/thumb-default.png");
    if (img == NULL)
        return NULL;
    if (img->w == 0 || img->h == 0) {
        SDL_FreeSurface(img);
        return NULL;
    }

    double sw = (double)IMG_MAX_WIDTH / img->w;
    double sh = (double)IMG_MAX_HEIGHT / img->h;
    double s = MIN(sw, sh);

    SDL_PixelFormat *ft = img->format;
    SDL_Surface *dst = SDL_CreateRGBSurface(0, (int)(s * img->w), (int)(s * img->h), ft->BitsPerPixel, ft->Rmask, ft->Gmask, ft->Bmask, ft->Amask);
    if (dst == NULL) {
        SDL_FreeSurface(img);
        return NULL;
    }

    SDL_Rect src_rect = {0, 0, img->w, img->h};
    SDL_Rect dst_rect = {0, 0, dst->w, dst->h};
    SDL_SoftStretch(img, &src_rect, dst, &dst_rect);

    SDL_FreeSurface(img);

    return dst;
}

/* New UI: drawn with the shared canvas look. Header with the chart icon
 * and the total play time as a chip; 4 games per page, each a row with its
 * rank, picture, name, details and total time; footer with the page. */
static void renderScreen(int current_page, int num_pages, const char *total_str)
{
    CuiCanvas c = {(uint32_t *)screen->pixels, screen->pitch / 4};
    cui_rect(&c, 0, 0, CUI_W, CUI_H, CUI_BG);
    char chip[STR_MAX];
    snprintf(chip, sizeof(chip), "%s total", total_str);
    cui_header(&c, "chart", "Activity Tracker", chip, -1);

    if (rom_image_cache_page != current_page) {
        clearRomImageCache();
        rom_image_cache_page = current_page;
        for (int row = 0; row < 4; row++) {
            int index = current_page * 4 + row;
            if (index >= play_activities->count)
                break;
            rom_image_cache[row] = loadRomImage(play_activities->play_activity[index]->rom->image_path);
        }
    }

    if (play_activities->count == 0)
        cui_text(&c, 320, 240, "No games played yet", 22, CUI_GREY, CUI_CENTER);

    for (int row = 0; row < 4; row++) {
        int index = current_page * 4 + row;
        if (index >= play_activities->count)
            break;
        PlayActivity *entry = play_activities->play_activity[index];
        ROM *rom = entry->rom;
        int top = 68 + row * 88;
        float cy = top + 40;

        cui_rrect(&c, 10, top, 620, 80, 12, CUI_RGB(36, 36, 39));
        char num_str[12];
        snprintf(num_str, sizeof(num_str), "%d", index + 1);
        cui_text(&c, 46, cy, num_str, 26, index < 3 ? CUI_ACCENT : CUI_GREY, CUI_CENTER);

        SDL_Surface *romImage = rom_image_cache[row];
        if (romImage != NULL) {
            SDL_Rect r = {76 + (72 - romImage->w) / 2, (Sint16)(top + 4 + (72 - romImage->h) / 2)};
            SDL_BlitSurface(romImage, NULL, screen, &r);
        }

        char rom_name[STR_MAX];
        if (show_raw_names) {
            strncpy(rom_name, rom->name, STR_MAX - 1);
            rom_name[STR_MAX - 1] = '\0';
        }
        else
            file_cleanName(rom_name, rom->name);

        char total[25], average[25], details[STR_MAX];
        str_serializeTime(total, entry->play_time_total);
        str_serializeTime(average, entry->play_time_average);
        snprintf(details, sizeof(details), "%d %s \xc2\xb7 %s on average", entry->play_count,
                 entry->play_count == 1 ? "time" : "times", average);

        int time_w = cui_text_width(total, 22);
        int name_max = 610 - 18 - time_w - 164;
        if (includeCJK(rom_name))
            renderText(rom_name, fontCJKRomName25, color_white, &(SDL_Rect){164, top + 10, name_max, 34});
        else {
            char fitted[STR_MAX];
            cui_fit(rom_name, 21, name_max, fitted, sizeof(fitted));
            cui_text(&c, 164, cy - 12, fitted, 21, CUI_WHITE, CUI_LEFT);
        }
        cui_text(&c, 164, cy + 16, details, 15, CUI_GREY, CUI_LEFT);
        cui_text(&c, 612, cy, total, 22, CUI_WHITE, CUI_RIGHT);
    }

    cui_rect(&c, 0, 428, CUI_W, 52, CUI_HEAD);
    int hx = 16;
    if (num_pages > 1) {
        cui_rrect(&c, hx, 442, 36, 24, 6, CUI_TRACK);
        cui_chevron(&c, hx + 10, 454, -1, CUI_WHITE);
        cui_chevron(&c, hx + 26, 454, 1, CUI_WHITE);
        hx += 36 + 7 + cui_text(&c, hx + 43, 454, "PAGE", 20, CUI_WHITE, CUI_LEFT) + 20;
    }
    hx += cui_hint(&c, hx, 454, "Y", show_raw_names ? "CLEAN NAMES" : "FILE NAMES", 20);
    cui_hint(&c, hx, 454, "B", "EXIT", 20);
    char pages[24];
    snprintf(pages, sizeof(pages), "%d/%d", current_page + 1, num_pages > 0 ? num_pages : 1);
    cui_text(&c, 624, 454, pages, 17, CUI_GREY, CUI_RIGHT);
}

int main(int argc, char *argv[])
{
    log_setName("playActivityUI");

    init();

    play_activities = play_activity_find_all();
    printf_debug("found %d roms\n", play_activities->count);

    int num_pages = (int)ceil((double)play_activities->count / (double)4);
    int current_page = 0;

    char play_time_total_formatted[STR_MAX];
    str_serializeTime(play_time_total_formatted, play_activities->play_time_total);

    renderScreen(current_page, num_pages, play_time_total_formatted);
    SDL_BlitSurface(screen, NULL, video, NULL);
    SDL_Flip(video);

    bool changed;
    KeyState keystate[320] = {(KeyState)0};
    bool menu_armed = false; // MENU pressed while Activity Tracker was open

    while (!quit) {
        changed = false;

        if (updateKeystate(keystate, &quit, true, NULL)) {
            // MENU leaves on its release, once its press was seen here:
            // RELEASED is also the state of every idle key.
            if (keystate[SW_BTN_MENU] >= PRESSED)
                menu_armed = true;
            else if (menu_armed)
                quit = true;
            if (keystate[SW_BTN_B] == PRESSED)
                quit = true;
            if (keystate[SW_BTN_RIGHT] >= PRESSED) {
                if (current_page < num_pages - 1) {
                    current_page++;
                    changed = true;
                }
            }
            if (keystate[SW_BTN_LEFT] >= PRESSED) {
                if (current_page > 0) {
                    current_page--;
                    changed = true;
                }
            }
            if (keystate[SW_BTN_Y] == PRESSED) {
                show_raw_names = !show_raw_names;
                changed = true;
            }
        }

        if (!changed) {
            SDL_Delay(15); // wait for input without spinning
            continue;
        }

        renderScreen(current_page, num_pages, play_time_total_formatted);

        SDL_BlitSurface(screen, NULL, video, NULL);
        SDL_Flip(video);
    }

    free_resources();

    return EXIT_SUCCESS;
}
