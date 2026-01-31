---
slug: /multiplayer/standardnetplay
---

# Standard Netplay

*![](https://github.com/OnionUI/Onion/assets/47260768/031e60fa-e6dd-4059-9982-3ec397a3d0cd)*

*Challenge your friends to epic battles from anywhere in the world!*

**Standard Netplay** makes it incredibly easy to create and join RetroArch multiplayer sessions. Each game session you host is automatically available over both the internet and your local network, making it simple for friends near or far to join your game.

## Features

- **Effortless host setup** — Launch a multiplayer session in seconds
- **Optimized RetroArch configuration** — Pre-tuned settings for best performance
- **Automatic core selection** — Onion picks the best emulator core for Netplay
- **Performance boost** — CPU overclocking when needed for demanding games

## Using Standard Netplay

### As the host

1. Find your game in the Games menu
2. Press <kbd>Y</kbd> to open GLO
3. Choose `Netplay` → `Host a session...` → `Standard Netplay (use current Wi-Fi)`
 
Your Onion netplay session is now live and ready for friends to join — whether they're on your local home network or on the other side of the world!

### As the client

1. Select the same game as the host
2. Press <kbd>Y</kbd> to open GLO
3. Choose `Netplay` → `Join a session...` → `Standard Netplay (use current Wi-Fi)`
4. Press <kbd>MENU</kbd> + <kbd>SELECT</kbd> to display the RetroArch menu, navigate to Netplay, then click on:
	- `Refresh Netplay Host List` → to find hosted games over the internet
	- `Refresh Netplay Lan List` → to find hosted games on your local network
5. Select your target server from the list and connect!
 
---

## Example

### Host: 




import NetStdHost from './assets/Netplay - standard - host.mp4';

<video controls>
  <source src={NetStdHost}/>
</video>



### Client:

import NetStdJoin from './assets/Netplay - standard - join.mp4';

<video controls>
  <source src={NetStdJoin}/>
</video>


