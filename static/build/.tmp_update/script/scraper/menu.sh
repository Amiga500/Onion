#!/bin/sh
#echo $0 $*    # for debugging

sysdir=/mnt/SDCARD/.tmp_update
PATH="$sysdir/bin:$PATH"
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/mnt/SDCARD/.tmp_update/lib:$sysdir/lib/parasyte"


romname=$(basename "$1")
CurrentSystem=$(echo "$1" | grep -o '/Roms/[^/]*' | cut -d'/' -f3)
romNameNoExtension=${romname%.*}
romPath=$1
romimage="/mnt/SDCARD/Roms/$CurrentSystem/Imgs/$romNameNoExtension.png"
ScraperConfigFile=/mnt/SDCARD/.tmp_update/config/scraper.json

# Prompts in the menus' style (shellect.sh): the button names (A, MENU,
# START: ...) in red, the rest as it is. One line of single-spaced words.
keys_red() { # text
    _kr_sep=
    # No globbing while splitting the words; the caller's setting after.
    case $- in *f*) _kr_glob= ;; *) _kr_glob=1 ;; esac
    set -f
    for _kr_w in $1; do
        printf '%s' "$_kr_sep"
        _kr_sep=' '
        _kr_key=${_kr_w%[,.]}
        case $_kr_key in
            A | A: | B | B: | X | X: | Y | Y: | MENU | MENU: | START | START: | SELECT | SELECT: | L1 | L1: | R1 | R1: | Enter | Enter:)
                printf '\033[31;1m%s\033[0m%s' "$_kr_key" "${_kr_w#"$_kr_key"}" ;;
            *) printf '%s' "$_kr_w" ;;
        esac
    done
    [ -n "$_kr_glob" ] && set +f
    return 0
}

# "Press A to ...": wait for A only. Any key used to go on, so a D-pad press
# (or the console resting on its buttons) skipped the message. A reaches the
# terminal as a space, START as Enter, the D-pad as Esc [ A..D: only the
# space ends the wait (or no input at all: the terminal is gone).
press_a() { # prompt
    keys_red "$1"
    while IFS= read -n 1 -s -r _pa_key; do
        [ "$_pa_key" = " " ] && break
    done
}

# Close the terminal this menu runs in (SIGTERM, as "pkill st" did).
# "pkill st" matched any process with "st" in its name (hostapd,
# installUI): only our own st is asked to quit, as shellect.sh's
# quit_terminal does. It hangs up this script.
close_terminal() {
    _ct_pid=$$
    while [ "$_ct_pid" -gt 1 ] 2> /dev/null; do
        _ct_stat=$(cat "/proc/$_ct_pid/stat" 2> /dev/null) || break
        _ct_comm=${_ct_stat#*\(}
        _ct_comm=${_ct_comm%%)*}
        if [ "$_ct_comm" = st ]; then
            kill -TERM "$_ct_pid"
            break
        fi
        # Fields after the name: state, then the parent's pid.
        set -- ${_ct_stat##*) }
        _ct_pid=$2
    done
}

# The keys at the bottom of every menu (shellect right-aligns them) and the
# titles, centred, in capitals: the same layout in every menu.
FootValidate="Menu: Exit      A: Validate"
FootToggle="Menu: Exit      A: On/Off"


# # for debugging
# echo "CurrentSystem : $CurrentSystem"
# echo "romname : $romname"
# echo "romimage : $romimage"
# echo "romNameNoExtension : $romNameNoExtension"
# echo "romimage : $romimage"
# press_a "Press A to continue"



##########################################################################################

Menu_Config()
{
    Option1="Media preferences"
	Option2="Region selection"
	Option3="Scraping sources"
    Option4="Screenscraper: account settings"
    Option5="Toggle background scraping"
    Option7="Screenscraper: Game Info media"
    Option6="Back"
    # Long jobs over many games (zazzagone): here rather than in the main
    # menu, where a wrong press could start hours or days of scraping.
    Option8="Scrape all favorites"
    Option9="Scrape all consoles"
    Option10="Delete media of deleted games: all consoles"

    Mychoice=$( echo -e "$Option1\n$Option2\n$Option3\n$Option4\n$Option5\n$Option7\n$Option8\n$Option9\n$Option10\n$Option6" | /mnt/SDCARD/.tmp_update/script/shellect.sh -t "--== CONFIGURATION ==--" -b "$FootValidate")

    # One case: the menus called here set Option* and Mychoice too, so a
    # test after them could match a value of theirs.
    case "$Mychoice" in
        "") ;;
        "$Option1") Menu_Config_MediaType ;;
        "$Option2") Menu_RegionSelection ;;
        "$Option3") Menu_Config_ScrapingSource ;;
        "$Option4") Menu_Config_SSAccountSettings ;;
        "$Option5") Menu_Config_BackgroundScraping ;;
        "$Option7") Menu_Config_GameInfo ;;
        "$Option8") Scrape_Favorites ;;
        "$Option9") Scrape_All_Consoles ;;
        "$Option10") Clean_Media all ;;
        "$Option6") Menu_Main ;;
    esac

	sync
}


##########################################################################################

