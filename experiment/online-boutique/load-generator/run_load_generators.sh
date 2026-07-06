#!/bin/bash

ARCH=$1
IP=$2
PORT=$3
LOAD_GEN_PATH="./load-generator/"

if [ -z "$ARCH" ] || [ -z "$IP" ] || [ -z "$PORT" ]; then
  echo "Usage: $0 <sigbpf> <IP> <Port>"
  exit 1
fi

echo "Creating separate directory for each Locust worker..."
for j in {1..29}
do
    if [ ! -d "locust_worker_${j}/" ] ; then
      echo "locust_worker_${j}/ DOES NOT exists."
      cp -r $LOAD_GEN_PATH locust_worker_${j}/
    fi
    # sleep 1
done

if [ -z "$TMUX" ]; then
  if [ -n "`tmux ls | grep sigbpf`" ]; then
    tmux kill-session -t sigbpf 
  fi
  tmux new-session -s sigbpf -n demo "./set_tmux_worker.sh $ARCH $IP $PORT $LOAD_GEN_PATH"
fi
