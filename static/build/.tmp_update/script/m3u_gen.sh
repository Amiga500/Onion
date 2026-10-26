# Credits: gotbletu (@gmail|twitter|youtube|github|lbry)
#          adapted by Onion Team (https://github.com/OnionUI/Onion)

count_m3u="/tmp/count_m3u"
# Overridable for the tests
ROMS_ROOT=${M3U_ROMS_ROOT:-/mnt/SDCARD/Roms}
CUE_GEN=${M3U_CUE_GEN:-/mnt/SDCARD/.tmp_update/script/cue_gen.sh}

# target files extension
EXT_INT="cue|gdi|chd|pbp|iso|dsk"

# Disc files in folder $1 whose name matches the glob $2 (case-insensitive)
# and ends with one of $EXT_INT. The extensions used to be appended to the
# glob as "*[$EXT_INT]", a character class: any name ending in one of those
# letters matched, so the box art (.png) of a "(Disc 1)" game was moved and
# listed as a disc (#310).
find_discs() {
  find "$1" -maxdepth 1 ! -iname '*.m3u' -type f -iname "$2" \( -iname '*.cue' -o -iname '*.gdi' \
    -o -iname '*.chd' -o -iname '*.pbp' -o -iname '*.iso' -o -iname '*.dsk' \)
}

# A literal title for a find glob: [ ] * ? and \ escaped. "Game [T-En]
# (Disc 1)" used [T-En] as a character class, so its discs were found but
# never moved or listed: an empty playlist (#310).
glob_escape() {
  printf '%s' "$1" | sed 's/[][*?\\]/\\&/g'
}

helpmsg() {
  echo "desc: create m3u playlist for multi disc games [$EXT_INT]"
  echo "    e.g Playstation, Sega CD/Mega CD, NeoGeo CD,"
  echo "    PC Engine CD/PC-FX, Amiga"
  echo ""
  echo "usage: ${0##*/} [options] [folder name]"
  echo ""
  echo "options:"
  echo "  -md  create multiple directories : one directory and one m3u playlist for each game"
  echo "      from"
  echo "          Novastorm (USA) (Disc 1).bin"
  echo "          Novastorm (USA) (Disc 1).cue"
  echo "          Novastorm (USA) (Disc 2).bin"
  echo "          Novastorm (USA) (Disc 2).cue"
  echo "          Metal Gear Solid (USA) (Disc 1) (Rev 1).chd"
  echo "          Metal Gear Solid (USA) (Disc 2) (Rev 1).chd"
  echo "      to"
  echo "          Novastorm (USA).m3u"
  echo "          Metal Gear Solid (USA) (Rev 1).m3u"
  echo "          /.Novastorm (USA)/Novastorm (USA) (Disc 1).bin"
  echo "          /.Novastorm (USA)/Novastorm (USA) (Disc 1).cue"
  echo "          /.Novastorm (USA)/Novastorm (USA) (Disc 2).bin"
  echo "          /.Novastorm (USA)/Novastorm (USA) (Disc 2).cue"
  echo "          /.Metal Gear Solid (USA) (Rev 1)/Metal Gear Solid (USA) (Disc 1) (Rev 1).chd"
  echo "          /.Metal Gear Solid (USA) (Rev 1)/Metal Gear Solid (USA) (Disc 2) (Rev 1).chd"
  echo ""
  echo "  -sd create a single \"multi-disc\" directory for all multidisk files"
  echo "      from"
  echo "          Heart of Darkness (USA) (Disc 1).bin"
  echo "          Heart of Darkness (USA) (Disc 1).cue"
  echo "          Heart of Darkness (USA) (Disc 2).bin"
  echo "          Heart of Darkness (USA) (Disc 2).cue"
  echo "          Lunar 2 - Eternal Blue Complete (USA) (Disc 1).chd"
  echo "          Lunar 2 - Eternal Blue Complete (USA) (Disc 2).chd"
  echo "          Lunar 2 - Eternal Blue Complete (USA) (Disc 3).chd"
  echo "      to"
  echo "          Heart of Darkness (USA).m3u"
  echo "          Lunar 2 - Eternal Blue Complete (USA).m3u"
  echo "          .multi-disc/Heart of Darkness (USA) (Disc 1).bin"
  echo "          .multi-disc/Heart of Darkness (USA) (Disc 1).cue"
  echo "          .multi-disc/Heart of Darkness (USA) (Disc 2).bin"
  echo "          .multi-disc/Heart of Darkness (USA) (Disc 2).cue"
  echo "          .multi-disc/Lunar 2 - Eternal Blue Complete (USA) (Disc 1).chd"
  echo "          .multi-disc/Lunar 2 - Eternal Blue Complete (USA) (Disc 2).chd"
  echo "          .multi-disc/Lunar 2 - Eternal Blue Complete (USA) (Disc 3).chd"
  echo ""
  echo "  -h, --help                 display this help message"
  echo ""
  echo "Swap Disc: Retroarch > [Load Your Game] > Quick Menu > Disc Control"
}

