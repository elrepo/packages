When asking questions on a forum, mailing list, or IRC, it is
important to provide correct and complete information about your
system.  Good general guidance can be found in the classic guide
"How To Ask Questions The Smart Way" by open source developer and
philosopher Eric S. Raymond.

http://www.catb.org/~esr/faqs/smart-questions.html

To aid in the process a script getinfo.sh has been developed to
collect system information for a variety of classes of questions.  The
script will save the information to a temporary file which can be
posted in-line, or for longer files saved to a site such as
http://pastebin.org/.  The script should work on RHEL, Fedora,
and closely related distributions.

All options can be run as a normal user but fdisk information can only
be provided if run as root. You should check the contents of the
output file to ensure it does not contain any sensitive information
such as public IP addresses or hostnames.

### Usage ###

1. Basic info - appropriate for any and all questions:

getinfo.sh basic

2. Disk/filesystem problems - Basic + full disk info - requires the
user to become root to run as this really needs "fdisk -l":

getinfo.sh disk

3. Hardware/driver/kernel problems - Basic + hardware and kernel
info:

getinfo.sh driver

4. Network problems - Basic + full network info:

getinfo.sh network

5. Package problems with yum, rpm, and repos - Basic + rpm, yum,
kernel:

getinfo.sh package

6. The kitchen sink - combines all of the above. Run as either root or
non-root, but won't get fdisk info unless root. This is the default:

getinfo.sh all

If necessary obfuscate sensitive information before posting.
For example:

   146.xxx.xxx.41 or myhost.mydomain.net

Note that it is not necessary to hide network information for private
subnets often assigned by DHCP servers using NAT in ranges:

10.0.0.0 – 10.255.255.255; 172.16.0.0 – 172.31.255.255; 192.168.0.0 –
192.168.255.255

Other tools for gathering more detailed system information include
dmidecode and sos, available in most distribution repositories.
Providing appropriate information will help you get a prompt and
accurate answer, and will help others to efficiently answer your
questions.
