/*Copyright 2026  Universidade Federal do Mato Grosso do Sul
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0 
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <time.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <sys/shm.h>
#include <signal.h>

#include "sigbpf.h"

#include "./include/io.h"
#include "./log/log.h"


void io_rx( void **obj, void *sigbpf_ptr, sigset_t *set){

	uint64_t addr;
	siginfo_t data_rcv;

	if( likely( sigwaitinfo(set, &data_rcv) > 0) ){

		addr = (uint64_t)data_rcv.si_value.sival_ptr;
		//pid_t pid_emissor = data_rcv.si_pid;
		//for(i=0 ; i < ONLINE_CONTAINERS; i++){

		//    //log_info("%d", matriz[i][1]);
		//    //if(!matriz[i][1]){
		//    matriz[i][1] = sigbpf_lookup_map("mapa_sinal", i);
		//    //log_info("consultando mapa ebpf...%d == %d", i, matriz[i][1]);
		//    //}

		//    if ( matriz[i][1] == pid_emissor ){
		//        //if(addr >= 0 && addr < N_ELEMENTS){
		//        //log_info("PID_EMISSOR BATEU COM matriz[%d][1] = %d",i, matriz[i][1]);
		//        if(likely( addr <= N_ELEMENTS) ){
		//            sigbpf_mempool_access(obj, addr);
		//            //log_info("Endereco de retorno: %p", sigbpf_mempool_access(obj, addr));
		//            //if(!obj)
		//            //	log_info("### Invalid addr: %ld\n", addr);
		//            return;
		//        }
		//        else{
		//            log_error("### Invalid addr: %ld\n", addr);
		//            return;
		//        }
		//    }
		if (addr >= N_ELEMENTS) {
			printf("Invalid addr\n");
			return;
		}

		sigbpf_mempool_access(obj, addr);
	}

//    //printf("Invalid addr: %ld\n", addr);
//}
	else{
		log_error("sigwaitinfo() returned < 0\n");
		//perror("ERROR: sigwaitinfo() returned < 0");
	}

	//log_error("sigwaitinfo() returned < 0\n");
	return;
}


void wait_for_ownership( void **obj, void *sigbpf_ptr, sigset_t *set, HashTable hash[]){

	uint64_t addr;
	uint32_t index;
	siginfo_t data_rcv;

	sigwaitinfo(set, &data_rcv);

	addr = (uint64_t)data_rcv.si_value.sival_ptr;
	pid_t pid_emissor = data_rcv.si_pid;	


	index = pid_emissor & (HASH_SIZE - 1);

	if( /*likely(addr >= N_ELEMENTS) && */ likely( hash[index].pid == pid_emissor ) ){
		sigbpf_mempool_access(obj, addr);
		return;
	}
	else{
		log_error("PID not in hash table: index(%d) PID:%d", index, pid_emissor);
		return;
	}

	return;
}


int transfer_ownership(uint64_t addr, uint8_t next_fn, int *map_fd, int pid, int next_fn_pid, HashTable hash[]){

	sigval_t data_send;
	data_send.sival_ptr = (void *)addr;
	uint64_t index = (HASH_SIZE - 1) & next_fn_pid;

	if( likely( hash[index].pid == next_fn_pid) ){

		if( unlikely(sigqueue( next_fn_pid, SIGBPF, data_send) < 0) ){
			log_error("==io_tx_matriz(%d)== Error to send signal: next_fn:%d pid:%d | sinal:%d | addr:%ld", pid, next_fn, next_fn_pid, SIGBPF, addr);
			exit(1);
		}

		return 0;
	}
	else{
		log_error("PID not in hash table: index(%d) PID:%d", index, next_fn_pid);
		return 0;
	}
	
	return 0;
}
