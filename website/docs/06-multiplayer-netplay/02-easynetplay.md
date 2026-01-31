---
slug: /multiplayer/easynetplay
---


# Easy Netplay

*![](https://github.com/OnionUI/Onion/assets/47260768/031e60fa-e6dd-4059-9982-3ec397a3d0cd)*

*Challenge your friends or team up with them — the easy way!*

**Easy Netplay** is the ultimate convenience feature for local multiplayer gaming. Play with all nearby Miyoo Mini Plus devices without any Wi-Fi network or RetroArch configuration whatsoever!

This intelligent tool automatically sets up a personal hotspot, launches RetroArch, and enables Netplay on the host device. On the client side, it seamlessly joins the hotspot, verifies ROM and core checksums for compatibility, and connects to the RetroArch session — ensuring a smooth, hassle-free multiplayer experience from start to finish.

**Important:** When you're done playing, make sure to exit RetroArch through the menu (don't just power down). The script will then automatically reconnect you to your original Wi-Fi network.

## Features

- **1-click host setup** — Start hosting instantly
- **1-click client setup** — Join games effortlessly
- **ROM/core checksum verification** — Ensures perfect compatibility between devices

## Using Easy Netplay

You'll find the Easy Netplay feature in the GLO menu. Simply browse to the game you want to play and press <kbd>Y</kbd> to access it.

### As the host

1. Find your game in the Games menu
2. Press <kbd>Y</kbd> to open GLO
3. Choose `Netplay` → `Host a session...` → `Easy Netplay (Play anywhere, local only)`
 
Onion will now take over and automatically set up a session using the built-in personal hotspot — just wait for your friends to connect!

### As the client

Joining a host session is even simpler! You'll use the same GLO menu process:

1. Navigate to any game in the Games menu
2. Press <kbd>Y</kbd> to open GLO
3. Choose `Netplay` → `Join a session...` → `Easy Netplay (Play anywhere, local only)`
 
Onion will automatically join the hotspot, retrieve the host information, and connect you to the game session! 


## Example

### Host: 

![](https://github.com/OnionUI/Onion/assets/47260768/e319297d-d65d-4060-9fa0-174d9c3b4516)

### Client:

![](https://github.com/OnionUI/Onion/assets/47260768/4d6bb983-e986-47b6-8810-17cd9e15f553)

:::note Important
If you change the hotspot password on the host device, the Easy Netplay connection process will fail. The password should be left as the default: `onionos+`
:::
