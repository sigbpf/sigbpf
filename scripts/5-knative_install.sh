#!/bin/bash


export MYMOUNT=/mydata
mount_path=$MYMOUNT

if [[ $mount_path == "" ]]
then
	echo MYMOUNT env var not defined
	exit 1
fi

echo 'Install Knative Serving v1.15.2'

# Install the required custom resources by running the command:
kubectl apply -f https://github.com/knative/serving/releases/download/knative-v1.15.2/serving-crds.yaml
# Install the core components of Knative Serving by running the command:
kubectl apply -f https://github.com/knative/serving/releases/download/knative-v1.15.2/serving-core.yaml

echo 'Install Kourier v1.15.2'
# Install the Knative Kourier controller by running the command:
kubectl apply -f https://github.com/knative/net-kourier/releases/download/knative-v1.15.1/kourier.yaml

# Configure Knative Serving to use Kourier by default by running the command:
kubectl patch configmap/config-network \
  --namespace knative-serving \
  --type merge \
  --patch '{"data":{"ingress-class":"kourier.ingress.networking.knative.dev"}}'

# Fetch the External IP address or CNAME by running the command:
kubectl --namespace kourier-system get service kourier

# echo 'Install the required CRDs and the core components of Eventing'
# kubectl apply -f https://github.com/knative/eventing/releases/download/v0.22.0/eventing-crds.yaml
# kubectl apply -f https://github.com/knative/eventing/releases/download/v0.22.0/eventing-core.yaml
