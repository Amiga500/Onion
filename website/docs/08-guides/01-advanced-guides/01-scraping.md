---
slug: /advanced/scraping
---

# Scraping artwork for games

<sup>Credit: f8less & Julian</sup>

:::note
Onion now includes a built-in scraper ([Scraper](../apps/scraper)) that works directly on your device! However, for large game collections, it's usually faster to scrape from a PC — this guide will help you configure optimal scraping settings.
:::

## File type and placement

Scraped artwork should be in _.PNG_ format, with a maximum size of 250px (W) × 360px (H). Images must be placed in the `Roms/<gamesystem>/Imgs` folder. 

**Important:** The `Imgs` folder name is case-sensitive (must have a capital `I`). Examples:    
`Roms/FC/Imgs`  
`Roms/SFC/Imgs`  
`Roms/GB/Imgs`  


## Box art/screenshot scraping process

### Install

- Download and extract _Skraper_ from: https://www.skraper.net/   
- Log in to Skraper with your account

### Initial setup

- Select "_Recalbox_" as your platform  
- Enable the option "_Include non-Recalbox rom folders_"   
   > Some systems aren't automatically recognized due to non-standard folder names. To add missing systems: press the `+` symbol in the bottom left, select the systems you need, and click "OK". Then click each added system in the left sidebar and correct the ROM folder path via the "_GAMES & FRONT END_" tab → "_Games/Roms folder:_" field, or by clicking the folder icon next to it.

[You can find a complete list of ROM folder names here](../emulators)


### Media selection

**Use a Custom Image Template Designed for OnionOS:**

**_Examples:_**<br/>
![image](https://user-images.githubusercontent.com/56418567/212767886-753a83ae-2f56-4255-a22d-f658ba656690.png)
![image](https://user-images.githubusercontent.com/56418567/212768343-a1d7d47b-1384-45a2-8f35-3d80b10fff5c.png)
![image](https://user-images.githubusercontent.com/56418567/212769101-5d5d5c77-bc23-43a2-83fd-859d938a0466.png)<br/>

**_Live Example:_**<br/>
![image](https://user-images.githubusercontent.com/56418567/212769542-49a3e1f4-971b-4fd4-bf79-36d589aee97a.png)<br/>

- Download the template from the Retro Game Handhelds Discord:<br/>
[Skraper_Mix_-_Miyoo_Mini_Big_Zoom_by_AchillesPDX.zip](https://github.com/OnionUI/Onion/raw/main/files/20240310/Skraper_Mix_-_Miyoo_Mini_Big_Zoom_by_AchillesPDX.zip) - _Template by: AchillesPDX_
- Extract the _.ZIP_ file into the following _Skraper_ folder: `.\Skraper-#.#.#\Mixes\`
- In the "ALL SYSTEMS" section on the left, go to the "_MEDIA_" tab → Clear the "_Fetched Media List_" of all but one image type by pressing the "_minus_" button
- Change "_Media type:_" to: "_USER PROVIDED MIX_" and click the file icon to the right
- Select the extracted: `Miyoo Mini Big Zoom.xml`
- Disable the settings "_Resize width to_", "_Resize height to_", and "_Keep Image Ratio_" 

> _**— OR —**_

**Customize Image Settings:**  
- In "ALL SYSTEMS" on the left, go to the "_MEDIA_" tab → Enable and set "_Resize width to_" to 250, then enable "_Keep Image Ratio_"    
- Remove unwanted image types from the "_Fetched Media List_" by selecting them and pressing the "_minus_" button, leaving only one picture type   
   > You can customize the image mix appearance using the two buttons under "_Media type_". We recommend changing "_4 IMAGES MIX_" to "_Screenscraper's Recalbox Mix V2_", but feel free to experiment with what works best for you.
   > If you choose an image that's taller than it is wide (like box art), set "_Resize height to_" to 360 and disable "_Keep Image Ratio_"


### Output settings

- Set "_Output folder_" to `%ROMROOTFOLDER%\Imgs` (note the capital `I`)
- Under 'Gamelist Link', ensure both "_Link from node '&lt;thumbnail&gt;'_" and '_Optimize media storage_' are enabled (these settings are important if you plan to generate a gamelist.xml to create a miyoogamelist.xml for use in Onion — more information on this below)
- Click the system you want to scrape on the left sidebar (or "All Systems" to scrape everything), then press the play button in the bottom right corner

This will automatically scrape and organize images into the correct folders for your Miyoo Mini!


### YouTube tutorials

#### Add Stunning Boxart To Your Miyoo Mini In Two Easy Ways _by RetroBreeze_

https://www.youtube.com/watch?v=RFu2DKRDq7o


#### EASY Scraping art for retro games (MiYoo Mini Skraper tutorial on Onion OS) _by TechDweeb_

https://www.youtube.com/watch?v=DguILcIyZQE&ab_channel=TechDweeb
