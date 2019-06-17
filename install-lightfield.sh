#!/bin/bash

#########################################################
##                                                     ##
##     No user-serviceable parts below this point.     ##
##                                                     ##
#########################################################

set -e

if [ "${UID}" != "0" ]
then
    echo This script must be run as root.
    exit 1
fi

function clear () {
    echo -ne "\x1B[0m\x1B[H\x1B[J\x1B[3J"
}

function blue-bar () {
    echo -e "\r\x1B[1;37;44m$*\x1B[K\x1B[0m" 1>&2
}

PRINTRUN_SRC=/home/lumen/Volumetric/printrun
LIGHTFIELD_SRC=/home/lumen/Volumetric/LightField
MOUNTMON_SRC=${LIGHTFIELD_SRC}/mountmon
USBDRIVER_SRC=${LIGHTFIELD_SRC}/usb-driver

if [ "$1" = "-q" ]
then
    VERBOSE=
    CHXXXVERBOSE=
else
    VERBOSE=-v
    CHXXXVERBOSE=-c
fi

clear

blue-bar • Building debugging version of set-projector-power
cd ${USBDRIVER_SRC}
[ -f set-projector-power ] && rm ${VERBOSE} -f set-projector-power
g++ -o set-projector-power -pipe -g -Og -D_DEBUG -std=gnu++1z -Wall -W -D_REENTRANT -fPIC dlpc350_usb.cpp dlpc350_api.cpp main.cpp -lhidapi-libusb

blue-bar • Building debugging version of Mountmon
cd ${MOUNTMON_SRC}
./rebuild -x

blue-bar • Building debugging version of LightField
cd ${LIGHTFIELD_SRC}
./rebuild -x

blue-bar • Creating any missing directories
[ ! -d /var/cache/lightfield/print-jobs     ] && mkdir ${VERBOSE} -p /var/cache/lightfield/print-jobs
[ ! -d /var/lib/lightfield/model-library    ] && mkdir ${VERBOSE} -p /var/lib/lightfield/model-library
[ ! -d /var/lib/lightfield/software-updates ] && mkdir ${VERBOSE} -p /var/lib/lightfield/software-updates
[ ! -d /var/log/lightfield                  ] && mkdir ${VERBOSE} -p /var/log/lightfield

chown ${CHXXXVERBOSE} -R lumen:lumen /var/cache/lightfield
chown ${CHXXXVERBOSE} -R lumen:lumen /var/lib/lightfield
chown ${CHXXXVERBOSE} -R lumen:lumen /var/log/lightfield

blue-bar • Installing files
install ${VERBOSE} -DT -m 644    apt-files/volumetric-lightfield.list            /etc/apt/sources.list.d/volumetric-lightfield.list
install ${VERBOSE} -DT -m 644    gpg/pubring.gpg                                 /etc/apt/trusted.gpg.d/volumetric-keyring.gpg
install ${VERBOSE} -DT -m 440    system-stuff/lumen-lightfield                   /etc/sudoers.d/lumen-lightfield
install ${VERBOSE} -DT -m 644    system-stuff/getty@tty1.service.d_override.conf /etc/systemd/system/getty@tty1.service.d/override.conf
install ${VERBOSE} -DT -m 644    system-stuff/lumen-bash_profile                 /home/lumen/.bash_profile
install ${VERBOSE} -DT -m 644    system-stuff/lumen-real_bash_profile            /home/lumen/.real_bash_profile
install ${VERBOSE} -DT -m 600    gpg/pubring.gpg                                 /home/lumen/.gnupg/pubring.gpg
install ${VERBOSE} -DT -m 600    gpg/trustdb.gpg                                 /home/lumen/.gnupg/trustdb.gpg
install ${VERBOSE} -DT -m 644    system-stuff/set-projector-power.service        /lib/systemd/system/set-projector-power.service
install ${VERBOSE} -DT -m 644    usb-driver/90-dlpc350.rules                     /lib/udev/rules.d/90-dlpc350.rules
install ${VERBOSE} -DT -m 755 -s build/lf                                        /usr/bin/lf
install ${VERBOSE} -DT -m 755 -s mountmon/build/mountmon                         /usr/bin/mountmon
install ${VERBOSE} -DT -m 755 -s usb-driver/set-projector-power                  /usr/bin/set-projector-power
install ${VERBOSE} -DT -m 644    stdio-shepherd/printer.py                       /usr/share/lightfield/libexec/stdio-shepherd/printer.py
install ${VERBOSE} -DT -m 755    stdio-shepherd/stdio-shepherd.py                /usr/share/lightfield/libexec/stdio-shepherd/stdio-shepherd.py
install ${VERBOSE} -DT -m 755    system-stuff/reset-lumen-arduino-port           /usr/share/lightfield/libexec/reset-lumen-arduino-port
install ${VERBOSE} -DT -m 644    system-stuff/99-waveshare.conf                  /usr/share/X11/xorg.conf.d/99-waveshare.conf
chmod 700 /home/lumen/.gnupg

cd ${PRINTRUN_SRC}
install ${VERBOSE} -DT -m 644    printrun/__init__.py                            /usr/share/lightfield/libexec/printrun/printrun/__init__.py
install ${VERBOSE} -DT -m 644    printrun/eventhandler.py                        /usr/share/lightfield/libexec/printrun/printrun/eventhandler.py
install ${VERBOSE} -DT -m 644    printrun/gcoder.py                              /usr/share/lightfield/libexec/printrun/printrun/gcoder.py
install ${VERBOSE} -DT -m 644    printrun/printcore.py                           /usr/share/lightfield/libexec/printrun/printrun/printcore.py
install ${VERBOSE} -DT -m 644    printrun/utils.py                               /usr/share/lightfield/libexec/printrun/printrun/utils.py
install ${VERBOSE} -DT -m 644    printrun/plugins/__init__.py                    /usr/share/lightfield/libexec/printrun/printrun/plugins/__init__.py
install ${VERBOSE} -DT -m 644    Util/constants.py                               /usr/share/lightfield/libexec/printrun/Util/constants.py

blue-bar • Configuring system
systemctl daemon-reload
systemctl enable getty@tty1.service
systemctl enable set-projector-power.service
systemctl set-default multi-user.target