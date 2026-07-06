/*
 * This file has been simplified and modified from the original project SPRIGHT for the purposes of test and experiment the latency impact of real-time signals in microsservices.
 *
 */


/*
# Copyright 2025 University of California, Riverside
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
*/

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sched.h>

#include "sigbpf.h"
#include "../ebpf/xsk_kern.skel.h"

#include "./include/http.h"
#include "./include/io.h"
#include "./include/spright.h"
#include "./include/timer.h"
#include "./include/utility.h"

struct xsk_kern *skel;
int mapa_fd, pid_gateway;
sigset_t set,set2;


int matriz[20][2] = {0};

#define MAX_FDS 65535

#define IS_SERVER_TRUE 1
#define IS_SERVER_FALSE 0

#define HTTP_RESPONSE                                                                                                  \
    "HTTP/1.1 200 OK\r\n"                                                                                              \
    "Connection: close\r\n"                                                                                            \
    "Content-Type: text/plain\r\n"                                                                                     \
    "Content-Length: 13\r\n"                                                                                           \
    "\r\n"                                                                                                             \
    "Hello World\r\n"


struct server_vars
{
    int rpc_svr_sockfd; // Handle intra-cluster RPCs
    int ing_svr_sockfd; // Handle external clients
    int epfd;
};

typedef struct {
    int sockfd;
    int is_server;     // 1 for server_fd, 0 for client_fd
    int peer_svr_fd;   // Peer server_fd (for client_fd)
} sockfd_context_t;


static sockfd_context_t fd_contexts[MAX_FDS]; // Memória pré-alocada no segmento de dados

int peer_node_sockfds[ROUTING_TABLE_SIZE];
static int dispatch_msg_to_fn(struct http_transaction *txn){
    int ret_io;

    if (txn->next_fn != sigbpf_cfg->route[txn->route_id].hop[txn->hop_count])
    {
        if (txn->hop_count == 0)
        {
            txn->next_fn = sigbpf_cfg->route[txn->route_id].hop[txn->hop_count];
            //log_debug("Dispatcher receives a request from conn_read.");
        }
        else
        {
            //log_debug("Dispatcher receives a request from conn_write or rpc_server.");
            log_info("Dispatcher receives a request from conn_write or rpc_server.");
        }
    }

    
    ret_io = transfer_ownership(txn->addr, txn->next_fn, &map_fd, pid_gateway, matriz[txn->next_fn][1], hash_t); 
    if (unlikely(ret_io == -1)){
        log_error("io_tx() error");
        return -1;
    }

    return 0;
}


static int rpc_client_setup(char *server_ip, uint16_t server_port, uint8_t peer_node_idx){
    log_info("RPC client connects with node %u (%s:%u).", peer_node_idx, cfg->nodes[peer_node_idx].ip_address,
             INTERNAL_SERVER_PORT);

    struct sockaddr_in server_addr;
    int sockfd;
    int ret;
    int opt = 1;

    log_debug("Destination Gateway Address (%s:%u).", server_ip, server_port);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (unlikely(sockfd == -1)){
        log_error("socket() error: %s", strerror(errno));
        return -1;
    }

    // Set SO_REUSEADDR to reuse the address
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        perror("setsockopt(SO_REUSEADDR) failed");
        close(sockfd);
        return -1;
    }

    configure_keepalive(sockfd);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    ret = retry_connect(sockfd, (struct sockaddr *)&server_addr);
    if (unlikely(ret == -1))
    {
        log_error("connect() failed: %s", strerror(errno));
        return -1;
    }

    return sockfd;
}


static int rpc_client_send(int peer_node_idx, struct http_transaction *txn)
{
    log_debug("Route id: %u, Hop Count %u, Next Hop: %u, Next Fn: %u, \
        Caller Fn: %s (#%u), RPC Handler: %s()",
              txn->route_id, txn->hop_count, cfg->route[txn->route_id].hop[txn->hop_count], txn->next_fn,
              txn->caller_nf, txn->caller_fn, txn->rpc_handler);

    ssize_t bytes_sent;
    int sockfd = peer_node_sockfds[peer_node_idx];

    bytes_sent = send(sockfd, txn, sizeof(*txn), 0);

    log_debug("sockfd: %d, peer_node_idx: %d \t bytes_sent: %ld \t sizeof(*txn): %ld", sockfd, peer_node_idx, bytes_sent, sizeof(*txn));
    if (unlikely(bytes_sent == -1))
    {
        log_error("send() error: %s", strerror(errno));
        return -1;
    }

    log_debug("rpc_client_send is done.");

    return 0;
}


