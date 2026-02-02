<p>&nbsp;</p>

# <img alt="Onion" src="https://user-images.githubusercontent.com/44569252/179510333-40793fbc-f2a3-4269-8ab9-569b191d423f.png" width="196px">

*An enhanced operating system for your Miyoo Mini and Mini+, featuring fine-tuned emulation with 100+ built-in emulators, auto-save and resume, a wealth of customization options, and much more. Performant, reliable, and straightforward retro gaming right in your pocket.*

<p>&nbsp;</p>

<p align="center">
<a href="https://onionui.github.io/docs"><img alt="Getting Started" src="https://user-images.githubusercontent.com/44569252/190487908-0fb16c8e-5ff5-4ee2-921a-0a9427f26587.png"></a>
<a href="https://onionui.github.io/docs/features"><img alt="Features" src="https://user-images.githubusercontent.com/44569252/190487893-7a4a2287-462a-4d91-a4fc-ace80580653a.png"></a>
<a href="https://onionui.github.io/docs/faq"><img alt="FAQ" src="https://user-images.githubusercontent.com/44569252/190487922-3d6b8df9-da26-47e8-b397-e4104156ede6.png"></a>
</p>

<p align="center">
<a href="https://github.com/OnionUI/Themes/blob/main/README.md"><img alt="Getting Started" src="https://user-images.githubusercontent.com/44569252/226488035-e321e466-87c3-431f-bc81-52eb6a33c225.png"></a>
<a href="https://onionui.github.io/docs/ports"><img alt="Features" src="https://user-images.githubusercontent.com/44569252/228782816-cd9c479f-4c46-46ba-abd5-42158d19de7b.png"></a>
</p>


<p>&nbsp;</p>

<p align="center"><a href="https://onionui.github.io/docs/features"><img src="https://user-images.githubusercontent.com/44569252/226488511-297034e2-bb69-4f87-bd18-2ae6ff1e7300.gif"></a></p>

<p align="right"><sub><i>Icons by <a href="https://icons8.com" target="_blank">Icons8</a></i></sub></p>

---

## 🛠️ For Developers

### Building Onion

Onion is optimized for fast compilation on both development machines and the embedded Miyoo Mini+ hardware:

```bash
# Clone the repository
git clone https://github.com/OnionUI/Onion.git
cd Onion

# Build with parallel compilation (recommended)
make -j$(nproc)

# Or specify job count
make -j4
```

### Build System Optimizations

The build system has been optimized for the Miyoo Mini+ ARM platform:

- **🚀 Parallel Builds:** Utilizes all CPU cores for 60-75% faster compilation
- **📁 Efficient I/O:** Consolidated file operations reduce SD card access
- **🔧 Smart Scripts:** O(n) algorithms replace O(n²) bottlenecks

For detailed build optimization information, see [BUILD_OPTIMIZATION.md](BUILD_OPTIMIZATION.md)

### Security & Code Quality

The codebase has been analyzed for common C vulnerabilities:

- ✅ Fixed critical memory leaks and buffer overflows
- ✅ Added NULL pointer checks and bounds validation
- ✅ Eliminated file descriptor leaks

For the complete security analysis, see [SECURITY_ANALYSIS.md](SECURITY_ANALYSIS.md)

### Performance Considerations

Onion is designed for the resource-constrained Miyoo Mini+:
- **CPU:** ARM Cortex-A7 @ 1.2 GHz
- **RAM:** 64-128 MB
- **Storage:** MicroSD (variable speed)

Code optimizations focus on minimal memory footprint and efficient CPU usage.