Screenscraper_accountState()
{

	clear
	if [ "$userSS" = "null" ] || [ "$passSS" = "null" ] || [ "$userSS" = "" ] || [ "$passSS" = "" ]; then
		echo -e "Login or Password is empty!\nCheck user IDs!\n"
		press_a "Press A to continue"
		return
	fi
	
	echo "Retrieve account information..."
	# Appel de l'API avec curl et stockage de la réponse JSON dans une variable
	url="https://www.screenscraper.fr/api2/ssuserInfos.php?devid=xxx&devpassword=yyy&softname=zzz&output=json&ssid=$userSS&sspassword=$passSS"
	api_result=$(curl -k -s "$url")


	if echo "$api_result" | grep -q "^Erreur"; then
		echo -e "Authentification failed.\nCheck user IDs!\n"
		press_a "Press A to continue"
		return
	fi

	# Extraction des informations du JSON
	id=$(echo "$api_result" | jq -r '.response.ssuser.id')
	level=$(echo "$api_result" | jq -r '.response.ssuser.niveau')
	contribution=$(echo "$api_result" | jq -r '.response.ssuser.contribution')
	approvedParticipations=$(echo "$api_result" | jq -r '.response.ssuser.propositionok')
	maxThreads=$(echo "$api_result" | jq -r '.response.ssuser.maxthreads')
	maxDownloadSpeed=$(echo "$api_result" | jq -r '.response.ssuser.maxdownloadspeed')
	requestsToday=$(echo "$api_result" | jq -r '.response.ssuser.requeststoday')
	maxRequestsPerDay=$(echo "$api_result" | jq -r '.response.ssuser.maxrequestsperday')
	maxRequestsPerMinute=$(echo "$api_result" | jq -r '.response.ssuser.maxrequestspermin')
	lastScrape=$(echo "$api_result" | jq -r '.response.ssuser.datedernierevisite')

	# Affichage des informations extraites
	clear
	echo -e "\n*****************************************************"
	echo -e "*************** SCREENSCRAPER ACCOUNT ***************"
	echo -e "*****************************************************\n\n"
	echo -e "\n\n id: $id"
	echo " Level: $level"
	echo " Financial contribution: $contribution"
	echo " Approved participations: $approvedParticipations"
	echo " maxthreads: $maxThreads"
	echo " max download speed: $maxDownloadSpeed"
	echo " requests today: $requestsToday"
	echo " max requests per day: $maxRequestsPerDay"
	echo " max requests per minute: $maxRequestsPerMinute"
	echo -e " Last scrape: $lastScrape\n\n\n\n\n\n\n\n\n\n\n\n"

	press_a "Press A to continue"

}


##########################################################################################


Menu_Config_SSAccountSettings()
{
    
    echo "-------------------------------------------------"
    saveMetadata=false
    clear
    echo "Loading..."
    echo "==================================================="
    
    #We check for existing credentials
    
    if [ -f "$ScraperConfigFile" ]; then
    
        config=$(cat $ScraperConfigFile)
    
        userSS=$(echo "$config" | jq -r '.screenscraper_username')
        passSS=$(echo "$config" | jq -r '.screenscraper_password')
    
    
        if [ "$userSS" = "null" ] || [ "$passSS" = "null" ] || [ "$userSS" = "" ] || [ "$passSS" = "" ]; then
            userStored=false
        else
            userStored=true
        fi
    fi
    
    

        while true; do
			echo "username: $userSS"
			if ! [ -z "$passSS" ]; then
				passwordState="Password: xxxx (hidden)"
			else
				passwordState="Password: (not set)"
			fi
			clear
			Mychoice=$( echo -e "Screenscraper information\nUsername : $userSS\n${passwordState}\nAccount state and stats\nBack" | /mnt/SDCARD/.tmp_update/script/shellect.sh -t "--== SCREENSCRAPER ACCOUNT ==--" -b "$FootValidate")
			
			case "$Mychoice" in
				*Username\ *)
					clear
					echo -ne "\e[?25h"  # display the cursor
					echo -e "Press X to display the keyboard.\nPress A to enter a key.\nPress L1 for shift.\nPress R1 for backspace.\nPress Enter to validate.\n\n\n\nEnter your screenscraper username.\n\n"
					readline -m "username: "
					userSS=$(cat /tmp/readline.txt)
					userSS="${userSS// /}"  # removing spaces
					rm /tmp/readline.txt
					config=$(cat $ScraperConfigFile)
					config=$(echo "$config" | jq --arg user "$userSS" '.screenscraper_username = $user')
					echo "$config" > $ScraperConfigFile
					sync
					;;
				*Password:\ *)
					clear
					echo -ne "\e[?25h"  # display the cursor
					echo -e "Press X to display the keyboard.\nPress A to enter a key.\nPress L1 for shift.\nPress R1 for backspace.\nPress Enter to validate.\n\n\n\nEnter your screenscraper password.\n\n"
					readline -m "password: "
					passSS=$(cat /tmp/readline.txt)
					passSS="${passSS// /}"  # removing spaces
					rm /tmp/readline.txt
					config=$(cat $ScraperConfigFile)
					config=$(echo "$config" | jq --arg pass "$passSS" '.screenscraper_password = $pass')
					echo "$config" > $ScraperConfigFile
					sync
					;;
				*Screenscraper\ information*)
					Screenscraper_information
					;;
				*Account\ state\ and\ stats*)
					Screenscraper_accountState
					;;
				Back)
					Menu_Config 
					break
					;;
				*)
					false
					;;
			esac

        done

    clear
}


##########################################################################################

Menu_Config_BackgroundScraping()
{
    

    # Check if the configuration file exists
    if [ ! -f "$ScraperConfigFile" ]; then
      clear
      echo -e "Error: configuration file not found.\n"
      press_a "Press A to continue"
      exit 1
    fi
    
    clear
    echo -e "====================================================\n\n"
    echo -e "Background scraping allows you to use your\nMiyoo during scraping.\n\nYou may experience slowdowns,so choose \nnon-demanding emulators.\n\nHowever you will not see download in live if enabled.\n"
    echo -e "====================================================\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
    press_a "Press A to continue"
    clear
    
    config=$(cat "$ScraperConfigFile")
    ScrapeInBackground=$(echo "$config" | jq -r '.ScrapeInBackground')
    
    
    	Mychoice=$( echo -e "No\nYes\nBack" | /mnt/SDCARD/.tmp_update/script/shellect.sh -t "--== BACKGROUND SCRAPING: $([ "$ScrapeInBackground" = "true" ] && echo ON || echo OFF) ==--" -b "$FootValidate")
        # TODO : add a new option to display the tail of the log to show what happens in background

        if [ "$Mychoice" = "Yes" ]; then
            ScrapeInBackground="true"
        elif [ "$Mychoice" = "No" ]; then
            ScrapeInBackground="false"
		elif [ "$Mychoice" = "Back" ]; then
			Menu_Config
			return
        fi

        config=$(cat $ScraperConfigFile)
        config=$(echo "$config" | jq --arg ScrapeInBackground "$ScrapeInBackground" '.ScrapeInBackground = $ScrapeInBackground')
        echo "$config" > $ScraperConfigFile
        sync
        Menu_Config
}