int rpc_client(struct http_transaction *txn)
{
    int ret;

    uint8_t peer_node_idx = get_node(txn->next_fn);

    if (peer_node_sockfds[peer_node_idx] == 0)
    {
        peer_node_sockfds[peer_node_idx] =
            rpc_client_setup(cfg->nodes[peer_node_idx].ip_address, INTERNAL_SERVER_PORT, peer_node_idx);
    }
    else if (peer_node_sockfds[peer_node_idx] < 0)
    {
        log_error("Invalid socket error.");
        return -1;
    }

    ret = rpc_client_send(peer_node_idx, txn);
    if (unlikely(ret == -1))
    {
        log_error("rpc_client_send() failed: %s", strerror(errno));
        return -1;
    }


    return 0;
}


static int conn_accept(int svr_sockfd, struct server_vars *sv){
    struct epoll_event event;
    int clt_sockfd;
    int ret;

    clt_sockfd = accept(svr_sockfd, NULL, NULL);
    if (unlikely(clt_sockfd == -1)){
        log_error("accept() error: %s", strerror(errno));
        goto error_0;
    }

    sockfd_context_t *clt_sk_ctx = malloc(sizeof(sockfd_context_t));
    clt_sk_ctx->sockfd      = clt_sockfd;
    clt_sk_ctx->is_server   = IS_SERVER_FALSE;
    clt_sk_ctx->peer_svr_fd = svr_sockfd;

    /* Configure RPC connection keepalive 
     * TODO: keep external connection alive 
     */
    if (svr_sockfd == sv->rpc_svr_sockfd){
        log_debug("Set RPC connection to keep alive.");
        configure_keepalive(clt_sockfd);
        event.events = EPOLLIN;
    } else // svr_sockfd == sv->ing_svr_sockfd
    {
        event.events = EPOLLIN | EPOLLONESHOT;
    }

    event.data.ptr = clt_sk_ctx;

    ret = epoll_ctl(sv->epfd, EPOLL_CTL_ADD, clt_sockfd, &event);
    if (unlikely(ret == -1)){

        log_error("epoll_ctl() error: %s", strerror(errno));
        goto error_1;
    }

    return 0;

error_1:
    close(clt_sockfd);
    //free(clt_sk_ctx);
error_0:
    return -1;
}


static int conn_close(struct server_vars *sv, int sockfd){
    int ret;
    int saved_errno = 0;

    if (sockfd < 0)
        return -1;

    // Try to remove from epoll; ENOENT means already removed.
    ret = epoll_ctl(sv->epfd, EPOLL_CTL_DEL, sockfd, NULL);
    if (ret == -1 && errno != ENOENT) {
        saved_errno = errno;
        log_error("epoll_ctl(DEL, fd=%d) error: %s", sockfd, strerror(errno));
    }

    // Always close the socket, even if epoll_ctl failed
    if (close(sockfd) == -1) {
        if (!saved_errno) saved_errno = errno;
        log_error("close(fd=%d) error: %s", sockfd, strerror(errno));
    }

    if (saved_errno) {
        errno = saved_errno;
        return -1;
    }

    return 0;
}


static void parse_route_id(struct http_transaction *txn){
    const char *string = strstr(txn->request, "/");
    //log_info("%s", string);

    // /1/cart
    if (unlikely(string == NULL)) {
        txn->route_id = 0;
	log_error("==gateway== txn->request EH NULO");
    } else {
        // Skip consecutive slashes in one step
        string += strspn(string, "/");
	
	//log_info("string: %s", string);
        
 	errno = 0;
        txn->route_id = strtol(string, NULL, 10);
        if (unlikely(errno != 0 || txn->route_id < 0)) {
            txn->route_id = 0;
	    log_error("==gateway== 2 route_id = 0");
        }
    }

    //log_debug("Route ID: %d", txn->route_id);
}

