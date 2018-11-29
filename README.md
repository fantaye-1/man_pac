# Manpac LKM Project

This project aims to build a LKM that will activate system hook up code if Konami Code is entered: `Up-Up-Down-Down-Left-Right-Left-Right-B-A-Enter`. Once the system hook up code is activated, it scans for an attempt that if user is trying to look up on a manual page for `pac`, or `man pac` command. If such attempt was made, it will initiate a small video game of Manpac, where our hero Manpac will try to catch all the ghosts to get rid of annoying ghosts flying in the screen.

### Prerequisites

Following packages are needed to compile this code:
* libelf-dev
* libx11-dev
* libxext-dev

### How To Run

* Compile code using `make`
* Install LKM by running command `sudo make load`. This will install LKM and copy necessray files.
* Hook up a system call by inputting Konami Code using keyboard: `Up-Up-Down-Down-Left-Right-Left-Right-B-A-Enter`. Note that this won't let user know if system hookup is successful.
* After system call hookup, attempting to open manual page for `pac`, or command `man pac` will initiate a mini video game.
* The video game will spawn four ghosts as well as our hero manpac to catch all the ghosts.
* When all the ghosts were caught or ghost processes were killed, the system will be reverted into the stat prior to entering Konami Code.
* When removing LKM is needed, run `make clean` to delete LKM as well as files.

## Authors

* **Chase Coleman**
* **Abrham Fantaye**
* **Youngsoo Kang**

See also the list of [contributors](https://github.com/your/project/contributors) who participated in this project.

## License

This project is licensed under the GPLv3 License - see the [LICENSE.md](LICENSE.md) file for details

## Acknowledgments

* This README.md template is excerpted from **PurpleBooth**'s work (https://gist.github.com/PurpleBooth/109311bb0361f32d87a2)