##########################################################################################

# Game Info media: what ScreenScraper also gets for Open MainUI's Game Info
# (scrap_screenscraper.sh, gi_*), a comma list in "ScreenscraperGameInfo".
GI_ITEMS="desc box2dfront box2dback box3d screenshot video manual"
GI_DEFAULT="desc,box2dfront,box2dback,box3d,screenshot"

gi_label() { # item
    case "$1" in
        desc) echo "Description, genre, rating" ;;
        box2dfront) echo "Box front (gallery)" ;;
        box2dback) echo "Box back" ;;
        box3d) echo "3D box" ;;
        screenshot) echo "Screenshots" ;;
        video) echo "Video (a few MB each)" ;;
        manual) echo "Manual PDF (often big)" ;;
    esac
}

# The list with one item switched on or off, in GI_ITEMS order ("none" when
# empty, so that the default does not come back).
gi_toggle() { # list, item
    gi_new=""
    for gi_i in $GI_ITEMS; do
        gi_on=false
        case ",$1," in *",$gi_i,"*) gi_on=true ;; esac
        [ "$gi_i" = "$2" ] && { [ "$gi_on" = true ] && gi_on=false || gi_on=true; }
        [ "$gi_on" = true ] && gi_new="${gi_new:+$gi_new,}$gi_i"
    done
    echo "${gi_new:-none}"
}

Menu_Config_GameInfo()
{
    if [ ! -f "$ScraperConfigFile" ]; then
      clear
      echo -e "Error: configuration file not found.\n"
      press_a "Press A to continue"
      exit 1
    fi

    clear
    echo -e "====================================================\n\n"
    echo -e "Besides the cover, Screenscraper can get what\nGame Info shows: description, boxes,\nscreenshots, video and manual. They go in\nRoms/<console>/media and gamelist.xml.\n\nVideos and manuals are big: they take time\nand space on the SD card.\n"
    echo -e "====================================================\n\n\n\n\n\n\n\n\n\n"
    press_a "Press A to continue"

    while true; do
        config=$(cat "$ScraperConfigFile")
        GameInfo=$(echo "$config" | jq -r '.ScreenscraperGameInfo // empty')
        [ -n "$GameInfo" ] || GameInfo=$GI_DEFAULT

        list=""
        for item in $GI_ITEMS; do
            case ",$GameInfo," in *",$item,"*) state="On" ;; *) state="Off" ;; esac
            list="$list$(gi_label "$item"): $state\n"
        done
        clear
        Mychoice=$( echo -e "${list}Back" | /mnt/SDCARD/.tmp_update/script/shellect.sh -t "--== GAME INFO MEDIA ==--" -b "$FootToggle")

        picked=""
        for item in $GI_ITEMS; do
            case "$Mychoice" in "$(gi_label "$item"):"*) picked=$item ;; esac
        done
        [ -n "$picked" ] || break

        GameInfo=$(gi_toggle "$GameInfo" "$picked")
        config=$(echo "$config" | jq --arg GameInfo "$GameInfo" '.ScreenscraperGameInfo = $GameInfo')
        echo "$config" > "$ScraperConfigFile"
        sync
    done
    Menu_Config
}


##########################################################################################


Menu_Config_MediaType()
{
    # Check if the configuration file exists
    if [ ! -f "$ScraperConfigFile" ]; then
      clear
      echo -e "Error: configuration file not found.\n"
      press_a "Press A to continue"
      exit 1
    fi


    # Display Welcome
    clear
    echo -e 
    echo -e "====================================================\n\n"
    echo -e " All the media types are not available on\n each scraper engine.\n\n"
    echo -e "	SS = Screenscraper" 
    echo -e "	LB = Launchbox" 
    echo -e "	RA = Retroarch\n\n"
  
    echo -e "====================================================\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
	
    # retrieve current media settings
    config=$(cat "$ScraperConfigFile")
    RetroarchMediaType=$(echo "$config" | jq -r '.RetroarchMediaType')
    ScreenscraperMediaType=$(echo "$config" | jq -r '.ScreenscraperMediaType')
    LaunchBoxMediaType=$(echo "$config" | jq -r '.LaunchboxMediaType')
	
    press_a "Press A to continue"
    clear

    # Screenscreaper.fr
    Option01="Box Art                    (available on SS,LB,RA)"
    Option02="Screenshot - Title Screen  (available on SS,LB,RA)"
    Option03="Screenshot - In Game       (available on SS,LB,RA)"
    Option04="Box Art - 3D               (available on SS,LB)"
    Option05="Wheel                      (available on SS,LB)"
    Option06="Marquee                    (available on SS,LB)"
    Option07="Screenscraper Mix V1       (available on SS)"
    Option08="Screenscraper Mix V2       (available on SS)"
	Option09="Back"

    Mychoice=$( echo -e "$Option01\n$Option02\n$Option03\n$Option04\n$Option05\n$Option06\n$Option07\n$Option08\n$Option09\n" | /mnt/SDCARD/.tmp_update/script/shellect.sh -t "--== MEDIA TYPE: $(echo "$ScreenscraperMediaType" | tr a-z A-Z) ==--" -b "$FootValidate")
    
    [ "$Mychoice" = "$Option01" ]  && SSmediaType="box-2D"                 && LBmediaType="Box - Front"                      && RAmediaType="Named_Boxarts"  
    [ "$Mychoice" = "$Option02" ]  && SSmediaType="sstitle"                && LBmediaType="Screenshot - Game Title"          && RAmediaType="Named_Titles"  
    [ "$Mychoice" = "$Option03" ]  && SSmediaType="ss"                     && LBmediaType="Screenshot - Gameplay"            && RAmediaType="Named_Snaps"  
    [ "$Mychoice" = "$Option04" ]  && SSmediaType="box-3D"                 && LBmediaType="Box - 3D"                         && RAmediaType=""  
    [ "$Mychoice" = "$Option05" ]  && SSmediaType="wheel"                  && LBmediaType="Clear Logo"                       && RAmediaType=""  
    [ "$Mychoice" = "$Option06" ]  && SSmediaType="screenmarqueesmall"     && LBmediaType="Banner"                           && RAmediaType=""  
    [ "$Mychoice" = "$Option07" ]  && SSmediaType="mixrbv1"                && LBmediaType=""                                 && RAmediaType=""  
    [ "$Mychoice" = "$Option08" ]  && SSmediaType="mixrbv2"                && LBmediaType=""                                 && RAmediaType=""  
	[ "$Mychoice" = "$Option09" ]  && Menu_Config && return
					  
    clear


    config=$(cat $ScraperConfigFile)
    config=$(echo "$config" | jq --arg RAmediaType "$RAmediaType" '.RetroarchMediaType = $RAmediaType')
    config=$(echo "$config" | jq --arg LBmediaType "$LBmediaType" '.LaunchboxMediaType = $LBmediaType')
    config=$(echo "$config" | jq --arg SSmediaType "$SSmediaType" '.ScreenscraperMediaType = $SSmediaType')        
    echo "$config" > $ScraperConfigFile
    sync
    Menu_Config

}



