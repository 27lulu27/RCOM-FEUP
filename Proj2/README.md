# RCOM Project 2

## 1. FTP Downloader

Para testar:

```bash
$ gcc -Wall -o download clientTCP.c
$ ./download ftp://ftp.up.pt/pub/gnu/emacs/elisp-manual-21-2.8.tar.gz
$ ./download ftp://demo:password@test.rebex.net/readme.txt
$ ./download ftp://anonymous:anonymous@ftp.bit.nl/speedtest/100mb.bin

$ ./download ftp://rcom:rcom@ftp.netlab.fe.up.pt/README
$ ./download ftp://rcom:rcom@ftp.netlab.fe.up.pt/pipe.txt
$ ./download ftp://rcom:rcom@ftp.netlab.fe.up.pt/files/crab.mp4
```
