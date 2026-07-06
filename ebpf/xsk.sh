#!/bin/bash

clang -g -O2 -target bpf -c xsk_kern.c
 
bpftool gen skeleton xsk_kern.o > xsk_kern.skel.h
