#!/bin/bash

SIGBPF_PATH="/mydata/sigbpf/"
ulimit -HSn 102400

if [ -z "$TMUX" ]; then
  if [ -n "`tmux ls | grep sigbpf`" ]; then
    tmux kill-session -t sigbpf && sudo rm /mydata/sigbpf/dados/*
  fi
  tmux new-session -s sigbpf -n demo "./set_tmux_master.sh $SIGBPF_PATH"
fi
