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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>  
#include <sys/types.h> 
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <assert.h>
#include <stddef.h>
#include <sched.h>
#include <stdatomic.h>

#include "sigbpf.h"
#include "include/spright.h"
#include "include/http.h"
#include "include/io.h"
#include "log/log.h"


#define N_MASK (N_ELEMENTS - 1)

void *sigbpf_ptr;
struct spright_cfg_s *sigbpf_cfg;
int fd_sigbpf_mem;
int fd_sigbpf_mempool;
int fd_cfg_file;

char temp[400];
char dir_temp[256];
int map_fd = -1;

struct sigbpf_ringbuffer *ringbuff;
void *sigbpf_mempool;
uint64_t rb[N_ELEMENTS];

HashTable hash_t[HASH_SIZE] = {0};

int fd_shm = -1;
int mapa_sinal_fd = -1;

void *sigbpf_create_mem(){

    fd_sigbpf_mem = shm_open(SIGBPF_NAME, O_CREAT | O_RDWR, 0777);
    if (fd_sigbpf_mem < 0){ 
        perror("Error: shm_open()");
        exit(1);
    }

    int ret_ftruncate = ftruncate(fd_sigbpf_mem, SIGBPF_TAM); 
    if ( ret_ftruncate == -1 ){
        perror("Error: ftruncate()");
        exit(1);  
    }

    return ( void *) mmap(0, SIGBPF_TAM, PROT_WRITE, MAP_SHARED, fd_sigbpf_mem, 0);
}

void *sigbpf_ptr_mem(){

    fd_sigbpf_mem = shm_open(SIGBPF_NAME, O_CREAT | O_RDWR, 0777);
    if ( fd_sigbpf_mem < 0){ 
        perror("Error: shm_open()");
        exit(1);
    }

    return ( void *) mmap(0, SIGBPF_TAM, PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd_sigbpf_mem, 0);
}

struct spright_cfg_s *sigbpf_cfg_mem(){

    fd_cfg_file = shm_open("CFG_MEM", O_CREAT | O_RDWR, 0777);
    if (fd_cfg_file < 0){ 
        perror("Error: shm_open()");
        exit(1);
    }

    int ret_ftruncate = ftruncate(fd_cfg_file, sizeof(struct spright_cfg_s)); 
    if ( ret_ftruncate == -1 ){
        perror("Error: ftruncate()");
        exit(1);  
    }

    return ( struct spright_cfg_s *) mmap(0, sizeof(struct spright_cfg_s), PROT_WRITE, MAP_SHARED, fd_cfg_file, 0);
}

struct spright_cfg_s *sigbpf_cfg_ptr(){

    fd_cfg_file = shm_open("CFG_MEM", O_CREAT | O_RDWR, 0777);
    if (fd_cfg_file < 0){ 
        perror("Error: shm_open()");
        exit(1);
    }

    return ( struct spright_cfg_s *) mmap(0, sizeof(struct spright_cfg_s), PROT_WRITE, MAP_SHARED, fd_cfg_file, 0);
}

int sigbpf_update_map(char *map_name, int fn_id, int pid, int *map_fd){

    char temp[256];
    char *dir_temp = getenv("SIGBPF");

    sprintf(temp, "%s/dados/%s", dir_temp, map_name);
    *map_fd = bpf_obj_get(temp);

    if(bpf_map_update_elem(*map_fd, &fn_id, &pid, BPF_ANY) < 0){
        perror("Error to update eBPF map");
        return -1;
    }

    log_info("(update_map(%d)) eBPF map updated...\n", pid);
    return 0;
}

int sigbpf_lookup_map(char *map_name, int key){

	int map_sig_fd = 0;
	char temp[256];
	char *dir_temp = getenv("SIGBPF");
	int pid_ret;


	sprintf(temp, "%s/dados/%s", dir_temp, map_name);
	map_sig_fd = bpf_obj_get(temp);
	if(map_sig_fd <= 0){
		sprintf(temp, "%s/dados/%s", dir_temp, map_name);
		map_sig_fd = bpf_obj_get(temp);
	}

	if( bpf_map_lookup_elem(map_sig_fd, &key, &pid_ret) < 0 ){
		printf("Error to lookup at eBPF map | map_sinal_fd:%d  key:%d  pid_ret:%d\n", map_sig_fd, key, pid_ret);
		return -1;
	}

	return pid_ret;
}

