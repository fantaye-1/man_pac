![demo](https://user-images.githubusercontent.com/5411/49317121-37648480-f4c1-11e8-9868-0bb42b4f0f6e.gif)

# Manpac LKM Project

This project focuses on a Linux Kernel Module that intercepts specific key interrupts (this is not a keylogger) in order to install system hooks if the Konami Code is entered: `Up-Up-Down-Down-Left-Right-Left-Right-B-A-Enter`.

Once the kernel-level system hooks are installed, looking up on a manual page for `pac`, that is running the command `man pac` will initiate a small video game of Manpac instead of looking up a manual entry.

In this game, our hero Manpac will try to catch all the ghosts to get rid of the Ghost processes flying on the screen. Ghost processes hide themselves while they are alive (or undead), but once Manpac collides and terminates the process, it will be visible in utilities such as `ps` or `top`.

### Prerequisites

*Linux Kernel version:* ~4.15. Other versions have not been tested.

American QWERTY keyboard with arrow keys.

The following Ubuntu packages are needed to compile this code:
* build-essential
* linux-headers-$(uname -r)
* libelf-dev
* libx11-dev
* libxext-dev

These can be installed with `sudo apt install build-essential linux-headers-$(uname-r) libelf-dev libx11-dev linxext-dev`

### How To Run

1. Compile the code using `make DEBUG=1`. This enables the build with debug symbols and messages printed to the kernel module log visible with `dmesg`.
2. Copy the necessary files and install the LKM by running `sudo make load`.
3. Input the Konami Code using the key combination: `Up-Up-Down-Down-Left-Right-Left-Right-B-A-Enter`.
4. You can track if you successfully entered the code and all other messages using `dmesg --follow`.
5. After the system hooks are installed, viewing manual pages with `man` will continue to work, but running `man pac` will run the game.
6. The video game will spawn four ghosts as well as our hero manpac to catch all the ghosts.
  * If you run `ps -A | grep ghost`, you'll notice that the Ghost color-coded process IDs listed in `man pac`'s terminal window do not show up until they are killed.
  * Manpac can be controlled using the arrow keys and when it collides with a Ghost process, it will the process.
  * Ghost processes can be killed directly using `kill` with their corresponding process IDs.
7. When all the ghosts were caught or ghost processes were killed, system behavior will be reverted into the state prior to entering Konami Code.
  * Entering the Konami Code again will allow you to repeat the game by running `man pac`.
8. To fully uninstall the kernel module, run `sudo rmmod konami`.

## Authors

* **Chase Colman**
* **Abrham Fantaye**
* **Youngsoo Kang**

See also the list of [contributors](https://github.com/fantaye-1/man_pac/contributors) who participated in this project.

## License

This project is licensed under the GPLv3 License - see the [LICENSE](LICENSE) file for details

## Acknowledgments

* This README.md template is excerpted from **PurpleBooth**'s work (https://gist.github.com/PurpleBooth/109311bb0361f32d87a2)
