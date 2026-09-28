# This script moves roms without preview (mng or png) in "_RomsWithoutPreview" Folder
# Use this script only on Non-Merged romsets ! Otherwise some required game files will be missing.
# Credits : Schmurtz - Onion Team

#  Define the folders to be compared
$RomsFolder = (get-location).Drive.Name + ":\Roms\ADVMAME"
$SnapFolder = (get-location).Drive.Name + ":\Roms\ADVMAME\Snaps" 
$MovedFolder = (get-location).Drive.Name + ":\Roms\ADVMAME\_RomsWithoutPreview"

New-Item -ItemType Directory -Path $MovedFolder -Force

# Retrieve files in RomsFolder
$RomsFolder_files = Get-ChildItem $RomsFolder -Filter *.zip

# Read the Snaps folder only once, into a lookup set of preview names (without extension)
$preview_names = @{}
foreach ($file2 in (Get-ChildItem $SnapFolder)) {
    $preview_names[[System.IO.Path]::GetFileNameWithoutExtension($file2.Name)] = $true
}

# Loop for each zip file in rom folder
foreach ($file in $RomsFolder_files) {
    # Retrieve preview file name without extension
    $file_name_without_ext = [System.IO.Path]::GetFileNameWithoutExtension($file.Name)

    if (!$preview_names.ContainsKey($file_name_without_ext)) {
        Write-Host "Moving $file"
        Move-Item "$RomsFolder\$($file.Name)" -Destination $MovedFolder
    }
}