static int conn_read(int sockfd, void* sk_ctx){

    uint64_t addr = allocate_object(sigbpf_ptr);

    struct http_transaction *txn = sigbpf_mempool_access((void **)&txn, addr); 
    int ret;
    
    if ( txn == NULL){
        log_error("sigbpf_mempool_get error: return NULL");
        goto error_0;
    }
    
    txn->addr = addr;


    log_debug("Receiving from External User.");
    txn->length_request = recv(sockfd, txn->request, HTTP_MSG_LENGTH_MAX, 0);
    if (unlikely(txn->length_request == -1))
    {
        log_error("read() error: %s", strerror(errno));
        goto error_1;
    }

    txn->sockfd = sockfd;
    txn->sk_ctx = sk_ctx;

    // TODO: parse tenant ID from HTTP request,
    // use "0" as the default tenant ID for now.
    txn->tenant_id = 0;

    parse_route_id(txn);

    txn->hop_count = 0;

    ret = dispatch_msg_to_fn(txn);
    if (unlikely(ret == -1)){
        log_error("dispatch_msg_to_fn() error: %s", strerror(errno));
        goto error_0;
    }

    return 0;

error_1:
    //rte_mempool_put(cfg->mempool, txn);
error_0:
    return -1;
}



static uint64_t conn_write(int *sockfd){

    int ret;
    ssize_t bytes_sent;
    uint64_t addr = -1;
    struct http_transaction *txn = NULL;
    
    wait_for_ownership((void **)&txn, sigbpf_ptr, &set,  hash_t);

    if(txn == NULL){
    	log_error("==gateway== ERRO sigbpf_mempool_access retornou NULL");
	return -1;
    }
    addr=txn->addr;

    //log_debug("Route id: %u, Hop Count %u, Next Hop: %u, Next Fn: %u", txn->route_id, txn->hop_count, sigbpf_cfg->route[txn->route_id].hop[txn->hop_count], txn->next_fn);

    txn->hop_count++;
    txn->next_fn = sigbpf_cfg->route[txn->route_id].hop[txn->hop_count];

    // Intra-node Communication (use io_tx_matriz() method)
    if (txn->hop_count < sigbpf_cfg->route[txn->route_id].length){
        
	ret = dispatch_msg_to_fn(txn);
	if (unlikely(ret == -1)){
            log_error("dispatch_msg_to_fn() error: %s", strerror(errno));
	    return -1;
            //goto error_1;
        }

        return 1;
    }

    // Respond External Client
    *sockfd = txn->sockfd;
    txn->length_response = strlen(HTTP_RESPONSE);
    memcpy(txn->response, HTTP_RESPONSE, txn->length_response);

    bytes_sent = write(*sockfd, txn->response, txn->length_response);
    if (unlikely(bytes_sent == -1)){
	    log_error("write() error: %s", strerror(errno));
	    return -1;
    }

    // Return addr to free mem after connection is closed
    return addr;
}