if [ "$1" = -h ] || [ "$1" = --help ]; then
  helpmsg
  exit 0
elif [ "$1" = -md ]; then
  TARGETFOLDER="-md"
elif [ "$1" = -sd ]; then
  TARGETFOLDER="-sd"
else # case for eventual GLO menu
  LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so prompt -t "Multi-disc - Target Directory" -m "Choose the sub folder name where\noriginal disc files will be stored." \
    "Single directory (\".multi-disc\")" \
    "Multiple directories (\".Game_Name\")"
  retcode=$?

  if [ $retcode -eq 0 ]; then
    TARGETFOLDER="-sd"
  elif [ $retcode -eq 1 ]; then
    TARGETFOLDER="-md"
  elif [ $retcode -eq 255 ]; then
    exit
  fi
fi

echo 0 >"$count_m3u"
# create missing cue files.
"$CUE_GEN"

if [ "$TARGETFOLDER" = -sd ]; then
  DIR_NAME=".multi-disc"
fi

DIR_LIST="PS SEGACD NEOCD PCE PCFX AMIGA"
for dir in $DIR_LIST; do
  full_path="$ROMS_ROOT/$dir"
  echo "============================= $full_path ============================="
  if [ -d "$full_path" ]; then
    cd "$full_path"
    ### TYPE 1: TitleOfGame (USA) (Disc 1).chd
    #---------------------------------------------------------------------------------
    find_discs . "*([Dd][Ii][Ss][KkCc] 1).*" | while read line; do
      # searching for a "Disc 1" file.
      FILE_NAME="$(echo "${line%.*}" | sed 's@./@@g' | sed 's@ ([Dd][Ii][Ss][KkCc] 1)@@g')"
      if [ "$TARGETFOLDER" = -md ]; then
        DIR_NAME=".$(echo "${line%.*}" | sed 's@./@@g' | sed 's@ ([Dd][Ii][Ss][KkCc] 1)@@g')"
        DIR_NAME="${DIR_NAME%"${DIR_NAME##*[![:space:]]}"}" # remove spaces at the end
      fi
      SEARCH_NAME="$(glob_escape "${line%.*}" | sed 's@./@@g' | sed 's@([Dd][Ii][Ss][KkCc] 1)@([Dd][Ii][Ss][KkCc] ?)@g')"
      mkdir -p "$DIR_NAME"
      echo -e "****************\nType 1: ${line}\n**** SEARCH_NAME : $SEARCH_NAME \n**** DIR_NAME : $DIR_NAME\n**** FILE_NAME : $FILE_NAME"
      count=$(cat "$count_m3u")
      count=$((count + 1))
      echo "$count" >"$count_m3u"
      sync
      # move matching files to directory
      find . -maxdepth 1 ! -iname '*.m3u' -type f -iname "$SEARCH_NAME.*" -exec mv -n -- '{}' "$DIR_NAME" \;
      # Create m3u playslist file
      find_discs "$DIR_NAME" "$SEARCH_NAME*.*" | sed -e 's@^\./@@' | sort >"$FILE_NAME".m3u
    done

    ### TYPE 2: AnotherTitleOfGame (USA) (Disc 1) (Rev 2).chd
    #---------------------------------------------------------------------------------
    find_discs . "*([Dd][Ii][Ss][KkCc] 1) *.*" | while read line; do
      # searching for a "Disc 1" file.
      FILE_NAME="$(echo "${line%.*}" | sed 's@./@@g' | sed 's@ ([Dd][Ii][Ss][KkCc] 1)@@g')"
      if [ "$TARGETFOLDER" = -md ]; then
        DIR_NAME=".$(echo "${line%.*}" | sed 's@./@@g' | sed 's@ ([Dd][Ii][Ss][KkCc] 1)@@g')"
        DIR_NAME="${DIR_NAME%"${DIR_NAME##*[![:space:]]}"}" # remove spaces at the end
      fi
      SEARCH_NAME="$(glob_escape "${line%.*}" | sed 's@./@@g' | sed 's@([Dd][Ii][Ss][KkCc] 1)@([Dd][Ii][Ss][KkCc] ?)@g')"
      echo -e "****************\nType 2: ${line}\n**** SEARCH_NAME : $SEARCH_NAME \n**** DIR_NAME : $DIR_NAME\n**** FILE_NAME : $FILE_NAME"
      count=$(cat "$count_m3u")
      count=$((count + 1))
      echo "$count" >"$count_m3u"
      sync
      mkdir -p "$DIR_NAME"
      # move matching files to directory
      find . -maxdepth 1 ! -iname '*.m3u' -type f -iname "$SEARCH_NAME.*" -exec mv -n -- '{}' "$DIR_NAME" \;
      # Create m3u playslist file
      find_discs "$DIR_NAME" "$SEARCH_NAME*.*" | sed -e 's@^\./@@' | sort >"$FILE_NAME".m3u
    done

    ### TYPE 3: AmstradMSXTitleOfGame (19xx)(Developer)(Disc 1 of 3).dsk
    #---------------------------------------------------------------------------------
    find_discs . "*([Dd][Ii][Ss][KkCc] 1 of ?).*" | while read line; do
      # searching for a "Disc 1" file.
      FILE_NAME="$(echo "${line%.*}" | sed 's@./@@g' | sed 's@([Dd][Ii][Ss][KkCc] 1 of .*)@@g')"
      if [ "$TARGETFOLDER" = -md ]; then
        DIR_NAME=".$(echo "${line%.*}" | sed 's@./@@g' | sed 's@([Dd][Ii][Ss][KkCc] 1 of .*)@@g')"
        DIR_NAME="${DIR_NAME%"${DIR_NAME##*[![:space:]]}"}" # remove spaces at the end
      fi
      SEARCH_NAME="$(glob_escape "${line%.*}" | sed 's@./@@g' | sed 's@([Dd][Ii][Ss][KkCc] 1 of@([Dd][Ii][Ss][KkCc] ? of@g')"
      # SEARCH_NAME="$(echo "${line%.*}" | sed 's@./@@g' | sed 's@([Dd][Ii][Ss][KkCc] 1)@([Dd][Ii][Ss][KkCc] ?)@g')"
      echo -e "****************\nType 3: ${line}\n**** SEARCH_NAME : $SEARCH_NAME \n**** DIR_NAME : $DIR_NAME\n**** FILE_NAME : $FILE_NAME"
      count=$(cat "$count_m3u")
      count=$((count + 1))
      echo "$count" >"$count_m3u"
      sync
      mkdir -p "$DIR_NAME"
      # move matching files to directory
      find . -maxdepth 1 ! -iname '*.m3u' -type f -iname "$SEARCH_NAME*.*" -exec mv -n -- '{}' "$DIR_NAME" \;
      # Create m3u playslist file
      find_discs "$DIR_NAME" "$SEARCH_NAME*.*" | sed -e 's@^\./@@' | sort >"$FILE_NAME".m3u
    done
  else
    echo "Directory $full_path does not exist."
  fi
done

if [ "$#" -eq 0 ]; then # when launched by GLO menu, without args.
  infoPanel --title "Multi-disc Creator" --message "$(cat "$count_m3u") playlist files created." --auto
fi
