# This script moves incompatible AdvanceMame roms to "_not_compatible" Folder
# It checks the presence of the rom in the file : RApp\advancemame\.advance\AdvanceMAME.xml
# Credits : Schmurtz - Onion Team


$RomsFolder = (get-location).Drive.Name + ":\Roms\ADVMAME"
$notCompatibleDirectory = (get-location).Drive.Name + ":\Roms\ADVMAME\_not_compatible"
$xmlFile = 	  (get-location).Drive.Name + ":\RApp\advancemame\.advance\AdvanceMAME.xml"


New-Item -ItemType Directory -Path $notCompatibleDirectory -Force

$xml = [xml](Get-Content $xmlFile)

# Index the machine/rom names declared in the XML once.
# Scope to <game> elements only: other elements (e.g. <biosset name="...">) also carry a
# "name" attribute with a different meaning and would falsely mark roms as compatible.
$known_roms = @{}
foreach ($node in $xml.SelectNodes("//game[@name]")) {
    $known_roms[$node.GetAttribute("name")] = $true
}

$files = Get-ChildItem $RomsFolder -File -Filter *.zip

foreach ($file in $files) {
    $fileName = $file.BaseName

    if (!$known_roms.ContainsKey($fileName)) {
        Write-Host "File $fileName not found in $xmlFile."
		Move-Item $file.FullName  -Destination $notCompatibleDirectory
    } else {
        # Write-Host "File $fileName found in $xmlFile."
    }
}