// signalfd implementation
//static void *nf_rx(void *arg){
//    //struct http_transaction *txn = NULL;
//    struct http_transaction *txn;
//    ssize_t bytes_written;
//    uint8_t i;
//    //int ret;
//    //uint64_t addr;
//    //int pid = getpid();
//
//    sigset_t block_set;
//
//    sigemptyset(&block_set);       
//    sigaddset(&block_set, SIGRTMIN+1); 
//    pthread_sigmask(SIG_BLOCK, &block_set, NULL);
//
//    //txn = (struct http_transaction *) mmap(0, SIGBPF_TAM, PROT_WRITE, MAP_SHARED, fd_sigbpf_mem, 0);
//
//    struct epoll_event eventos[UINT8_MAX];
//    int n;
//    int sigfd = signalfd(-1, &block_set, SFD_NONBLOCK | SFD_CLOEXEC);
//
//    int epfd = epoll_create1(0);
//    if (unlikely(epfd == -1))
//    {
//        log_error("epoll_create1() error: %s", strerror(errno));
//        return NULL;
//    }
//
//    eventos[0].events = EPOLLIN; // The associated file is available for read(2) operations.
//    eventos[0].data.fd = sigfd;
//
//    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sigfd, &eventos[0]) < 0) {
//        perror("epoll_ctl");
//        exit(1);
//    }
//
//    for (i = 0;; i = (i + 1) % sigbpf_cfg->nf[fn_id - 1].n_threads){
//
//	//log_info("Recebendo sinal...");
//        //ret = io_rx((void **)&txn);
//        //addr = io_rx(txn, sigbpf_ptr, &set);
//        //txn = io_rx(txn, sigbpf_ptr, &set);
//        
//	//io_rx((void **)&txn, sigbpf_ptr, &set);
//        
//	//if (unlikely(ret == -1))
//        //if (unlikely(addr == -1)){
//        //    log_error("io_rx() error");
//        //    return NULL;
//        //}
//	
//	//printf("Esperando sinal com epoll_wait()...\n");
//	n = epoll_wait(epfd, eventos, UINT8_MAX, -1);  // -1 = block indefinitely
//        
//        if (n < 0) {
//            if (errno == EINTR)
//                continue;
//            perror("epoll_wait");
//            break;
//        }
//
//        for (int j = 0; j < n; j++) {
//            if (eventos[j].data.fd == sigfd) {
//                
//		struct signalfd_siginfo si;
//                ssize_t res = read(sigfd, &si, sizeof(si));
//                if (res != sizeof(si)) {
//                    perror("read(signalfd)");
//                    continue;
//                }
//
//                //printf("Received signal %d from PID %d | data: %ld\n", si.ssi_signo, si.ssi_pid, (uint64_t)si.ssi_ptr);
//		txn = sigbpf_mempool_access( (void**)&txn, (uint64_t)si.ssi_ptr );
//		if(unlikely(txn == NULL)){
//			printf("==frontend== txn retornou NULL\n");
//			exit(1);
//		}
//		
//		bytes_written = write(pipefd_rx[i][1], &txn, sizeof(struct http_transaction *));
//		if (unlikely(bytes_written == -1)){
//			log_error("write() error: %s", strerror(errno));
//			return NULL;
//		}
//	    }
//	}
//
//
//
//	//printf("==frontend== dps sigbpf_mempool_access() | txn->addr:%ld\n", txn->addr);
//        //log_info("(ADDR RX:%ld), HOP: %u, Next Fn: %u, Caller Fn: %s (#%u) ", txn->addr, txn->hop_count, txn->next_fn, txn->caller_nf, txn->caller_fn);
//    }
//
//    return NULL;
//}




int req_client = 0;
static int event_process(struct epoll_event *event, struct server_vars *sv){

    int ret;
    log_debug("Processing an new RX event.");
    sockfd_context_t *sk_ctx = (sockfd_context_t *)event->data.ptr;

    log_debug("sk_ctx->sockfd: %d \t sv->rpc_svr_sockfd: %d", sk_ctx->sockfd, sv->rpc_svr_sockfd);

    if (sk_ctx->is_server){
        
	//log_debug("Accepting new connection on %s.", sk_ctx->sockfd == sv->rpc_svr_sockfd ? "RPC server" : "Ingress server");
        ret = conn_accept(sk_ctx->sockfd, sv);
        
	if (unlikely(ret == -1)){
            log_error("conn_accept() error");
            return -1;
        }
    }
    else if(event->events & EPOLLIN){
        
	if (sk_ctx->peer_svr_fd == sv->ing_svr_sockfd){
            
	    //log_debug("Reading new data from external client.");
            ret = conn_read(sk_ctx->sockfd, sk_ctx);
            
	    if (unlikely(ret == -1)){
                log_error("conn_read() error");
                return -1;
            }
        }
	else{
            log_error("Unknown peer_svr_fd");
            return -1;
        }

        if (ret == 1){
            
	    event->events |= EPOLLONESHOT;
            ret = epoll_ctl(sv->epfd, EPOLL_CTL_MOD, sk_ctx->sockfd, event);

            if (unlikely(ret == -1)){
                log_error("epoll_ctl() error: %s", strerror(errno));
                return -1;
            }
        }
    }
    else if (event->events & (EPOLLERR | EPOLLHUP)){

        /* TODO: Handle (EPOLLERR | EPOLLHUP) */
        log_error("(EPOLLERR | EPOLLHUP)");

        log_debug("Error - Close the connection.");
        
	ret = conn_close(sv, sk_ctx->sockfd);
        free(sk_ctx);
        if (unlikely(ret == -1)){
            log_error("conn_close() error");
            return -1;
        }
    }

    return 0;
}


