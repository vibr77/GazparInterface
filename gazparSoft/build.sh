#!/bin/sh
#make clean
#echo "**************************** NEW BUILD ***********************************"
#make
echo "**************************** PROG ***********************************"
echo " -i ICSP, -E erase,"
minipro -p 'ATMEGA328P@DIP28' -E -y -i
minipro -p 'ATMEGA328P@DIP28' -c code -w ./.pio/build/ATmega328P_GAZPAR/firmware.hex -f ihex -e -y -i
minipro -p 'ATMEGA328P@DIP28' -c config -w fuses.cfg -y -e -i