##########################################################################################


Menu_RegionSelection()
{
    # Check if the configuration file exists
    if [ ! -f "$ScraperConfigFile" ]; then
      clear
      echo -e "Error: configuration file not found.\n"
      press_a "Press A to continue"
      exit 1
    fi

    # retrieve current media settings
    config=$(cat "$ScraperConfigFile")
    ScreenscraperRegion=$(echo "$config" | jq -r '.ScreenscraperRegion')


    # Display Welcome
    clear
    echo -e 
    echo -e "====================================================\n\n"
    echo -e " Select your country.\n\n"
    echo -e " If no media is found for your country code," 
    echo -e " other countries will be searched as fallback.\n" 
    echo -e "====================================================\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
    press_a "Press A to continue"
    clear


Option01="Australia (au)"
Option02="Brazil (br)"
Option03="Canada (ca)"
Option04="China (cn)"
Option05="Finland (fi)"
Option06="France (fr)"
Option07="Germany (de)"
Option08="Greece (gr)"
Option09="Italy (it)"
Option10="Japan (jp)"
Option11="Korea (kr)"
Option12="Netherlands (nl)"
Option13="Norway (no)"
Option14="Spain (sp)"
Option15="Sweden (se)"
Option16="United States (us)"
Option17="United Kingdom (uk)"
Option18="Back"

    Mychoice=$( echo -e "$Option01\n$Option02\n$Option03\n$Option04\n$Option05\n$Option06\n$Option07\n$Option08\n$Option09\n$Option10\n$Option11\n$Option12\n$Option13\n$Option14\n$Option15\n$Option16\n$Option17\n$Option18\n" \
	| /mnt/SDCARD/.tmp_update/script/shellect.sh -t "--== REGION: $(echo "$ScreenscraperRegion" | tr a-z A-Z) ==--" -b "$FootValidate") \
    
	SSregion=$(echo "$Mychoice" | sed -n 's/.*(\(.*\))/\1/p')
	LBregion=$(echo "$Mychoice" | sed -n 's/\(.*\) (.*)/\1/p')

	# exceptions :
	case "$Mychoice" in
	  "$Option12") LBregion="The Netherlands" ;;
	  "$Option18") Menu_Config; return; ;;
	esac
 clear

    config=$(cat $ScraperConfigFile)
    config=$(echo "$config" | jq --arg LBregion "$LBregion" '.LaunchboxRegion = $LBregion')
    config=$(echo "$config" | jq --arg SSregion "$SSregion" '.ScreenscraperRegion = $SSregion')        
    echo "$config" > $ScraperConfigFile
	sync    
    Menu_Config

}


##########################################################################################

Menu_Config_ScrapingSource()
{
    clear
    echo "Loading..."
    echo "==================================================="
    # The JSON file to modify
            
    # Read the content of the JSON file and convert it to a JSON string
    json=$(jq -c '.' "$ScraperConfigFile")
    
    
    # Recovering the values of variables
    Screenscraper_enabled=$(echo "$json" | jq -r '.Screenscraper_enabled')
    Launchbox_enabled=$(echo "$json" | jq -r '.Launchbox_enabled')
    Retroarch_enabled=$(echo "$json" | jq -r '.Retroarch_enabled')

    [ "$Screenscraper_enabled" = "true" ] && Screenscraper="[x] Screenscraper" || Screenscraper="[ ] Screenscraper"
    [ "$Launchbox_enabled" = "true" ] && Launchbox="[x] Launchbox" || Launchbox="[ ] Launchbox"
    [ "$Retroarch_enabled" = "true" ] && Retroarch="[x] Retroarch" || Retroarch="[ ] Retroarch"    
    
    while true; do
    
        Mychoice=$( echo -e "$Screenscraper\n$Launchbox\n$Retroarch\nBack" | /mnt/SDCARD/.tmp_update/script/shellect.sh -t "--== SCRAPING SOURCES ==--" -b "$FootValidate")
        
        [ "$Mychoice" = "[ ] Screenscraper" ] && Screenscraper="[x] Screenscraper"
        [ "$Mychoice" = "[x] Screenscraper" ] && Screenscraper="[ ] Screenscraper"
        [ "$Mychoice" = "[ ] Launchbox" ] && Launchbox="[x] Launchbox"
        [ "$Mychoice" = "[x] Launchbox" ] && Launchbox="[ ] Launchbox"
        [ "$Mychoice" = "[ ] Retroarch" ] && Retroarch="[x] Retroarch"
        [ "$Mychoice" = "[x] Retroarch" ] && Retroarch="[ ] Retroarch"

        [ "$Mychoice" = "Back" ] && break
    
    done
    
    [ "$Screenscraper" = "[x] Screenscraper" ] && Screenscraper_enabled="true" || Screenscraper_enabled="false"    
    [ "$Launchbox" = "[x] Launchbox" ] && Launchbox_enabled="true" || Launchbox_enabled="false"
    [ "$Retroarch" = "[x] Retroarch" ] && Retroarch_enabled="true" || Retroarch_enabled="false"
   

    # Modify the JSON string with the new variable values
    json=$(echo "$json" | jq --arg Screenscraper "$Screenscraper_enabled" --arg Launchbox "$Launchbox_enabled" --arg Retroarch "$Retroarch_enabled" '. + { Screenscraper_enabled: $Screenscraper, Launchbox_enabled: $Launchbox, Retroarch_enabled: $Retroarch }')

    # Rewrite the modified JSON content to the file
    #echo "$json"    # for debugging
    echo "$json" > "$ScraperConfigFile"
	sync
    #echo "$json" | jq '.' > "$ScraperConfigFile"
    
    Menu_Config

}


