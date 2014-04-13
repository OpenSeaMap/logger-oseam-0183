#!/bin/bash
#
# oseamlog.sh
#
# Purpose:
#   o test utility for the openseamap logger
#   o SD card formatting for 512MB media to store a HEX file for the Arduino
#   o WARNING: using the wrong /dev/sdXX may destroy harddisk data!!!!
#
# Options:
#    help    -h      - get this help
#    com     -c <p>  - set com port TTY
#    test    -t      - output nmea data through tty port
#    dev     -d <d>  - SD-card device
#    save    -s      - save the contents of SD-card to a file
#    erase   -e      - erase whole SD card by overwriting with zeros 
#    init    -i      - init SD card by writing an image file
#    fat     -f      - create a new partition table and FAT16 file system
#    hex     -x      - write hex file
#    ls      -l      - list card contents, showing special infos for DAT files
#
# Usage examples: 
#   oseamlog.sh -d /dev/sdd -i    # initialize sdcard"
#   oseamlog.sh -f                # init + format sdcard"
#
# Background:
#   o for all SD card operations you need to run with sudo"
#   o you need to apt-get install gpsd package for testing
#   o see also:
#     https://github.com/willie68/OpenSeaMapLogger
#
# Author:
#   A.Merz
#
# Default settings: 
TTY=/dev/ttyS0        # com port to send out nmea messages
delay_s=0.02          # delay for nmea messages

SDDEV=/dev/sdc             # SDcard device
SDDEV1=${SDDEV}1           # first partition
SDIMAGE=image.dd.gz        # a virgin image with a clean DOS FAT16 file system
###


function security {
     echo
     echo "Attention: overwriting $SDDEV"
     if [ "`mount | grep $SDDEV`" ] ; then
       echo "to erase, we first need to unmount $SDDEV1 - proceed? (Ctrl-C to abort)"
       read proceed
       sudo umount $SDDEV1
     fi
     size=`sudo fdisk -l $SDDEV | grep " $SDDEV" | awk '{print $5}'`
     if [ $size -gt 17000000000 ] ; then
       echo "$SDDEV is a really HUGE disk, $size Bytes... really proceed? (Ctrl-C to abort)"
       read proceed
     fi
     if [ $size -gt 600000000 ] ; then
       echo "$SDDEV is more than 512MB $size Bytes... really proceed? (Ctrl-C to abort)"
       read proceed
     fi
     echo "ok, proceeding.."
}


echo
echo $0
echo default settings: SD card device=$SDDEV, com-port: $TTY

# time information and hash values
thash=`date +%s | awk '{printf("%08lx",$1-1000000000)}'`
today=`date +%F_%T`
tstmp=`echo $today | tr -d ':-'`
echo "today: $today    tstmp: $tstmp    hash: 0x$thash"




