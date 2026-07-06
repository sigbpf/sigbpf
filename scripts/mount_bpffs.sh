#!/bin/bash

cd /mydata/sigbpf

if [ $# -eq 0 ];then
	echo "$0 up/down"

elif [ $1 == "up" ];then
	if [ ! -d "dados" ]; then
	    mkdir dados
	    echo "Dir 'dados' created."
	else
	    echo "Dir 'dados' already exists."
	fi
	sudo mount -t bpf bpffs /sys/fs/bpf
	sudo mount --bind /sys/fs/bpf ./dados;

elif [ $1 == "down" ];then
	sudo umount /mydata/sigbpf/dados

fi
