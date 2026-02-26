# pacsat_telem

installation instructions for Bullseye for CubeSatSim (test on Bookworm TBD)

       sudo apt-get install -y libbsd-dev
       cd
       git clone https://github.com/alanbjohnston/pacsat_telem.git
       cd pacsat_telem/Debug
       make all
       sudo ./pacsat_telem -v -d /home/pi/PacSat/pacsat 

For ground station, install 0.46o or later. Download here: https://www.g0kla.com/pacsat/downloads/test/

Edit pi-pacsat.properties to set callsign to AMSAT

Overwrite existing spacecraft file - yes
