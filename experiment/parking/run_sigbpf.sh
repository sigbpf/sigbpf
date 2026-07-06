#!/bin/bash

SIGBPF_PATH="/mydata/sigbpf/"

if [ -z "$TMUX" ]; then
  #if [ -n "`tmux ls | grep spright`" ]; then
  if [ -n "`tmux ls | grep sigbpf`" ]; then
    #tmux kill-session -t spright
    tmux kill-session -t sigbpf && sudo rm ../../dados/* 
  fi
  tmux new-session -s sigbpf -n demo "./set_tmux_master.sh $SIGBPF_PATH"
fi
