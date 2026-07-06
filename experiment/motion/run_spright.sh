#!/bin/bash

#SPRIGHT_PATH="/mydata/spright/"
SIGSHARED_PATH="/mydata/sigshared/"

if [ -z "$TMUX" ]; then
  #if [ -n "`tmux ls | grep spright`" ]; then
  if [ -n "`tmux ls | grep sigshared`" ]; then
    tmux kill-session -t sigshared 
  fi
  tmux new-session -s sigshared -n demo "./set_tmux_master.sh $SIGSHARED_PATH"
fi
