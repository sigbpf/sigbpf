#!/bin/bash

SIGBPF_PATH=$1

if [ -z "$SIGBPF_PATH" ] ; then
  echo "Usage: $ <SIGBPF_PATH>"
  exit 1
fi


echo "Creating tmux panes..."
for j in {0..19}
do
    tmux split-window -v -p 80 -t ${j}
    tmux select-layout -t ${j} tiled
done

echo "Configuring fds in tmux panes..."
for j in {1..19}
do
    tmux send-keys -t ${j} "cd /mydata/sigbpf/" Enter
    sleep 0.1
done


tmux set remain-on-exit on

echo "Testing S-SIGBPF with parking detection dataset..."
tmux send-keys -t 1 "sudo ./run.sh shm_mgr cfg/long-chain.cfg" Enter
sleep 1
tmux send-keys -t 2 "sudo ./run.sh gateway_nf" Enter
sleep 5 
tmux send-keys -t 3 "sudo ./run.sh nf 1" Enter
sleep 1
tmux send-keys -t 4 "sudo ./run.sh nf 2" Enter
sleep 1
tmux send-keys -t 5 "sudo ./run.sh nf 3" Enter
sleep 1
tmux send-keys -t 6 "sudo ./run.sh nf 4" Enter
sleep 1
tmux send-keys -t 7 "sudo ./run.sh nf 5" Enter
sleep 1
tmux send-keys -t 8 "sudo ./run.sh nf 6" Enter
sleep 1
tmux send-keys -t 9 "sudo ./run.sh nf 7" Enter
sleep 1
tmux send-keys -t 10 "sudo ./run.sh nf 8" Enter
sleep 1
tmux send-keys -t 11 "sudo ./run.sh nf 9" Enter
sleep 1
tmux send-keys -t 12 "sudo ./run.sh nf 10" Enter
sleep 1
tmux send-keys -t 13 "sudo ./run.sh nf 11" Enter
sleep 1
tmux send-keys -t 14 "sudo ./run.sh nf 12" Enter
sleep 1
tmux send-keys -t 15 "sudo ./run.sh nf 13" Enter
sleep 1
tmux send-keys -t 16 "sudo ./run.sh nf 14" Enter
sleep 1
tmux send-keys -t 17 "sudo ./run.sh nf 15" Enter
sleep 1
tmux send-keys -t 18 "sudo ./run.sh nf 16" Enter

sleep 0.1

echo "Starting CPU usage collection..."
cd /mydata

if [ ! -d "nf-results/" ] ; then
    echo "nf-results/ DOES NOT exists."
    mkdir motivation-results/
fi

#cd parking-results
cd motivation-results

pidstat 1 180 -G ^gateway_$ > sigbpf_gw.motivation.cpu & pidstat 1 180 -G ^nf_$ > sigbpf_fn.motivation.cpu

echo "CPU usage collection is done!"
