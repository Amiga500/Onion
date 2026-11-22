# SD Card Doctor

SD Card Doctor is a lightweight diagnostic utility for Onion / RedOnion on Miyoo Mini and Mini Plus.
It shows card capacity, volume label and mount status, checks important system files, performs full
read scans, tests storage speed, measures folder sizes, inspects FAT32 metadata, displays card
identifiers, identifies possible I/O errors, and offers optional reversible cleanup. Nothing is
formatted or automatically repaired.

**Version:** 0.6.3 | C / SDL 1.2.

## Tools

The main screen displays the FAT32 volume label (when available). The app looks for the
root-directory label first and falls back to the boot-sector label. `UNLABELED` means no valid label
was found; this does not imply damage.

- **Quick Sample:** prioritizes RedOnion/Onion boot files and directories based on the RedOnion
  runtime layout. It reads `runtime.sh`, its logging/network helpers, selected core scripts,
  configuration files, MainUI binaries and other named files in full (up to 64 MiB per file). It
  then samples `.tmp_update/script`, `bin`, `config`, `lib`, `res`, `startup`, `miyoo/app`,
  `miyoo/lib`, RetroArch, `Emu`, `RApp`, `App` and user folders. Additional small system scripts,
  launchers and configs are also read fully, within a 48 MiB budget. Large files receive a bounded
  read sample. Total limits remain 1,950 files and 15,600 directory entries. Missing optional paths
  are not treated as corruption. A successful read does not prove that the file contents match an
  official RedOnion release, or detect silent data corruption.

- **Deep Scan:** reads all accessible regular file contents (no ZIP CRC or ROM checksums). Records
  up to 96 failed paths in memory and counts any additional errors. Files are not changed.

- **Speed Test:** 64 MiB sequential read from a verified block device; optional confirmed 32 MiB
  write + `fsync` + readback verification + cleanup. Falls back to buffered I/O only when O_DIRECT
  is unsupported, not on actual media I/O errors. Buffered reads may be cached; speed is not a card
  rating.

- **Storage:** reads file metadata and totals logical sizes by top-level folder.

- **Safe Repair / Check FAT Status:** shows FAT[1] stored clean-shutdown and hard-error flags,
  FSInfo signature and known free cluster count. These stored flags do not necessarily reflect the
  current Linux mount state. The confusing live Linux boot status indicator is deliberately not
  displayed.

- **Safe Repair / Live FAT Check:** runs `fsck.fat -n` without writing anything. Requires a verified
  FAT32 block device, an installed checker and sufficient available RAM relative to the FAT size.
  The estimate models the peak FAT tables and per-cluster bookkeeping, retains a safety reserve, and
  supports older kernels that lack `MemAvailable`. If RAM is insufficient, the app shows estimated
  requirements and detected availability. Mounted checks may report transient issues. The full-width
  results page offers scrollable checker details, omits routine mount dirty-bit chatter and
  recognizes a backup boot-sector difference solely at byte 65 (the mounted dirty flag) without
  calling it corruption. Differences elsewhere remain visible and require offline verification.
  Unknown output is inconclusive, never a definitive corruption diagnosis. Use a computer with the
  card unmounted for reliable validation. Cancellation sends SIGTERM, then SIGKILL, then releases
  the UI after a bounded timeout if the kernel is stuck on I/O. If the old checker is still alive, a
  second Live FAT Check is blocked until it exits.

- **Safe Repair / Card Identity:** reads `name`, `manfid`, `serial` and `date` from
  `/sys/block/mmcblk*/device/` when available. Common manufacturer IDs are decoded to the
  manufacturer/controller maker (which may differ from the retail brand). MID `0xAD` is shown as
  Longsys / Amazon+ because Amazon Basics, Lexar, Raspberry Pi and other brands may share it. Values
  can be spoofed; the manufacturer lookup is not genuine-capacity certification.

- **Safe Repair / Review Cleanup:** manually select root-level `.DS_Store`, `._*`, signed abandoned
  test files, or known macOS metadata directories `.Spotlight-V100`, `.Trashes`, and `.fseventsd`
  for quarantine. Each move is confirmed. The original folder contents are preserved inside
  quarantine, subject to available storage and name collisions. Restore is separate. Other nested
  metadata is not yet included; no automated changes to ROMs, saves, BIOS, emulator configs or
  runtime folders.

- **FAT32 Repair:** provides offline instructions only. Neither `fsck.fat -a` nor manual dirty-bit
  clearing is offered on a mounted SD card.

## Results and reports

After Quick or Deep Scan, press **A** on the result page to inspect a scrollable list of up to 96
failed files (additional failures are counted), count suspicious root `FOUND.000` / `FSCK*.REC`
leftovers, and inspect historical kernel messages for MMC/FAT and I/O problems. Notices that the
volume was not properly unmounted are shown separately as unclean shutdowns, not as proof of
corruption. Kernel-log access may be unavailable, in which case the screen displays N/A. These log
entries may predate the scan; zero matches is not proof of perfect media health.

Press **A** again, review the confirmation and press **A** to explicitly save a small text report
under `/mnt/SDCARD/App/SDCardDoctor/reports/report-*.txt`. The `reports` directory is created on
demand after confirmation. Normally logs remain in RAM at `/tmp/sdcarddoctor.log`, with output
capped to about 128 KiB. The app never saves reports automatically.

On the **Live FAT Check** results screen, press **A** to review a confirmation, then press **A**
again to save `fat-report-*.txt` in the same `reports` directory. The footer explicitly says `A:
SAVE REPORT`. The saved report includes the checker output (up to its capture limit), exit code and
diagnostic indicators. It does not repair the card. Press **B** to return without saving.

## Controls

D-pad navigates, A selects/confirms, B backs out one screen or cancels the running operation to its
owning menu; B exits on the main screen after release. SELECT+START exits from every screen once
both are released and active operations have stopped or the FAT checker has timed out. If raw
`/dev/input/event0` input is unavailable, SDL fallback expects A=SPACE, B=LCTRL, SELECT=RCTRL; check
on your hardware. Long information screens wrap text rather than shortening it, with Up/Down
scrolling where needed. Button instructions follow the message.

## Safety and limitations

A battery percentage from `/tmp/percBat` is preferred when present, otherwise Linux power-supply
sysfs is used. Write tests are prevented below 25% reported battery or without at least 160 MiB
free. The Onion estimate is voltage based and may fluctuate, so charge the device before
maintenance.

No multi-gigabyte fake-capacity fill test or ZIP content verification is implemented. No filesystem
write-repair is enabled. Full repair requires an unmounted card and a separate maintenance
environment; user warnings alone do not make live repair safe. Preserve backups of games and saves.

## License

MIT license for application source; see `LICENSE` for included bitmap font attribution and terms. No
font files are distributed.
