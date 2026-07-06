#!/bin/bash

ARCH=$1
IP=$2
PORT=$3
LOAD_GEN_PATH=$4

if [ -z "$ARCH" ] ; then
  echo "Usage: $0 <ARCH> <LOAD_GEN_PATH>"
  exit 1
fi

tmux set remain-on-exit on

echo "Creating tmux panes..."
for j in {0..29}
do
    tmux split-window -v -p 80 -t ${j}
    tmux select-layout -t ${j} tiled
done

echo "Configuring fds in tmux panes..."
for j in {1..29}
do
    tmux send-keys -t ${j} "./set_fd.sh" Enter
    tmux send-keys -t ${j} "cd ./locust_worker_${j}/" Enter
    sleep 0.1
done

echo "Testing the locust master in tmux pane 1..."
if [ $ARCH == "sigbpf" ]; then
  tmux send-keys -t 1 "locust --version && echo \"Run SPRIGHT's locust master\" " Enter
else
  tmux send-keys -t 1 "locust --version && echo \"Run Knative's locust master\"" Enter
fi

sleep 0.1

echo "Testing locust workers in each tmux pane..."
for j in {2..29}
do
    #if [ $ARCH == "spright" ]; then
    if [ $ARCH == "sigbpf" ]; then
      tmux send-keys -t ${j} "locust --version && echo \"Run SPRIGHT's locust worker in pane ${j}\"" Enter
    else
      tmux send-keys -t ${j} "locust --version && echo \"Run Knative's locust worker in pane ${j}\"" Enter
    fi
    sleep 0.1
done

# tmux kill-pane -t 0

######
echo "Run the locust master in tmux pane 1..."
if [ $ARCH == "sigbpf" ]; then
  echo "Run SPRIGHT's locust master"
  tmux send-keys -t 1 "locust -u 15000 -r 500 -t 3m --csv kn --csv-full-history -f sigbpf-locustfile.py --headless  -H http://10.10.1.1:8080 --master --expect-workers=27" Enter
fi

sleep 0.1

echo "Run locust workers in each tmux pane..."
for j in {2..29}
do
    if [ $ARCH == "sigbpf" ]; then
      echo "Run sigbpf locust worker"
      tmux send-keys -t ${j} "locust -f sigbpf-locustfile.py --worker" Enter
    fi
done

sleep 0.1
