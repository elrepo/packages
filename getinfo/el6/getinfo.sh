#!/bin/bash
# getinfo.sh
# This script collects system hardware and software information.
# Exact information collected depends on how it is called.
# It was developed by CentOS Forum members and is released under
# the GNU General Public License, version 2 or later (GPL).
# It should work on most Enterprise Linux or Fedora distributions.
# Version 1.0 4/5/2011

# Set locale to English
export LC_ALL=en_US.UTF-8
export LANGUAGE=en_US:en_GB:en

PATH=/sbin:/bin:/usr/sbin:/usr/bin

# Separator for commands.
IFS=: 

# Temporary file for output.
TMPFILE="`mktemp -t basedata.XXXXXX`" || exit 1

# Command list:
PRGBASE="uname -rmi:rpm -qa \*-release\*:cat /etc/redhat-release:getenforce:free -m"
PRGPKGS="rpm -qa yum\* rpm-\* python | sort:ls /etc/yum.repos.d:cat /etc/yum.conf"
PRGPKGS=$PRGPKGS:"yum repolist all:egrep 'include|exclude' /etc/yum.repos.d/*.repo"
PRGPKGS=$PRGPKGS:'sed -n -e "/^\[/h; /priority *=/{ G; s/\n/ /; s/ity=/ity = /; p }" /etc/yum.repos.d/*.repo | sort -k3n'
PRGKRNL="rpm -qa kernel\\* | sort"
PRGHARD="lspci -nn:lsusb:rpm -qa kmod\* kmdl\*"
PRGSNET="ifconfig -a:brctl show:route -n:cat /etc/resolv.conf:grep net /etc/nsswitch.conf:chkconfig --list | grep -Ei 'network|wpa'"

if [ $# -lt 1 ]; then
    echo "No option provided.  Default to all information."
    CASE="all"
else
    CASE="$1"
fi

# If not running as root then no fdisk, parted, or lvdisplay
if [[ $EUID -eq 0 ]]; then
    PRGDISK="cat /etc/fstab:df -h:fdisk -l:parted -l:blkid:cat /proc/mdstat:pvs:vgs:lvs"
else
    PRGDISK="cat /etc/fstab:df -h:blkid:cat /proc/mdstat"
    echo ""
    echo "WARNING!!!"
    echo "Not running as root. No fdisk, parted, or LVM information will be provided!"
    echo ""
    if [ "$CASE" = "disk" ]; then
        echo "Not running as root. No fdisk, parted, or LVM information will be provided!!!" >> $TMPFILE 2>&1
    fi
fi

case "$CASE" in
  basic)
        PRGS="$PRGBASE"
        echo "Basic system information." >> $TMPFILE 2>&1
	;;
  disk)
        PRGS="$PRGBASE:$PRGDISK"
        echo "Information for disk problems." >> $TMPFILE 2>&1
        ;;
  driver)
        PRGS="$PRGBASE:$PRGKRNL:$PRGHARD"
        echo "Information for driver problems." >> $TMPFILE 2>&1
        ;;
  network)
        PRGS="$PRGBASE:$PRGHARD:$PRGSNET"
        echo "Information for network problems." >> $TMPFILE 2>&1
	;;
  package)
	PRGS="$PRGBASE:$PRGPKGS:$PRGKRNL"
        echo "Information for package problems." >> $TMPFILE 2>&1
	;;
  all)
	PRGS="$PRGBASE:$PRGPKGS:$PRGDISK:$PRGKRNL:$PRGHARD:$PRGSNET"
        echo "Information for general problems." >> $TMPFILE 2>&1
	;;
  *)
        echo "Usage: `basename $0` {all|basic|disk|driver|network|package}"
	echo "Option $CASE not recognized."
        exit 1
esac
echo "Collecting system information for $CASE questions."

echo "[code]" >> $TMPFILE 2>&1

for program in $PRGS 
do
   echo "== BEGIN $program ==" >> $TMPFILE 2>&1
   eval $program >> $TMPFILE 2>&1
   echo "== END   $program ==" >> $TMPFILE 2>&1
   echo >> $TMPFILE 2>&1
done

echo "[/code]" >> $TMPFILE 2>&1

chmod a+r $TMPFILE

echo -e "\n########################"
echo -e "Results are in the file:\n* $TMPFILE *"
echo -e "########################\n"

echo    "To provide this information in support of a question on fora,"
echo -e "mailing list, or IRC channel please post its contents to\n"
echo    "      http://pastebin.org/"
echo -e "\nor another similar public site of your choice."
echo    "Select a retention time longer than the default of 'a day'"
echo -e "and provide a link to the information in your post.\n"

echo    "Alternatively, post in-line, trimming to remove any unnecessary information"
echo    "for your topic.  For forum posts, please leave in the"
echo -e "[code] ... [/code] tags to preserve formatting.\n"

echo    "WARNING - Check the contents of the $TMPFILE file to ensure it does"
echo    "not contain any sensitive information such as public IP addresses or hostnames."
echo    "If necessary obfuscate such information before posting. For example:"
echo    "   146.xxx.xxx.41 or myhost.mydomain.net"
