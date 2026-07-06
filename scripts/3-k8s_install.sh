#/bin/bash
#this script can be run with non-root user


export MYMOUNT=/mydata

function usage {
        echo "$0 [master] [master-ip] or [slave]"
        exit 1
}

node_type=$1
master_ip=$2

if [ "$node_type" = "master" -a "$master_ip" = "" ]
then
    usage

elif [ "$node_type" != "master" -a "$node_type" != "slave" ] 
then
    usage
fi

function move_docker_dir {
	sudo service docker stop
	sudo mv /var/lib/docker /my_mount
	sudo ln -s /my_mount/docker /var/lib/docker	
	sudo service docker restart
	sudo docker -v
}

function install_docker_ce {
	sudo apt-get update
	sudo apt-get install -y \
             default-jre \
             default-jdk \
	     apt-transport-https \
	     ca-certificates \
	     curl \
	     gnupg-agent \
	     software-properties-common \
	     conntrack
}

function off_swap {
	sudo swapoff -a
	cat /etc/fstab | grep -v '^#' | grep -v 'swap' | sudo tee /etc/fstab	
}

function install_k8s_tools {
	echo "Now Installing Kubernetes and Tools"

	# Installation Documentation: https://kubernetes.io/docs/setup/production-environment/tools/kubeadm/install-kubeadm/

	# ** Update apt package index and install dependencies for Kubernetes apt repository

	sudo apt-get update
	# apt-transport-https may be a dummy package; if so, you can skip that package
	sudo apt-get install -y apt-transport-https ca-certificates curl gpg

	# ** Download public signing key for Kubernetes apt repository

	# If the directory `/etc/apt/keyrings` does not exist, it should be created before the curl command.
	# In releases older than Debian 12 and Ubuntu 22.04, directory /etc/apt/keyrings does not exist by default, and it should be created before the curl command.
	sudo mkdir -p -m 755 /etc/apt/keyrings
	curl -fsSL https://pkgs.k8s.io/core:/stable:/v1.31/deb/Release.key | sudo gpg --dearmor -o /etc/apt/keyrings/kubernetes-apt-keyring.gpg

	# ** Add Kubernetes apt repository

	# This overwrites any existing configuration in /etc/apt/sources.list.d/kubernetes.list
	echo 'deb [signed-by=/etc/apt/keyrings/kubernetes-apt-keyring.gpg] https://pkgs.k8s.io/core:/stable:/v1.31/deb/ /' | sudo tee /etc/apt/sources.list.d/kubernetes.list

	# ** Update apt package index and install kubectlm kubeadm, and kubectl

	sudo apt-get update
	sudo apt-get install -y kubelet kubeadm kubectl
	sudo apt-mark hold kubelet kubeadm kubectl

	# ** K8 Configurations

	# enable unsafe sysctl options in kubelet configure file
	sudo sed -i '/\[Service\]/a\Environment="KUBELET_UNSAFE_SYSCTLS=--allowed-unsafe-sysctls='kernel.shm*,kernel.sem,kernel.msg*,net.core.*'"' /etc/systemd/system/kubelet.service.d/10-kubeadm.conf
	sudo systemctl daemon-reload
	sudo systemctl restart kubelet

	echo "Finished Installing Kubernetes and Tools"
}
 
function deploy_k8s_master {
	# deploy kubernetes cluster
	sudo kubeadm init --apiserver-advertise-address=$master_ip --pod-network-cidr=10.244.0.0/16
 	#sudo kubeadm init --apiserver-advertise-address=$master_ip --pod-network-cidr=192.168.0.0/16
	# for non-root user, make sure that kubernetes config directory has the same permissions as kubernetes config file.
	mkdir -p $HOME/.kube
	sudo cp -i /etc/kubernetes/admin.conf $HOME/.kube/config
	sudo chown $(id -u):$(id -g) $HOME/.kube/config
	sudo chown $(id -u):$(id -g) $HOME/.kube/
	
	#deploy flannel
	sudo sysctl net.bridge.bridge-nf-call-iptables=1
	# https://kubernetes.io/docs/setup/production-environment/tools/kubeadm/create-cluster-kubeadm/
	#after this step, coredns status will be changed to running from pending
	kubectl apply -f https://raw.githubusercontent.com/coreos/flannel/master/Documentation/kube-flannel.yml

	# Install cni plugin to configure container network and fix NetworkReady=false
	#kubectl apply -f https://github.com/weaveworks/weave/releases/download/v2.8.1/weave-daemonset-k8s.yaml

	kubectl get nodes
	kubectl get pods --namespace=kube-system
}

install_docker_ce
off_swap
install_k8s_tools
if [ "$node_type" = "master" ] 
then
    deploy_k8s_master
fi