/* TODO: Cleanup on errors */
static int server_init(struct server_vars *sv)
{
    int ret;

    log_info("Initializing Ingress socket...");

    fn_id = 0;

    sv->ing_svr_sockfd = create_server_socket(sigbpf_cfg->nodes[sigbpf_cfg->local_node_idx].ip_address, EXTERNAL_SERVER_PORT);
    if (unlikely(sv->ing_svr_sockfd == -1))
    {
        log_error("socket() error: %s", strerror(errno));
        return -1;
    }
    sockfd_context_t *ing_svr_sk_ctx = malloc(sizeof(sockfd_context_t));
    ing_svr_sk_ctx->sockfd = sv->ing_svr_sockfd;
    ing_svr_sk_ctx->is_server = IS_SERVER_TRUE;
    ing_svr_sk_ctx->peer_svr_fd = -1;

    log_info("Initializing epoll...");
    sv->epfd = epoll_create1(0);
    if (unlikely(sv->epfd == -1))
    {
        log_error("epoll_create1() error: %s", strerror(errno));
        return -1;
    }

    struct epoll_event event;
    event.events = EPOLLIN;


    event.data.ptr = ing_svr_sk_ctx;
    ret = epoll_ctl(sv->epfd, EPOLL_CTL_ADD, sv->ing_svr_sockfd, &event);
    if (unlikely(ret == -1))
    {
        log_error("epoll_ctl() error: %s", strerror(errno));
        return -1;
    }

    return 0;
}

/* TODO: Cleanup on errors */
static int server_exit(struct server_vars *sv)
{
    int ret;

    ret = epoll_ctl(sv->epfd, EPOLL_CTL_DEL, sv->ing_svr_sockfd, NULL);
    if (unlikely(ret == -1))
    {
        log_error("epoll_ctl() error: %s", strerror(errno));
        return -1;
    }

    ret = close(sv->epfd);
    if (unlikely(ret == -1))
    {
        log_error("close() error: %s", strerror(errno));
        return -1;
    }

    ret = close(sv->ing_svr_sockfd);
    if (unlikely(ret == -1))
    {
        log_error("close() error: %s", strerror(errno));
        return -1;
    }

    return 0;
}


static void server_process_rx(void *arg){

    struct epoll_event event[N_EVENTS_MAX];
    struct server_vars *sv = NULL;
    int n_fds;
    int ret;
    int i;

    sv = arg;

    while (1){
        
	log_debug("Waiting for new RX events...");
        n_fds = epoll_wait(sv->epfd, event, N_EVENTS_MAX, -1);
        
	if (unlikely(n_fds == -1)){
            log_error("epoll_wait() error: %s", strerror(errno));
	    printf("==gateway== ERRO epoll_wait retornou -1\n");
	    return;
        }

        log_debug("epoll_wait() returns %d new events", n_fds);

        for (i = 0; i < n_fds; i++){
            
	    ret = event_process(&event[i], sv);
            if (unlikely(ret == -1)){
                
		log_error("event_process() error");
		printf("==gateway== ERRO event_process retornou -1\n");
		return;
            }
        }
    }
}

