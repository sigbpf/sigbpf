#!/bin/bash

SIGSHARED_PATH="/mydata/sigshared/"

if [ -z "$TMUX" ]; then
  #if [ -n "`tmux ls | grep spright`" ]; then
  if [ -n "`tmux ls | grep sigshared`" ]; then
    #tmux kill-session -t spright
    tmux kill-session -t sigshared && sudo rm ../../dados/* 
  fi
  tmux new-session -s sigshared -n demo "./set_tmux_master.sh $SIGSHARED_PATH"
fi