##########################################################################################


Launch_Scraping ()
{
    touch /tmp/stay_awake
    rm -f /tmp/scraper_script.sh
	
	if [ "$(ip r)" = "" ]; then 
		clear
		echo -e "You must be connected to Wi-Fi to use the scraper.\n"
		press_a "Press A to continue"
		exit
	fi

    # Check if the configuration file exists
    if [ ! -f "$ScraperConfigFile" ]; then
      clear
      echo -e "Error: configuration file not found.\n"
      press_a "Press A to continue"
      exit 1
    fi
    
    # Read the content of the configuration file
    config=$(cat "$ScraperConfigFile")
    
    # Get the values of the "Retroarch_enabled", "Screenscraper_enabled" and "Launchbox_enabled" keys
    Screenscraper_enabled=$(echo "$config" | jq -r '.Screenscraper_enabled')
    Launchbox_enabled=$(echo "$config" | jq -r '.Launchbox_enabled')
    Retroarch_enabled=$(echo "$config" | jq -r '.Retroarch_enabled')
    ScrapeInBackground=$(echo "$config" | jq -r '.ScrapeInBackground')
    MediaType=$(echo "$config" | jq -r '.MediaType')
    
    
    [ "$onerom" = "1" ] && onerom="$romname" || onerom=""
    
    # Check the value of each variable and run the corresponding script if the value is "true"

    if [ "$Screenscraper_enabled" = "true" ]; then
        if [ "$ScrapeInBackground" = "true" ]; then
            echo "/mnt/SDCARD/.tmp_update/script/scraper/scrap_screenscraper.sh $CurrentSystem" \"$onerom\" >>/tmp/scraper_script.sh
        else
            /mnt/SDCARD/.tmp_update/script/scraper/scrap_screenscraper.sh $CurrentSystem "$onerom"
        fi
    fi
    if [ -f "$romimage" ] && ! [ "$onerom" = "" ] ; then echo exiting $romimage; close_terminal; fi;  # exit if only one rom must be scraped and is already found

    if [ "$Launchbox_enabled" = "true" ]; then
        if [ "$ScrapeInBackground" = "true" ]; then
            echo "/mnt/SDCARD/.tmp_update/script/scraper/scrap_launchbox.sh $CurrentSystem" \"$onerom\" >>/tmp/scraper_script.sh
        else
            /mnt/SDCARD/.tmp_update/script/scraper/scrap_launchbox.sh $CurrentSystem "$onerom"
        fi
    fi
    if [ -f "$romimage" ] && ! [ "$onerom" = "" ] ; then  echo exiting $romimage ; close_terminal; fi;
    
    if [ "$Retroarch_enabled" = "true" ]; then
        if [ "$ScrapeInBackground" = "true" ]; then
            echo "/mnt/SDCARD/.tmp_update/script/scraper/scrap_retroarch.sh $CurrentSystem" \"$onerom\" >>/tmp/scraper_script.sh
        else
            /mnt/SDCARD/.tmp_update/script/scraper/scrap_retroarch.sh $CurrentSystem "$onerom"
        fi
    fi
    if [ -f "$romimage" ] && ! [ "$onerom" = "" ] ; then  echo exiting $romimage ; close_terminal; fi;

	rm -f /tmp/stay_awake
	close_terminal
}

##########################################################################################

Screenscraper_information () {
clear

cat << 	EOF

===================================
      = Screenscraper.fr =
===================================

Maximum number of requests per day:

-----------------------------------

Non-registered : 10.000 requests per day

level 0 to 19  : 20.000 requests per day
level 19 to 29 : 50.000 requests per day
level > 29     : 100.000 requests per day

1 or 2€  donation : 50.000  requests per day
5 or 10€ donation : 100.000 requests per day
moderator/Admin     : 200.000 requests per day

-----------------------------------





EOF

press_a "Press A to continue"
clear 

cat << 	EOF

===================================
      = Screenscraper.fr =
===================================

Additional information :

-----------------------------------

Increase your level requires to participate in the
evolution of the Screenscraper database and media. 

Quota counters are reset every day at midnight 
(French time - UTC+2) 

Download speed also increase with participation 
to database or with donations.

All is detailed at : 
https://www.screenscraper.fr/faq.php

-----------------------------------



EOF

press_a "Press A to continue"
clear
}

##########################################################################################