static void server_process_tx(void *arg){
    
    struct server_vars *sv = NULL;
    int sockfd;
    int ret;
    uint64_t addr=-1;

    sv = arg;
       while (1){

        addr = conn_write(&sockfd);
        if (unlikely(addr == -1)){

            log_error("conn_write() error");
            //printf("==gateway== Error event_process returned -1\n");
            return;
        }
        else if (addr == 1){
            continue;
        }

        //log_debug("Closing the connection after TX.\n");

        ret = conn_close(sv, sockfd);
        if (unlikely(ret == -1)){

            log_error("conn_close() error");
            exit(1);
            return;
        }

        //sigbpf_mempool_put(addr);
        release_object(addr);
    }

    return;
}


void set_cpu_affinity(pthread_t thread, int core_id){

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    int s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (s != 0) {
        fprintf(stderr, "Erro ao definir afinidade no Core %d\n", core_id);
    }
}

/***************************************************************/

void signal_rcv_config(){

	char temp[256];
	char *dir_temp = getenv("SIGBPF");
    	sprintf(temp, "%s/dados/pid_map", dir_temp);
    	int fd_map_signal = bpf_obj_get(temp);
	
	siginfo_t data_rcv;
	for(int i=1; i < ONLINE_CONTAINERS && (sigwaitinfo(&set2, &data_rcv) > 0) ; i++){

		pid_t pid_emissor = data_rcv.si_pid;
		int nf_id_rcv = (uint64_t )data_rcv.si_value.sival_ptr;
		seek_elem_map(fd_map_signal, nf_id_rcv, pid_emissor, matriz, hash_t);
		
	}
	log_info("Signals from all processes received...");
	log_info("Sending signal to them to populate the hash table...");

        sigval_t data_send;
        data_send.sival_ptr = (void *)1;

	for(int i=1; i < ONLINE_CONTAINERS; i++){

		if(sigqueue(matriz[i][1], SIGRTMIN+2, data_send) < 0){
			perror("Error to send signal to gateway");
			exit(1);	
		}
		log_info("SIGNALS SENT TO %d", matriz[i][1]);
	}
	log_info("SIGNALS SENT TO NF's");
}

/***************************************************************/


int prog_fd;
char patah[256];
FILE *f_bpf, *f_saida;
uint64_t  r_ns;

uint64_t get_program_ns(){

    char *temp = NULL;
    char *param = "run_time_ns:";
    char buffer[256]; // Buffer to store the line
    
    while (fgets(buffer, sizeof(buffer), f_bpf) != NULL) {
        
        temp = strstr(buffer, param);
        if(temp == NULL){
            //printf("temp: %s\n", temp);
            continue;
        }
        
        sscanf(temp, "%*s %ld", &r_ns);
    }

    rewind(f_bpf);
    return r_ns;
}
/***********************************************************************/


static void metrics_collect(void){

    while (1){
        sleep(30);
    }
}


