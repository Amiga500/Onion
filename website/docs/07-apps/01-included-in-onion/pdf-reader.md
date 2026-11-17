---
slug: /apps/pdf-reader
description: Green PDF reader by Steward-Fu
---

# PDF reader

## Presentation

Allows you to read PDF files, right on your device.

![](./assets/pdf-reader.png)

## Usage

### Controls

| Button | Function            |
| ------ | ------------------- |
| D-pad  | Navigate            |
| A      | Fit to width        |
| B      | Rotate              |
| X      | Zoom in             |
| Y      | Zoom out            |
| L1/R1  | Previous/Next page  |
| L2/R2  | Move by 10 pages    |
| Select | Mirror vertically   |
| Start  | Mirror horizontally |
| Menu   | Close file          |

### Scanned manuals

A scanned manual is heavy: Green decodes the whole scan of a page every time it shows it, so its pages turn slowly (several seconds per page for a big scan). The first time such a PDF is opened, RedOnion makes a light copy of it (each page as a smaller picture) in a `.light` folder next to it, and Green opens the copy from then on: the same manual then turns a page in about 0.3 seconds. The original PDF is not changed. PDFs that are mostly text are left as they are.

Making the copy takes about as long as showing every page once, so a big scanned manual can take a few minutes. The panel shows the time left; hold B to open the original right away instead, and the copy is offered again the next time.

To turn this off: Tweaks › Game lists › Game Info menu... › Lighter manuals (PDF).

## Advanced

Based on [Green](https://github.com/schandinat/green), compiled by Steward Fu.