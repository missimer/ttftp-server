ttftp-server
============

The Trivial Trivial File Transfer Protocol Server

Usage: 

ttftps [-p port_num] [-r path_root]

Defaults:
	port_num:  69
	path_root:  ./


What it is and why:

An easy to use/modify TFTP server.  The purpose of this program is to
be used along with the development of Boston University's Quest
operating system (http://www.cs.bu.edu/~richwest/quest.html).  For the
rapid (and remote) development of Quest we typically PXE boot it and
then use a TFTP server as our filesystem.  This works pretty well but
we currently use dnsmasq which has a max tftp block size of 1428
bytes.  This is too small for large files (since the whole point of
the tftp filesystem is for the "rapid" development of Quest).

Right now the features are very limited, it can only handle one client
at a time and can only handle read requests, and only the blksize
option.  These are the features we need for Quest.  I'll add more
features as they become necessary.

I'm putting this code here in the hope that it will be useful, but
with no gaurantee of its correctness.  In fact I can gaurantee that it
does not nor will it most likely ever support all of the TFTP
protocol, as it will only support the features necessary for its
purpose.

UPDATE: I will no longer be working on this as we now use a ramdisk
filesystem for the rapid development of Quest.
