#!/bin/bash

ARCH=$1
SIGBPF_PATH="/mydata/sigbpf/"

sudo sysctl -w net.ipv4.tcp_tw_reuse=1
ulimit -HSn 102400

if [ -z "$ARCH" ] ; then
  echo "Usage: $0 < sigbpf >"
  exit 1
fi

if [ -z "$TMUX" ]; then
  if [ -n "`tmux ls | grep sigbpf`" ]; then
    tmux kill-session -t sigbpf && sudo rm /mydata/sigbpf/dados/*
  fi
  tmux new-session -s sigbpf -n demo "./set_tmux_master.sh $ARCH $SIGBPF_PATH"
fi