# This game's media (zazzagone): its cover (Imgs), its Game Info media
# (media/<kind>, "-1", "-2"... screenshots included) and its manual
# (Manuals), named after the ROM. Names are compared, not used as
# patterns: "[!]" and "*" in a ROM name match only themselves.
Game_Media_Files ()
{
    _gm_root="${ROMS_DIR:-/mnt/SDCARD/Roms}/$CurrentSystem" # ROMS_DIR: tests
    [ -n "$romNameNoExtension" ] || return 0
    for _gm_dir in "$_gm_root/Imgs" "$_gm_root/Manuals" "$_gm_root"/media/*; do
        [ -d "$_gm_dir" ] || continue
        for _gm_f in "$_gm_dir"/*; do
            [ -f "$_gm_f" ] || continue
            _gm_n=${_gm_f##*/}
            _gm_n=${_gm_n%.*}
            if [ "$_gm_n" = "$romNameNoExtension" ]; then
                echo "$_gm_f"
                continue
            fi
            _gm_s=${_gm_n#"$romNameNoExtension"-}
            [ "$_gm_s" != "$_gm_n" ] || continue
            case $_gm_s in
                '' | *[!0-9]*) ;;
                *) echo "$_gm_f" ;;
            esac
        done
    done
}

# This game's media as its console's game lists name them (gamelist.xml,
# miyoogamelist.xml: <image>, <thumbnail>, <marquee>, <video>, <manual> of
# the <game> whose <path> is this ROM), whatever their names and folders:
# lists made with Skraper or ES-DE point at "./images/<name>-image.png",
# "./videos/...", "./manuals/..." (Quack Walks). Only existing files inside
# the console folder, and not one another game's entry names too.
Game_Listed_Media ()
{
    _gl_root="${ROMS_DIR:-/mnt/SDCARD/Roms}/$CurrentSystem" # ROMS_DIR: tests
    _gl_rel=${romPath#*/Roms/"$CurrentSystem"/}
    [ -n "$CurrentSystem" ] && [ -n "$romPath" ] && [ "$_gl_rel" != "$romPath" ] || return 0
    for _gl_x in "$_gl_root/miyoogamelist.xml" "$_gl_root/gamelist.xml"; do
        [ -f "$_gl_x" ] && cat "$_gl_x"
    done | awk -v rel="$_gl_rel" -v abs="/mnt/SDCARD/Roms/$CurrentSystem/" -v q="'" -v dq='"' '
        function unent(v, e, c,    i, out) {
            out = ""
            while ((i = index(v, e)) > 0) {
                out = out substr(v, 1, i - 1) c
                v = substr(v, i + length(e))
            }
            return out v
        }
        function value(v) {
            sub(/^<[a-z]*>/, "", v)
            v = unent(v, "&apos;", q)
            v = unent(v, "&#39;", q)
            v = unent(v, "&quot;", dq)
            v = unent(v, "&lt;", "<")
            v = unent(v, "&gt;", ">")
            v = unent(v, "&amp;", "&")
            sub(/^\.\//, "", v)
            # the same file written from the card root
            if (index(v, abs) == 1) v = substr(v, length(abs) + 1)
            return v
        }
        # games[m]: how many games name m (a game listed twice, in both
        # lists, counts once)
        function end_game(    i, key) {
            if (path != "")
                for (i = 1; i <= n; i++) {
                    key = media[i] SUBSEP path
                    if (!(key in by)) {
                        by[key] = 1
                        games[media[i]]++
                    }
                    if (path == rel) mine[media[i]] = 1
                }
            path = ""; n = 0
        }
        {
            line = $0
            while (match(line, /<\/game>|<game[ >\/]|<(path|image|thumbnail|marquee|video|manual)>[^<]*</)) {
                t = substr(line, RSTART, RLENGTH)
                line = substr(line, RSTART + RLENGTH)
                if (t == "</game>") end_game()
                else if (t ~ /^<game/) { path = ""; n = 0 }
                else if (t ~ /^<path>/) path = value(substr(t, 1, length(t) - 1))
                else { v = value(substr(t, 1, length(t) - 1)); if (v != "") media[++n] = v }
            }
        }
        END {
            for (m in mine)
                if (games[m] == 1) print m
        }' | while IFS= read -r _gl_m; do
        case $_gl_m in
            /*) continue ;; # outside the console folder (awk made the inside ones relative)
        esac
        case /$_gl_m/ in
            */../*) continue ;;
        esac
        [ -f "$_gl_root/$_gl_m" ] && echo "$_gl_root/$_gl_m"
    done
}

# For the main menu, quick: the names the scrapers write (cover .png, Game
# Info .png/.mp4/.pdf, manual .pdf), without listing whole folders, which
# can hold thousands of files. Game_Media_Files finds everything.
Game_Has_Media ()
{
    _gh_root="${ROMS_DIR:-/mnt/SDCARD/Roms}/$CurrentSystem" # ROMS_DIR: tests
    [ -n "$romNameNoExtension" ] || return 1
    [ -f "$_gh_root/Imgs/$romNameNoExtension.png" ] && return 0
    [ -f "$_gh_root/Manuals/$romNameNoExtension.pdf" ] && return 0
    for _gh_dir in "$_gh_root"/media/*; do
        [ -d "$_gh_dir" ] || continue
        for _gh_ext in png mp4 pdf; do
            [ -f "$_gh_dir/$romNameNoExtension.$_gh_ext" ] && return 0
        done
    done
    # media a game list names (Skraper, ES-DE layouts)
    [ -n "$(Game_Listed_Media)" ] && return 0
    return 1
}

Delete_Game_Media ()
{
    list=/tmp/game_media_list
    # By name, and as the game lists name them: once each.
    { Game_Media_Files; Game_Listed_Media; } | awk '!seen[$0]++' > "$list"
    count=$(grep -c . "$list")
    clear
    if [ "$count" -eq 0 ]; then
        echo -e "No cover or media for\n$romNameNoExtension.\n"
        rm -f "$list"
        press_a "Press A to continue"
        Menu_Main
        return
    fi
    echo -e "$count files of $romNameNoExtension:\n"
    head -n 12 "$list" | sed "s|^${ROMS_DIR:-/mnt/SDCARD/Roms}/$CurrentSystem/||"
    [ "$count" -gt 12 ] && echo "... and $((count - 12)) more"
    echo
    press_a "Press A to continue"

    confirm=$( echo -e "Delete $count files\nCancel" | /mnt/SDCARD/.tmp_update/script/shellect.sh -t "--== COVER AND MEDIA ==--" -b "$FootValidate")
    clear
    if [ "$confirm" = "Delete $count files" ]; then
        deleted=0
        while IFS= read -r f; do
            rm -f "$f" && deleted=$((deleted + 1))
        done < "$list"
        sync
        echo -e "$deleted files deleted.\n"
        press_a "Press A to continue"
    fi
    rm -f "$list"
    Menu_Main
}


##########################################################################################

# Media of deleted games (clean_media.sh): what would go is listed first, with
# its size, and is deleted only after confirmation. $1: one console folder, or
# "all" for every console.
Clean_Media ()
{
    clean=/mnt/SDCARD/.tmp_update/script/scraper/clean_media.sh
    if [ "$1" = "all" ]; then
        scope="all consoles"
        set -- /mnt/SDCARD/Roms/*/
    else
        scope=$(basename "$1")
    fi
    clear
    echo "Looking for media of deleted games ($scope)..."
    list=/tmp/clean_media_list
    sh "$clean" "$@" > "$list.all"
    grep -v "^skip: " "$list.all" > "$list"
    # Consoles not checked: their games could not all be read (SD card error)
    unread=$(grep -c "^skip: .* (read error)$" "$list.all")
    rm -f "$list.all"
    count=$(grep -c . "$list")
    unread_note=
    [ "$unread" -gt 0 ] && unread_note="Consoles not checked (SD card read error): $unread\n"

    clear
    if [ "$count" -eq 0 ]; then
        echo -e "No media of deleted games found\n($scope).\n"
        [ -n "$unread_note" ] && echo -e "$unread_note"
        rm -f "$list"
        press_a "Press A to continue"
        Menu_Main
        return
    fi

    bytes=0
    while IFS= read -r f; do
        b=$(wc -c < "$f" 2> /dev/null)
        bytes=$((bytes + ${b:-0}))
    done < "$list"
    size=$(awk -v b="$bytes" 'BEGIN { printf "%.1f MB", b / 1048576 }')

    echo -e "$count files of deleted games ($size):\n"
    head -n 8 "$list" | sed 's|^/mnt/SDCARD/Roms/||'
    [ "$count" -gt 8 ] && echo "... and $((count - 8)) more"
    echo -e "\nCovers (Imgs), Game Info media and manuals\nof games no longer on the SD card.\n"
    [ -n "$unread_note" ] && echo -e "$unread_note"
    press_a "Press A to continue"

    confirm=$( echo -e "Delete $count files ($size)\nCancel" | /mnt/SDCARD/.tmp_update/script/shellect.sh -t "--== MEDIA OF DELETED GAMES ==--" -b "$FootValidate")
    clear
    if [ "$confirm" = "Delete $count files ($size)" ]; then
        echo "Deleting..."
        deleted=$(sh "$clean" --delete "$@" | grep -vc "^skip: ")
        clear
        echo -e "$deleted files deleted.\n"
        press_a "Press A to continue"
    fi
    rm -f "$list"
    Menu_Main
}


##########################################################################################

# One line per favorite game, "<console folder><TAB><ROM file name>", from
# favourite.json ($1, one JSON object per line); apps and other entries
# outside Roms are left out.
Favorites_List ()
{
    jq -r '.rompath // empty' "$1" 2> /dev/null |
        awk -F'/Roms/' 'NF > 1 { n = split($NF, p, "/"); if (n >= 2 && p[n] != "") print p[1] "\t" p[n] }'
}

# Before a batch (all favorites, all consoles): Wi-Fi, the configuration
# and the sources turned on, as Launch_Scraping checks them for one console.
Batch_Prepare ()
{
    touch /tmp/stay_awake
    rm -f /tmp/scraper_script.sh

    if [ "$(ip r)" = "" ]; then
        clear
        echo -e "You must be connected to Wi-Fi to use the scraper.\n"
        press_a "Press A to continue"
        exit
    fi
    if [ ! -f "$ScraperConfigFile" ]; then
        clear
        echo -e "Error: configuration file not found.\n"
        press_a "Press A to continue"
        exit 1
    fi
    config=$(cat "$ScraperConfigFile")
    Screenscraper_enabled=$(echo "$config" | jq -r '.Screenscraper_enabled')
    Launchbox_enabled=$(echo "$config" | jq -r '.Launchbox_enabled')
    Retroarch_enabled=$(echo "$config" | jq -r '.Retroarch_enabled')
    ScrapeInBackground=$(echo "$config" | jq -r '.ScrapeInBackground')
    sdir=/mnt/SDCARD/.tmp_update/script/scraper
}

# Scrapes every favorite game, each in its own console, with the sources
# turned on in Configuration (as "Scrape current rom" does for one game).
Scrape_Favorites ()
{
    fav=/mnt/SDCARD/Roms/favourite.json
    list=/tmp/scrape_favorites_list
    clear
    Favorites_List "$fav" > "$list"
    count=$(grep -c . "$list")
    if [ "$count" -eq 0 ]; then
        echo -e "No favorite games to scrape.\n"
        rm -f "$list"
        press_a "Press A to continue"
        Menu_Main
        return
    fi

    Batch_Prepare
    n=0
    tab=$(printf '\t')
    # The list on fd 3: the scrapers read the keys on stdin.
    while IFS="$tab" read -r fsys from <&3; do
        n=$((n + 1))
        fimage="/mnt/SDCARD/Roms/$fsys/Imgs/${from%.*}.png"
        # Shown by the scrapers under their title (they clear the screen)
        export SCRAPER_PROGRESS="=== Favorite $n/$count: $from ($fsys) ==="
        for src in screenscraper launchbox retroarch; do
            case $src in
                screenscraper) [ "$Screenscraper_enabled" = "true" ] || continue ;;
                launchbox) [ "$Launchbox_enabled" = "true" ] || continue ;;
                retroarch) [ "$Retroarch_enabled" = "true" ] || continue ;;
            esac
            if [ "$ScrapeInBackground" = "true" ]; then
                echo "$sdir/scrap_$src.sh $fsys \"$from\"" >> /tmp/scraper_script.sh
            else
                "$sdir/scrap_$src.sh" "$fsys" "$from"
                # The account question once, not for every game
                export SCRAPER_NO_ACCOUNT_PROMPT=1
                # Cover found: the next sources are not needed for this game
                [ -f "$fimage" ] && break
            fi
        done
    done 3< "$list"
    rm -f "$list"

    if [ "$ScrapeInBackground" != "true" ]; then
        echo -e "\n$count favorite games done.\n"
        press_a "Press A to exit"
    fi
    rm -f /tmp/stay_awake
    close_terminal
}


