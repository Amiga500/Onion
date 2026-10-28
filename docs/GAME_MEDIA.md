# Game media: box art, screenshots and videos

Open MainUI 1.1.0 (from RedOnion 4.4.1) can show more than one picture per
game. In **Game Info**, press **Right** to open a gallery with the box front
and back, the 3D box, screenshots and a video. The video can also play in the
game list itself (see [Video preview](#video-preview-in-the-game-list)).

This page explains where those files go, and how to get them with
[Skraper](https://www.skraper.net/).

## Where the files go

Everything goes in a `media` folder inside the console's ROM folder, one
subfolder per kind of picture. Each file has **the name of the ROM without its
extension**, not the name shown in the list:

```
Roms/GB/
├── Pokemon Red (USA).gb
├── Imgs/
│   └── Pokemon Red (USA).png        the cover in the game list (as before)
└── media/
    ├── box2dfront/
    │   └── Pokemon Red (USA).png    box, front
    ├── box2dback/
    │   └── Pokemon Red (USA).png    box, back
    ├── box3d/
    │   └── Pokemon Red (USA).png    box, 3D
    ├── screenshot/
    │   ├── Pokemon Red (USA).png    screenshot
    │   ├── Pokemon Red (USA)-1.png  more screenshots, up to 20
    │   └── Pokemon Red (USA)-2.png
    └── videos/
        └── Pokemon Red (USA).mp4    video
```

- Every part is optional: the gallery shows what it finds.
- `Imgs` stays the **cover in the game list**, as in Onion. `media` is only
  for the gallery and the video preview; the list never shows its files.
- Pictures are PNG, as Skraper saves them; videos are MP4.

## Getting the media with Skraper

If you are new to Skraper, first follow Onion's
[scraping guide](https://onionui.github.io/docs/advanced/scraping): it sets
up Skraper and the cover in `Imgs`. Then add the gallery media:

1. In Skraper, select **ALL SYSTEMS** on the left and open the **MEDIA** tab.
2. In the **Fetched Media List**, press **+** to add one entry for each kind
   of media you want, and set its **Media type**:

   | Media type in Skraper | Output folder |
   | --- | --- |
   | Box: front (2D) | `%ROMROOTFOLDER%\media\box2dfront` |
   | Box: back (2D) | `%ROMROOTFOLDER%\media\box2dback` |
   | Box: 3D | `%ROMROOTFOLDER%\media\box3d` |
   | Screenshot (in game) | `%ROMROOTFOLDER%\media\screenshot` |
   | Video (normalized) | `%ROMROOTFOLDER%\media\videos` |

   The names of the media types can differ slightly between Skraper versions.
   The output folders must be exactly as above, in lower case.
3. Keep the file name as Skraper sets it (the ROM name) and the PNG format for
   pictures.
4. Keep your existing entry for the list cover, with its output folder
   `%ROMROOTFOLDER%\Imgs`.
5. Select a system (or **ALL SYSTEMS**) and press the play button at the
   bottom right.

Skraper writes straight into the ROM folders of your SD card (or of the copy
on your PC: then copy the `media` folders to the SD card).

## Videos

Open MainUI plays **H.264 MP4** videos. They play best at about
**320 × 240** and 15–30 frames per second; videos larger than **1280 × 720**
are refused. Skraper's *normalized* videos are usually within that limit;
converting them to 320 × 240 makes them smaller and lighter to play. If a
video is too big, or doesn't play, convert it with
[FFmpeg](https://ffmpeg.org/) on your PC:

```sh
ffmpeg -i "Game.mp4" -vf "scale=320:-2" -r 24 -c:v libx264 -profile:v baseline -crf 28 -c:a aac -b:a 64k -ac 1 "converted/Game.mp4"
```

All the videos of a folder at once, on Linux or macOS:

```sh
mkdir -p converted
for f in *.mp4; do
  ffmpeg -i "$f" -vf "scale=320:-2" -r 24 -c:v libx264 -profile:v baseline -crf 28 -c:a aac -b:a 64k -ac 1 "converted/$f"
done
```

On Windows (PowerShell):

```powershell
mkdir converted
Get-ChildItem *.mp4 | ForEach-Object { ffmpeg -i $_.Name -vf "scale=320:-2" -r 24 -c:v libx264 -profile:v baseline -crf 28 -c:a aac -b:a 64k -ac 1 "converted\$($_.Name)" }
```

Then copy the files in `converted` to `media/videos` on the SD card.

Small videos also take less space on the card and less battery.

## On the device

- **Gallery:** select a game, open **Game Info**, press **Right**. Left and
  Right move through the media, **B** goes back. In a video, **A** pauses and
  **L** / **R** jump about 5 seconds.
- **Video preview in the game list:** Tweaks › Appearance › Game lists ›
  **Video preview** plays the selected game's video, silently, after the wait
  you choose; **Video preview size** sets it where the cover is or over the
  whole screen.
