#!/bin/bash

echo "[+] Compiling..."
make
if [ $? -ne 0 ]; then
    echo "[-] Error in build!"
    exit 1
fi

echo "[+] Installing to /usr/bin/xzlang and /usr/xzlib/..."
if [ "$EUID" -ne 0 ]; then
    echo "[!] Not running as root. Will attempt to use sudo..."
    sudo make install
else
    make install
fi

if [ $? -eq 0 ]; then
    echo "[+] Everything is OK! You can now use 'xzlang' command anywhere."
else
    echo "[-] Error during installation!"
    exit 1
fi