# The consoles the scraper knows (the names in scrap_screenscraper.sh's
# get_ssSystemID) that hold at least one game, one per line, from the Roms
# folder ($1).
Consoles_List ()
{
    known=$(sed -n '/^get_ssSystemID() {/,/^}/p' \
        "${SCRAPER_DIR:-/mnt/SDCARD/.tmp_update/script/scraper}/scrap_screenscraper.sh" |
        sed -n 's/^ *\([A-Za-z0-9_]*\)).*/\1/p')
    for cdir in "$1"/*/; do
        [ -d "$cdir" ] || continue
        csys=$(basename "$cdir")
        # A whole line of the list (not grep -xF: a substring search in
        # BusyBox 1.20.2)
        case "
$known
" in
            *"
$csys
"*) ;;
            *) continue ;;
        esac
        find "$cdir" -maxdepth 2 -type f ! -name '.*' ! -name '*.xml' \
            ! -name '*.xml.*' ! -name '*.db' ! -name '*.cfg' ! -name '*.miyoocmd' \
            ! -name '*.json' ! -path '*/Imgs/*' ! -path '*/media/*' ! -path '*/.*/*' 2> /dev/null |
            grep -q . && echo "$csys"
    done
}

# Scrapes every console with games, one after the other, with the sources
# turned on in Configuration (as "Scrape all ... roms" does for one).
Scrape_All_Consoles ()
{
    list=/tmp/scrape_consoles_list
    clear
    echo "Looking for consoles with games..."
    Consoles_List /mnt/SDCARD/Roms > "$list"
    count=$(grep -c . "$list")
    clear
    if [ "$count" -eq 0 ]; then
        echo -e "No consoles with games to scrape.\n"
        rm -f "$list"
        press_a "Press A to continue"
        Menu_Main
        return
    fi
    confirm=$( echo -e "Scrape $count consoles (may take hours)\nCancel" | /mnt/SDCARD/.tmp_update/script/shellect.sh -t "--== SCRAPE ALL CONSOLES ==--" -b "$FootValidate")
    if [ "$confirm" != "Scrape $count consoles (may take hours)" ]; then
        rm -f "$list"
        Menu_Main
        return
    fi
    clear

    Batch_Prepare
    n=0
    # The list on fd 3: the scrapers read the keys on stdin.
    while read -r csys <&3; do
        n=$((n + 1))
        # Shown by the scrapers under their title (they clear the screen)
        export SCRAPER_PROGRESS="=== Console $n/$count: $csys ==="
        for src in screenscraper launchbox retroarch; do
            case $src in
                screenscraper) [ "$Screenscraper_enabled" = "true" ] || continue ;;
                launchbox) [ "$Launchbox_enabled" = "true" ] || continue ;;
                retroarch) [ "$Retroarch_enabled" = "true" ] || continue ;;
            esac
            if [ "$ScrapeInBackground" = "true" ]; then
                echo "$sdir/scrap_$src.sh $csys \"\"" >> /tmp/scraper_script.sh
            else
                "$sdir/scrap_$src.sh" "$csys" ""
                # The account question once, not for every console
                export SCRAPER_NO_ACCOUNT_PROMPT=1
            fi
        done
    done 3< "$list"
    rm -f "$list"

    if [ "$ScrapeInBackground" != "true" ]; then
        echo -e "\n$count consoles done.\n"
        press_a "Press A to exit"
    fi
    rm -f /tmp/stay_awake
    close_terminal
}


##########################################################################################

Menu_Main ()

{
clear
Option1="Scrape all $(basename "$CurrentSystem") roms"
Option2="Scrape current rom: $romname"
# Only when the game has a cover or other media
Game_Has_Media && Option3="Delete cover and media: $romNameNoExtension" || Option3=""
Option4="Configuration"
Option5="Exit"
Option6="Delete media of deleted games: $(basename "$CurrentSystem")"
# Opened from the Favorites tab (game_list_options.sh: glo_tab 2): the
# favorites are the list this game is in.
[ "$glo_tab" = 2 ] && Option8="Scrape all favorites" || Option8=""

# This game and its list only (zazzagone). The long jobs over many games
# (all favorites, all consoles, media of all consoles) are in
# Configuration. "Delete cover and media" only with some: an empty item
# matched an empty choice (MENU).
clear
Mychoice=$(
    {
        echo "$Option2"
        [ -n "$Option3" ] && echo "$Option3"
        echo "$Option1"
        [ -n "$Option8" ] && echo "$Option8"
        echo "$Option6"
        echo "$Option4"
        echo "$Option5"
    } | /mnt/SDCARD/.tmp_update/script/shellect.sh -t "--== MAIN MENU ==--" -b "$FootValidate"
)

# One case: the menus called here set Option* and Mychoice too.
case "$Mychoice" in
    "") ;;
    "$Option1") (onerom=0; Launch_Scraping;) ;;
    "$Option2") (onerom=1; Launch_Scraping;) ;;
    "$Option3") Delete_Game_Media ;;
    "$Option8") Scrape_Favorites ;;
    "$Option6") Clean_Media "/mnt/SDCARD/Roms/$CurrentSystem" ;;
    "$Option4") Menu_Config ;;
    "$Option5") exit ;;
esac


}


/mnt/SDCARD/.tmp_update/script/scraper/config_repair.sh &
Menu_Main