while [ true ] ; do
  cmd=$1

  case $cmd in
  
  -h|--help|help)
     echo
     # print header of this file
     sed -ne 1,/###$/p $0 | sed -e 's/^#/ /g' 
     exit
     ;;
     

  -t|test)
     echo "running logger test with faked nmea data"
    
     stty -F $TTY ispeed 4800 ospeed 4800

     if [ ! -f tmp.nmea ] ; then
       # extract test data
       cat 20130629_135830.nmea.gz | gunzip | tail -n +3 | sed -e 's/^.*: //' > tmp.nmea
     fi
     # send each new NMEA message after delay_s
     gpsfake -c $delay_s tmp.nmea &
     pid=$!
     echo "to stop everything, you may need to kill $pid"
     sleep 3
     # forward the gps data to the serial interface
     gpspipe -r | tee $TTY | grep "RMC\|DBT"

     ;;

  -c|com)
     if [ -c "$2" ] ; then
       TTY=$2                     # set com port, e.g. /dev/ttyUSB0
       echo "TTY=$TTY"
       shift
     fi
     ;;


  -d|device)
     if [ -b "$2" ] ; then
       SDDEV=$2                   # set new SDcard device
       SDDEV1=${SDDEV}1           # first partition
       echo "SDDEV=$SDDEV"
       shift
     fi
     ;;


  -s|save)
     echo "saving SD card image  .. this may take a while .."
     echo "dd if=$SDDEV bs=1M | gzip > sdcardimage_$tstmp.dd.gz"
     sudo dd if=$SDDEV bs=1M | gzip > sdcardimage_$tstmp.dd.gz
     echo "done"
     ;;
     
 
  -e|erase)
     security
     # erase the whole card
     echo "erasing $SDDEV... "
     sudo dd if=/dev/zero of=$SDDEV bs=1M 
     ;;


  -f|fat)
     security
     ftype=6
     if [ $size -gt 600000000 ] ; then
       echo "setting file type vfat, really proceed? (Ctrl-C to abort)"
       ftype=b
       mkfs_options="-r 4096"   # increase nr of root dir entries
       read proceed
     fi
     # erase the first 20MBytes
     echo "erasing $SDDEV... "
     sudo dd if=/dev/zero of=$SDDEV bs=1M  count=20

     # mit fdisk partition table anlegen, 
     # neue partition einrichten und typ FAT16 setzen
     echo "creating new partition with FAT16 file system type"
     echo "
     o
     
     n




     t

     $ftype
     w
     " \
     | sudo fdisk $SDDEV
     sudo fdisk -l $SDDEV

     sudo mkfs.vfat -v -n OPENSEAMAP $mkfs_options $SDDEV1
     sudo mount -t vfat $SDDEV1 /mnt
     sudo touch /mnt   # set actual date
     
     # C-program to change the disk identifier
     # http://www.linuxquestions.org/questions/linux-general-1/what-is-disk-identifier-740408/
     ;;
     
     
  -i|init)
     security
     echo "copying image $SDIMAGE to $SDDEV, please wait.."
     # oder ganz einfach vorbereitetes partition image aufspielen
     # und statt 0x805ea3ab neuen Disk identifier vergeben
     cat $SDIMAGE | gunzip | sudo  dd of=$SDDEV bs=1M
     echo "
     x
     i
     0x$thash

     w
     " \
     | sudo fdisk $SDDEV
     
     # verify
     sudo fdisk -l $SDDEV
     #sudo mkfs.vfat -n OPENSEAMAP  $SDDEV1
     sudo mount -t vfat $SDDEV1 /mnt
     ;;
     
     
  -x|hex)
     # we assume, that the latest arduino build was written to /tmp/buildXXX
     # otherwise, change newhex below
     echo 
     newdir=`ls -dtr /tmp/build* | tail -n 1`
     newhex=`find $newdir -name "OpenSeaMap*.hex"`
     sudo mount $SDDEV1 /mnt

     if [ -f /mnt/OSMFIRMW.HEX ] ; then
       echo "creating backup of /mnt/OSMFIRMW.HEX: OSMFIRMW_$tstmp.HEX.bak"
       cp -p /mnt/OSMFIRMW.HEX OSMFIRMW_$tstmp.HEX.bak
     fi
     echo

     if [ "$newhex" != "" ] ; then
       echo "copying $newhex to /mnt/OSMFIRMW.HEX"
       sudo cp $newhex /mnt/OSMFIRMW.HEX
     else
       echo "sorry, no hex file found in /tmp"
     fi
     diff -s -q /mnt/OSMFIRMW.HEX OSMFIRMW_$tstmp.HEX.bak
     ;;
     

  -l|ls)
     if [ "$2" == "" ] ; then
       # list card contents with time info comments
       sudo mount $SDDEV1 /mnt
       cd /mnt
     else
       ldir=$2
       cd $ldir
       shift
     fi
     echo
     echo DAT files contents of `pwd`
     echo
     
     # check for DAT files
     for i in  `ls -1 DATA*.DAT ` ; do
         
         # extract time and position info
         DATdateA="`grep ZDA $i | head -n 1`"
         DATdateZ="`grep ZDA $i | tail -n 1`"
         DATtimeA="`grep 'RMC.*,A' $i | head -n 2 | head -n 1`"
         DATtimeZ="`grep 'RMC.*,A' $i | tail -n 1`"
         DATstmpA="`head -n 1 $i | sed -e 's/;.*//'`"
         DATstmpZ="`tail -n 1 $i | sed -e 's/;.*//'`"
         
         RMCA="`echo $DATtimeA | awk -F ',' '{ print $10 \", \" $2}'`"
         RMCZ="`echo $DATtimeZ | awk -F ',' '{ print $2}'`"
         RMCP="`echo $DATtimeA | awk -F ',' '{ print \"\tpos: \" $4 \",\" $5 \", \" $6 \",\" $7}'`"

         #echo "`ls -l $i | awk '{ print $9 \"\tsize: \" $5 \"\tdate: \" }'` $RMCA - $RMCZ   $RMCP"
         
         # example output:
         #DATA0073.DAT    size:  1794306  01:00:01.475 - 02:00:00.769     date: 130414  131202 - 141201  pos: 4811.265,N, 01152.158,E
         #
         name_size="`ls -l $i | awk '{ print $9 \" \" $5 }'`"
         echo "$name_size $DATstmpA $DATstmpZ $RMCA $RMCZ $RMCP" \
          | awk '{ printf("%s\tsize: %8d \t%s - %s \tdate: %6d  %6d - %6d  %s %s %s\n", $1,$2,$3,$4,$5,$6,$7,$8,$9,$10); }'
         
         #echo "   $DATdateA"
         #echo "   $DATtimeA"
         #echo "   $DATdateZ"
         #echo "   $DATtimeZ"
         #echo
     done
       
     # list other files
     ls -l | grep -v "DATA.*DAT"
     cd -
     echo
     ;;
     
     
  *)
     echo
     echo "usage: $0 <command> [arg] .."
     echo "help:  $0 -h"
     ;;

  esac
  shift
  if [ "$1" == "" ] ; then break ; fi

done
sync

