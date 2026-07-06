#!/bin/bash

# Increase the limit of open files a process can handle
ulimit -HSn 102400

CPU_SHM_MGR=(0)
CPU_GATEWAY=(0 1 2 3 4 5)


#CPU_NF=( 3 4 5 6 7 8 9 20 21 22 23 24 25 26 27 28 29)
CPU_NF=(3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19)
        #0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19

BIN_PATH=/mydata/sigbpf/bin

if [ ${EUID} -ne 0 ]
then
	echo "${0}: Permission denied" 1>&2
	exit 1
fi

if [ ${BIN_PATH} ]
then
	echo "Executing bins under ${BIN_PATH}"
	build_path=${BIN_PATH}
else
	echo "BIN_PATH is not specified; use the default path or consider adding BIN_PATH before ${0}."
	build_path=bin/
fi

print_usage()
{
	echo "usage: ${0} < shm_mgr CFG_FILE | gateway | nf NF_ID >" 1>&2
}

shm_mgr()
{
	if ! [ ${1} ]
	then
		print_usage
		exit 1
	fi

	exec ${build_path}/shm_mgr_sigbpf \
		-l ${CPU_SHM_MGR[0]} \
		--file-prefix=sigbpf \
		--proc-type=primary \
		--no-telemetry \
		--no-pci \
		-- \
		${1}
}

gateway()
{
	exec ${build_path}/gateway_sigbpf \
		-l ${CPU_GATEWAY[0]},${CPU_GATEWAY[1]} \
		--main-lcore=${CPU_GATEWAY[0]} \
		--file-prefix=sigbpf \
		--proc-type=secondary \
		--no-telemetry \
		--no-pci
}

gateway_nf()
{

	CPU_NF=( 3 4 5 6 7 8 9 20 21 22 23 24 25 26 27 28 29)
	#CPU_NF=(3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19)
	exec ${build_path}/gateway_nf_sigbpf \
		-l ${CPU_GATEWAY[0]},${CPU_GATEWAY[1]} \
		--main-lcore=${CPU_GATEWAY[0]} \
		--file-prefix=sigbpf \
		--proc-type=secondary \
		--no-telemetry \
		--no-pci
}


nf()
{
	if ! [ ${1} ]
	then
		print_usage
		exit 1
	fi

	CPU_NF=( 3 4 5 6 7 8 9 20 21 22 23 24 25 26 27 28 29)
	exec ${build_path}/nf_sigbpf \
		-l ${CPU_NF[$((${1} - 1))]} \
		--file-prefix=sigbpf \
		--proc-type=secondary \
		--no-telemetry \
		--no-pci \
		-- \
		${1}

}


adservice()
{
	if ! [ ${1} ]
	then
		print_usage
		exit 1
	fi


	exec ${build_path}/nf_adservice_sigbpf \
		-l ${CPU_NF[$((${1} - 1))]} \
		--file-prefix=sigbpf \
		--proc-type=secondary \
		--no-telemetry \
		--no-pci \
		-- \
		${1}
}


currencyservice()
{
	if ! [ ${1} ]
	then
		print_usage
		exit 1
	fi


	exec ${build_path}/nf_currencyservice_sigbpf \
		-l ${CPU_NF[$((${1} - 1))]} \
		--file-prefix=sigbpf \
		--proc-type=secondary \
		--no-telemetry \
		--no-pci \
		-- \
		${1}

}

emailservice()
{
	if ! [ ${1} ]
	then
		print_usage
		exit 1
	fi


	exec ${build_path}/${go}nf_emailservice_sigbpf \
		-l ${CPU_NF[$((${1} + 3))]} \
		--file-prefix=sigbpf \
		--proc-type=secondary \
		--no-telemetry \
		--no-pci \
		-- \
		${1}

}

paymentservice()
{
	if ! [ ${1} ]
	then
		print_usage
		exit 1
	fi

	exec ${build_path}/nf_paymentservice_sigbpf \
		-l ${CPU_NF[$((${1} + 3))]} \
		--file-prefix=sigbpf \
		--proc-type=secondary \
		--no-telemetry \
		--no-pci \
		-- \
		${1}

}

shippingservice()
{
	if ! [ ${1} ]
	then
		print_usage
		exit 1
	fi

	exec ${build_path}/nf_shippingservice_sigbpf \
		-l ${CPU_NF[$((${1} - 1))]} \
		--file-prefix=sigbpf \
		--proc-type=secondary \
		--no-telemetry \
		--no-pci \
		-- \
		${1}
	

}

productcatalogservice()
{
	if ! [ ${1} ]
	then
		print_usage
		exit 1
	fi

	exec ${build_path}/nf_productcatalogservice_sigbpf \
		-l ${CPU_NF[$((${1} - 1))]} \
		--file-prefix=sigbpf \
		--proc-type=secondary \
		--no-telemetry \
		--no-pci \
		-- \
		${1}
}

cartservice()
{
	if ! [ ${1} ]
	then
		print_usage
		exit 1
	fi


	exec ${build_path}/${go}nf_cartservice_sigbpf \
		-l ${CPU_NF[$((${1} - 1))]} \
		--file-prefix=sigbpf \
		--proc-type=secondary \
		--no-telemetry \
		--no-pci \
		-- \
		${1}

}

recommendationservice()
{
	if ! [ ${1} ]
	then
		print_usage
		exit 1
	fi


	exec ${build_path}/${go}nf_recommendationservice_sigbpf \
		-l ${CPU_NF[$((${1} - 1))]} \
		--file-prefix=sigbpf \
		--proc-type=secondary \
		--no-telemetry \
		--no-pci \
		-- \
		${1}

}

frontendservice()
{
	if ! [ ${1} ]
	then
		print_usage
		exit 1
	fi


	exec ${build_path}/${go}nf_frontendservice_sigbpf \
		-l ${CPU_NF[$((${1} ))]} \
		--file-prefix=sigbpf \
		--proc-type=secondary \
		--no-telemetry \
		--no-pci \
		-- \
		${1}

}

checkoutservice()
{
	if ! [ ${1} ]
	then
		print_usage
		exit 1
	fi


	exec ${build_path}/${go}nf_checkoutservice_sigbpf \
		-l ${CPU_NF[$((${1} + 3))]} \
		--file-prefix=sigbpf \
		--proc-type=secondary \
		--no-telemetry \
		--no-pci \
		-- \
		${1}

}

case ${1} in
	"shm_mgr" )
		shm_mgr ${2}
	;;

	"gateway" )
		gateway
	;;

	"gateway_nf" )
		gateway_nf
	;;


	"nf" )
		nf ${2}	
	;;


	"adservice" )
		adservice ${2}
	;;

	"currencyservice" )
		currencyservice ${2}
	;;

	"emailservice" )
		emailservice ${2}
	;;

	"paymentservice" )
		paymentservice ${2}
	;;

	"shippingservice" )
		shippingservice ${2}
	;;

	"productcatalogservice" )
		productcatalogservice ${2}
	;;

	"cartservice" )
		cartservice ${2}
	;;

	"recommendationservice" )
		recommendationservice ${2}
	;;

	"frontendservice" )
		frontendservice ${2}
	;;

	"checkoutservice" )
		checkoutservice ${2}
	;;

	* )
		print_usage
		exit 1
esac