static int gateway(void)
{
    int ret;
    fn_id = 0;
    struct server_vars sv;
    struct rlimit r = {RLIM_INFINITY, RLIM_INFINITY};

    setrlimit(RLIMIT_MEMLOCK, &r);
    memset(peer_node_sockfds, 0, sizeof(peer_node_sockfds));
    
    pid_gateway = getpid();

    uint32_t index = (HASH_SIZE - 1) & 0;
    hash_t[index].pid = pid_gateway;
    hash_t[index].index = index;

    char *temp = getenv("SIGBPF");
    char path[300];
    sprintf(path, "%s/dados", temp);
    printf("###\n%s\n", path);

    skel = xsk_kern__open_and_load();
    if(skel == NULL){
        printf("+++ERROR+++\nskel == NULL\n");
        return 0;
    }

    log_info("(gateway) eBPF program loaded...\n");
    // Example: num_id: ens1f1 --> IP 
    skel->links.xdp_prog = bpf_program__attach_xdp( skel->progs.xdp_prog , 2 );
    if(skel->links.xdp_prog == NULL){
        log_error("+++VERIFY NET INTERFACE NUMBER+++\n");
        return -1;
    }

    if( bpf_object__pin_maps(skel->obj, path) < 0){
        log_error("Error to pinn map");
        return -1;
    }

    log_info("(gateway) eBPF map pinned...\n");

    char sinal_path[400]; 
    //sprintf(sinal_path, "%s/mapa_sinal", path);
    sprintf(sinal_path, "%s/pid_map", path);
    
    int fd_map = bpf_obj_get(sinal_path);
    if(fd_map < 0){
        log_error("Error to get map's FD...\n");
        return -1;
    }
    if( sigbpf_update_map("pid_map", fn_id, pid_gateway, &mapa_fd) < 0  ){
        log_error("Error to update map\n");
        goto error_0;
    }

    //log_info("mapa_sinal updated...\n");
    log_info("pid_map updated...\n");

    int prog_fd = bpf_program__fd(skel->progs.xdp_prog );
    char path_stats[256]; 
    if(sprintf(path_stats, "/proc/%d/fdinfo/%d", pid_gateway, prog_fd) < 0){
        perror("Error to create path to process's stats");
        exit(1);
    }

    f_bpf = fopen(path_stats, "r");
    if(f_bpf == NULL){
        perror("Error to open stats file");
        exit(1);
    }

    ret = server_init(&sv);
    if (unlikely(ret == -1))
    {
        log_error("server_init() error");
        goto error_0;
    }

    pthread_t rx_thread, tx_thread;
   
    if( pthread_create(&rx_thread, NULL, (void *)server_process_rx, (void *)&sv) < 0){
        log_error("Error to create thread RX");
        goto error_1;
    }
    set_cpu_affinity(rx_thread, 0);

    if( pthread_create(&tx_thread, NULL, (void *)server_process_tx, (void *)&sv) < 0){
        log_error("Error to create thread TX");
        goto error_1;
    }
    set_cpu_affinity(tx_thread, 1);

    struct sched_param sp = { .sched_priority = 50 };
    pthread_setschedparam(tx_thread, SCHED_FIFO, &sp);

    sleep(3);
    int pid_front, key_front = 1;
    if( bpf_map_lookup_elem(fd_map, &key_front, &pid_front ) < 0 ){
            perror("Error to lookup the gateway's PID");
            exit(1);
    }

    matriz[1][1] = pid_front;

    int aux_index = (HASH_SIZE - 1) & 1;
    hash_t[aux_index].pid = pid_front;
    hash_t[aux_index].index = aux_index;

    seek_elem_map(fd_map, 1, pid_front, matriz, hash_t);
    
    metrics_collect();
    return 0;

error_1:
    server_exit(&sv);
error_0:
    return -1;
}

int main(int argc, char **argv)
{
    log_set_level_from_env();

    // show all logs larger than level DEBUG
    // The level enum is defined in log.h
    log_set_level(LOG_INFO);
    int ret;

    char settar_cpuf[256];
    log_info("Pinning process in CPU 0...\n");
    sprintf(settar_cpuf, "taskset -cp 0 %d", getpid());
    int ret_sys = system(settar_cpuf);
    if( ret_sys == -1 || ret_sys == 127 ){
            log_error("Error to pinn CPU");
            exit(1);
    }

    sigbpf_ptr = sigbpf_ptr_mem();
    if(sigbpf_ptr == NULL){
        log_error("Error: sigbpf_ptr");
        return 1;
    }

    memset(fd_contexts, 0, sizeof(fd_contexts));

    sigbpf_cfg = sigbpf_cfg_ptr();
    if(sigbpf_cfg == NULL){
        log_error("Error: sigbpf_cfg");
        return 1;
    }

    ringbuff = sigbpf_mempool_ptr();
    if(ringbuff == NULL){
        log_error("Error:  ringbuff");
        return 1;
    }

    sigemptyset(&set);
    //sigaddset(&set, SIGRTMIN+1);
    sigaddset(&set, SIGBPF);
    sigprocmask(SIG_BLOCK, &set, NULL);

    sigemptyset(&set2);       
    sigaddset(&set2, SIGRTMIN+2); 
    pthread_sigmask(SIG_BLOCK, &set2, NULL);

    for(int i=0; i< ONLINE_CONTAINERS; i++)
	    matriz[i][1] = 0;

    ret = gateway();
    if (unlikely(ret == -1))
    {
        log_error("gateway() error");
        goto error_1;
    }

    return 0;

error_1:
    printf("Error: gateway()\n");
}