struct sigbpf_ringbuffer *sigbpf_mempool_create() {
    int fd = shm_open(RINGBUF_REGION, O_RDWR | O_CREAT, 0777);
    if (fd < 0) {
        perror("shm_open");
        return NULL;
    }

    if (ftruncate(fd, RINGBUF_TAM) < 0) {
        perror("ftruncate");
        close(fd); // Importante fechar o FD em caso de erro
        return NULL;
    }

    // MAP_POPULATE (Linux) pre-allocate mem pages 
    struct sigbpf_ringbuffer *rb_ptr = mmap(NULL, RINGBUF_TAM,
                                          PROT_READ | PROT_WRITE,
                                          MAP_SHARED | MAP_POPULATE, fd, 0);

    close(fd); 

    if (rb_ptr == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    // Values initialization 
    for (uint64_t i = 0; i < N_ELEMENTS; i++) {
        rb_ptr->ringbuffer[i] = i;
    }

    rb_ptr->head = 0;
    rb_ptr->tail = N_ELEMENTS - 1;

    ringbuff = rb_ptr;

    return rb_ptr;
}

struct sigbpf_ringbuffer *sigbpf_mempool_ptr(){
    int fd_ringbuff = shm_open(RINGBUF_REGION, O_CREAT | O_RDWR, 0777);
    if (fd_ringbuff < 0){ 
        perror("Error: shm_open()");
        exit(1);
    }

    return ( struct sigbpf_ringbuffer *) mmap(0, RINGBUF_TAM, PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd_ringbuff, 0);
}


uint64_t allocate_object() {
    uint64_t h = ringbuff->head;
    uint64_t new_h = ringbuff->head+1;
    if (new_h == ringbuff->tail){ 
        return (uint64_t)-1;
    }

    uint64_t addr = ringbuff->ringbuffer[h];
    ringbuff->head = (new_h) & N_MASK;
    
    return addr;
}

int release_object(uint64_t addr) {
    uint64_t next_tail = (ringbuff->tail + 1) & N_MASK;

    if (next_tail == ringbuff->head) {
        return -1; // Overflow
    }

    ringbuff->ringbuffer[ringbuff->tail] = addr;
    ringbuff->tail = next_tail;

    return 0;
}

struct http_transaction *sigbpf_mempool_access(void **temp, uint64_t addr) {
    if (unlikely(sigbpf_ptr == NULL || addr >= N_ELEMENTS)) {
	log_info("Error: wrong access");
	printf("ERROR IN sigbpf_mempool_access()\n");

        return NULL;
    }

    // Calculate offset
    struct http_transaction *ptr = &((struct http_transaction *)sigbpf_ptr)[addr];
    *temp = (void *)ptr;
    return ptr;
}


void seek_elem_map(int fd_map_signal, int nf_id, int pid,int matriz[][2], HashTable *hash_t){

    int temp, index;
    if ( bpf_map_lookup_elem(fd_map_signal, &nf_id, &temp) == 0 ){ 

            matriz[nf_id][1] = pid; 
	    index = (HASH_SIZE - 1) & pid; 
	    hash_t[index].pid = pid;
	    hash_t[index].index = index;

            log_info("mapa_sinal[%d]: %d index: %d\n", nf_id, temp, index);
        return;
    }

    perror("Error to read eBPF map"); 
}


void inline lookup_map(int *fd_mapa_sinal, int matriz[][2]){

    int temp;
    for(int i=1; i < ONLINE_CONTAINERS; i++){
        if ( bpf_map_lookup_elem(*fd_mapa_sinal, &i, &temp) == 0 ){

                matriz[i][1] = temp;
                uint32_t index = (HASH_SIZE - 1) & temp;
		hash_t[index].pid = temp;
                hash_t[index].index = index;

                log_info("(%d)hash_table[%d]: %d\n", i, index, temp);
        }
	else{
		log_error("Error: Failed to lookup eBPF map\n");
	}
    }
}


void inline lookup_map_nf(int *fd_mapa_sinal, int matriz[][2]){

    int temp;
    for(int i=1; i < NF_CONTAINERS; i++){
        if ( bpf_map_lookup_elem(*fd_mapa_sinal, &i, &temp) == 0 ){

                matriz[i][1] = temp;
                uint32_t index = (HASH_SIZE - 1) & temp;
		hash_t[index].pid = temp;
                hash_t[index].index = index;

                log_info("(%d)hash_table[%d]: %d\n", i, index, temp);
        }
	else{
		log_error("Error: Failed to lookup eBPF map\n");
	}
    }
}
