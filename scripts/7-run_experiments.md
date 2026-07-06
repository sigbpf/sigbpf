# To run the online-boutique

## To use SIGBPF macro in user space. Add this line in /usr/include/signal.h after line 370
```#define SIGBPF SIGRTMIN ```

## In node-0

```
## replace this line in /etc/default/grub --> GRUB_CMDLINE_LINUX_DEFAULT="intel_pstate=disable intel_idle.max_cstate=0  nosmt isolcpus=0-9"
$ sudo update-grub && sudo reboot

$ cd /mydata/sigbpf/scripts
$ ./mount_bpffs.sh up && ./fix_cpuclock.sh
$ cd ../
$ make clean && make all

$ cd /mydata/sigbpf/experiment/online-boutique/ 
$ ./run_sigbpf.sh sigbpf
# make sure to use the correct interface for XDP program to attach

# In another terminal in node-0
$ cd /mydata/sigbpf
$ pidstat 1 180 -G gateway_* > sigbpf_gw.cpu & pidstat 1 180 -G nf_* > sigbpf_fn.cpu & sudo ./cpu xdp_prog 180

to end tmux: 
Crtl+b d
$ tmux kill-session -t sigbpf && sudo rm /mydata/sigbpf/dados/*
```

## In node-1
### run trafficgen
```
## In /etc/default/grub replace this line --> GRUB_CMDLINE_LINUX_DEFAULT="intel_pstate=disable intel_idle.max_cstate=0 nosmt isolcpus=0-9"
$ sudo update-grub && sudo reboot

$ /mydata/sigbpf/scripts/fix_cpuclock.sh

$ cd /mydata/sigbpf/trafficgen
### To test different loads change the RPS(7000)
$ ./onlineboutique-trafficgen 10.10.1.1 /1 8080 7000 180 1000 56378234

```

***

# To run NF's 
## In node-0

```
## In /etc/default/grub replace this line --> GRUB_CMDLINE_LINUX_DEFAULT="intel_pstate=disable intel_idle.max_cstate=0 isolcpus=0-9,20-30"
$ sudo update-grub && sudo reboot

$ cd /mydata/sigbpf/scripts
$ ./mount_bpffs.sh up && ./fix_cpuclock.sh
$ cd ../

$ make all
$ cd /mydata/sigbpf/experiments/nf/ 
$ ./run_sigbpf.sh

__make sure to use the correct interface for XDP program to attach__
# In another terminal: 
$ cd /mydata/sigbpf
$ pidstat 1 180 -G gateway_* > sigbpf_gw.cpu & pidstat 1 180 -G nf_* > sigbpf_fn.cpu & sudo ./cpu xdp_prog 180


to end tmux: 
Crtl+b d

$tmux kill-session -t sigbpf && sudo rm ../../dados/*
```

## In node-1
### run trafficgen
```
$ cd /mydata/sigbpf/trafficgen

# Alter the value /1-5 to change the request type
$ make all
$ ./nf-trafficgen 10.10.1.1 /1 8080 7000 180 1000 56378234

```

