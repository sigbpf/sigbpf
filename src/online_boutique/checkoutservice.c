/*
 * This file has been simplified and modified from the original project SPRIGHT for the purposes of test and experiment the latency impact of real-time signals in microsservices.
 *
 */


/*
# Copyright 2022 University of California, Riverside
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
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <sched.h>

#include "../include/http.h"
#include "../include/io.h"
#include "../include/spright.h"
#include "../include/utility.h"

#include "../sigbpf.h"

sigset_t set;

int mapa_fd, pid;
int matriz[11][2] = {0};
int hash_table[HASH_SIZE];

static int pipefd_rx[UINT8_MAX][2];
static int pipefd_tx[UINT8_MAX][2];

char defaultCurrency[5] = "CAD";

void prepareOrderItemsAndShippingQuoteFromCart(struct http_transaction *txn);

static void sendOrderConfirmation(struct http_transaction *txn)
{
    uuid_t binuuid;
    uuid_generate_random(binuuid);
    uuid_unparse(binuuid, txn->order_result.OrderId);
    strcpy(txn->order_result.ShippingTrackingId, txn->ship_order_response.TrackingId);
    txn->order_result.ShippingCost = txn->get_quote_response.CostUsd;
    txn->order_result.ShippingAddress = txn->place_order_request.address;

    strcpy(txn->email_req.Email, txn->place_order_request.Email);

    strcpy(txn->rpc_handler, "SendOrderConfirmation");
    txn->caller_fn = CHECKOUT_SVC;
    txn->next_fn = EMAIL_SVC;
    txn->checkoutsvc_hop_cnt++;
}

static void emptyUserCart(struct http_transaction *txn)
{
    EmptyCartRequest *in = &txn->empty_cart_request;
    strcpy(in->UserId, "ucr-students");

    strcpy(txn->rpc_handler, "EmptyCart");
    txn->caller_fn = CHECKOUT_SVC;
    txn->next_fn = CART_SVC;
    txn->checkoutsvc_hop_cnt++;
}

static void shipOrder(struct http_transaction *txn)
{
    ShipOrderRequest *in = &txn->ship_order_request;
    strcpy(in->address.StreetAddress, txn->place_order_request.address.StreetAddress);
    strcpy(in->address.City, txn->place_order_request.address.City);
    strcpy(in->address.State, txn->place_order_request.address.State);
    strcpy(in->address.Country, txn->place_order_request.address.Country);
    in->address.ZipCode = txn->place_order_request.address.ZipCode;

    strcpy(txn->rpc_handler, "ShipOrder");
    txn->caller_fn = CHECKOUT_SVC;
    txn->next_fn = SHIPPING_SVC;
    txn->checkoutsvc_hop_cnt++;
}

static void chargeCard(struct http_transaction *txn)
{
    strcpy(txn->charge_request.CreditCard.CreditCardNumber, txn->place_order_request.CreditCard.CreditCardNumber);
    txn->charge_request.CreditCard.CreditCardCvv = txn->place_order_request.CreditCard.CreditCardCvv;
    txn->charge_request.CreditCard.CreditCardExpirationYear =
        txn->place_order_request.CreditCard.CreditCardExpirationYear;
    txn->charge_request.CreditCard.CreditCardExpirationMonth =
        txn->place_order_request.CreditCard.CreditCardExpirationMonth;

    strcpy(txn->charge_request.Amount.CurrencyCode, txn->total_price.CurrencyCode);
    txn->charge_request.Amount.Units = txn->total_price.Units;
    txn->charge_request.Amount.Nanos = txn->total_price.Nanos;

    strcpy(txn->rpc_handler, "Charge");
    txn->caller_fn = CHECKOUT_SVC;
    txn->next_fn = PAYMENT_SVC;
    txn->checkoutsvc_hop_cnt++;
}

static void calculateTotalPrice(struct http_transaction *txn)
{
    log_info("Calculating total price...");
    int i = 0;
    for (i = 0; i < txn->orderItemViewCntr; i++)
    {
        MultiplySlow(&txn->order_item_view[i].Cost, txn->order_item_view[i].Item.Quantity);
        Sum(&txn->total_price, &txn->order_item_view[i].Cost);
    }
    log_info("\t\t>>>>>> priceItem(s) Subtotal: %ld.%d", txn->total_price.Units, txn->total_price.Nanos);
    log_info("\t\t>>>>>> Shipping & Handling: %ld.%d", txn->get_quote_response.CostUsd.Units,
             txn->get_quote_response.CostUsd.Nanos);
    Sum(&txn->total_price, &txn->get_quote_response.CostUsd);

    return;
}

static void returnResponseToFrontendWithOrderResult(struct http_transaction *txn)
{
    txn->next_fn = FRONTEND;
    txn->caller_fn = CHECKOUT_SVC;
}

static void returnResponseToFrontend(struct http_transaction *txn)
{
    txn->next_fn = FRONTEND;
    txn->caller_fn = CHECKOUT_SVC;
}

// Convert currency for a ShippingQuote
static void convertCurrencyOfShippingQuote(struct http_transaction *txn)
{
    if (strcmp(defaultCurrency, "USD") == 0)
    {
        log_info("Default Currency is USD. Skip convertCurrencyOfShippingQuote");
        txn->get_quote_response.conversion_flag = true;
        txn->checkoutsvc_hop_cnt++;
        prepareOrderItemsAndShippingQuoteFromCart(txn);
    }
    else
    {
        if (txn->get_quote_response.conversion_flag == true)
        {
            txn->get_quote_response.CostUsd = txn->currency_conversion_result;
            log_info("Write back convertCurrencyOfShippingQuote");
            txn->checkoutsvc_hop_cnt++;
            prepareOrderItemsAndShippingQuoteFromCart(txn);
        }
        else
        {
            log_info("Default Currency is %s. Do convertCurrencyOfShippingQuote", defaultCurrency);
            strcpy(txn->currency_conversion_req.ToCode, defaultCurrency);
            strcpy(txn->currency_conversion_req.From.CurrencyCode, txn->get_quote_response.CostUsd.CurrencyCode);
            txn->currency_conversion_req.From.Units = txn->get_quote_response.CostUsd.Units;
            txn->currency_conversion_req.From.Nanos = txn->get_quote_response.CostUsd.Nanos;

            strcpy(txn->rpc_handler, "Convert");
            txn->caller_fn = CHECKOUT_SVC;
            txn->next_fn = CURRENCY_SVC;

            txn->get_quote_response.conversion_flag = true;
        }
    }
    return;
}

static void quoteShipping(struct http_transaction *txn)
{
    GetQuoteRequest *in = &txn->get_quote_request;
    in->num_items = 0;
    txn->get_quote_response.conversion_flag = false;

    int i;
    for (i = 0; i < txn->get_cart_response.num_items; i++)
    {
        in->Items[i].Quantity = i + 1;
        in->num_items++;
    }

    strcpy(txn->rpc_handler, "GetQuote");
    txn->caller_fn = CHECKOUT_SVC;
    txn->next_fn = SHIPPING_SVC;
    txn->checkoutsvc_hop_cnt++;
}

// Convert currency for products in the cart
static void convertCurrencyOfCart(struct http_transaction *txn)
{
    if (strcmp(defaultCurrency, "USD") == 0)
    {
        log_info("Default Currency is USD. Skip convertCurrencyOfCart");
        txn->checkoutsvc_hop_cnt++;
        prepareOrderItemsAndShippingQuoteFromCart(txn);
        return;
    }
    else
    {
        if (txn->orderItemCurConvertCntr != 0)
        {
            txn->order_item_view[txn->orderItemCurConvertCntr - 1].Cost = txn->currency_conversion_result;
        }

        if (txn->orderItemCurConvertCntr < txn->orderItemViewCntr)
        {
            log_info("Default Currency is %s. Do convertCurrencyOfCart", defaultCurrency);
            strcpy(txn->currency_conversion_req.ToCode, defaultCurrency);
            txn->currency_conversion_req.From = txn->order_item_view[txn->orderItemCurConvertCntr].Cost;

            strcpy(txn->rpc_handler, "Convert");
            txn->caller_fn = CHECKOUT_SVC;
            txn->next_fn = CURRENCY_SVC;

            txn->orderItemCurConvertCntr++;
            return;
        }
        else
        {
            txn->checkoutsvc_hop_cnt++;
            prepareOrderItemsAndShippingQuoteFromCart(txn);
            return;
        }
    }
}

static void getOrderItemInfo(struct http_transaction *txn)
{
    log_info("%d items in the cart.", txn->get_cart_response.num_items);
    if (txn->get_cart_response.num_items <= 0)
    {
        log_info("None items in the cart.");
        txn->total_price.Units = 0;
        txn->total_price.Nanos = 0;
        returnResponseToFrontend(txn);
        return;
    }

    if (txn->orderItemViewCntr != 0)
    {
        strcpy(txn->order_item_view[txn->orderItemViewCntr - 1].Item.ProductId, txn->get_product_response.Id);
        txn->order_item_view[txn->orderItemViewCntr - 1].Cost = txn->get_product_response.PriceUsd;
    }

    if (txn->orderItemViewCntr < txn->get_cart_response.num_items)
    {
        strcpy(txn->get_product_request.Id, txn->get_cart_response.Items[txn->orderItemViewCntr].ProductId);
        // log_info("Product ID: %s", txn->get_product_request.Id);

        strcpy(txn->rpc_handler, "GetProduct");
        txn->caller_fn = CHECKOUT_SVC;
        txn->next_fn = PRODUCTCATA_SVC;

        txn->orderItemViewCntr++;
    }
    else
    {
        txn->orderItemCurConvertCntr = 0;
        convertCurrencyOfCart(txn);
        txn->checkoutsvc_hop_cnt++;
    }
}

static void getCart(struct http_transaction *txn)
{
    strcpy(txn->get_cart_request.UserId, "ucr-students");

    strcpy(txn->rpc_handler, "GetCart");
    txn->caller_fn = CHECKOUT_SVC;
    txn->next_fn = CART_SVC;
    txn->checkoutsvc_hop_cnt++;
}

static void prepOrderItems(struct http_transaction *txn)
{

    if (txn->checkoutsvc_hop_cnt == 1)
    {
        getOrderItemInfo(txn);
    }
    else if (txn->checkoutsvc_hop_cnt == 2)
    {
        convertCurrencyOfCart(txn);
    }
    else
    {
        log_info("prepOrderItems doesn't know what to do for HOP %u.", txn->checkoutsvc_hop_cnt);
        returnResponseToFrontend(txn);
    }
}

void prepareOrderItemsAndShippingQuoteFromCart(struct http_transaction *txn){

    log_info("Call prepareOrderItemsAndShippingQuoteFromCart ### Hop: %u", txn->checkoutsvc_hop_cnt);

    if (txn->checkoutsvc_hop_cnt == 0)
    {
        getCart(txn);
        txn->orderItemViewCntr = 0;
    }
    else if (txn->checkoutsvc_hop_cnt >= 1 && txn->checkoutsvc_hop_cnt <= 2)
    //else if (txn->checkoutsvc_hop_cnt == 2)
    {
        prepOrderItems(txn);
    }
    else if (txn->checkoutsvc_hop_cnt == 3)
    {
        quoteShipping(txn);
    }
    else if (txn->checkoutsvc_hop_cnt == 4)
    {
        convertCurrencyOfShippingQuote(txn);
    }
    else if (txn->checkoutsvc_hop_cnt == 5)
    {
        calculateTotalPrice(txn);
        chargeCard(txn);
    }
    else if (txn->checkoutsvc_hop_cnt == 6)
    {
        shipOrder(txn);
    }
    else if (txn->checkoutsvc_hop_cnt == 7)
    {
        emptyUserCart(txn);
    }
    else if (txn->checkoutsvc_hop_cnt == 8)
    {
        sendOrderConfirmation(txn);
    }
    else if (txn->checkoutsvc_hop_cnt == 9)
    {
        returnResponseToFrontendWithOrderResult(txn);
    }
    else
    {
        log_info("prepareOrderItemsAndShippingQuoteFromCart doesn't know what to do for HOP %u.",
                 txn->checkoutsvc_hop_cnt);
        returnResponseToFrontend(txn);
    }
}

static void PlaceOrder(struct http_transaction *txn)
{
    prepareOrderItemsAndShippingQuoteFromCart(txn);
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


static void *nf_worker(void *arg)
{
    struct http_transaction *txn = NULL;
    ssize_t bytes_written;
    ssize_t bytes_read;
    uint8_t index;

    /* TODO: Careful with this pointer as it may point to a stack */
    index = (uint64_t)arg;

    while (1)
    {
        bytes_read = read(pipefd_rx[index][0], &txn, sizeof(struct http_transaction *));
        if (unlikely(bytes_read == -1))
        {
            log_error("read() error: %s", strerror(errno));
            return NULL;
        }

        // if (strcmp(txn->rpc_handler, "PlaceOrder") == 0) {
        PlaceOrder(txn);
        // } else {
        // 	log_info("%s() is not supported", txn->rpc_handler);
        // 	log_info("\t\t#### Run Mock Test ####");
        // 	// MockEmailRequest(txn);
        // 	// SendOrderConfirmation(txn);
        // }

        bytes_written = write(pipefd_tx[index][1], &txn, sizeof(struct http_transaction *));
        if (unlikely(bytes_written == -1))
        {
            log_error("write() error: %s", strerror(errno));
            return NULL;
        }
    }

    return NULL;
}

static void *nf_rx(void *arg)
{
    struct http_transaction *txn = NULL;
    ssize_t bytes_written;
    uint8_t i;

    for (i = 0;; i = (i + 1) % sigbpf_cfg->nf[fn_id - 1].n_threads){
	wait_for_ownership((void **)&txn, sigbpf_ptr, &set,  hash_t);

	if(txn == NULL){
		printf("==checkout== ERRO mempool_access retornou NULL\n");
		return NULL;
	}

        bytes_written = write(pipefd_rx[i][1], &txn, sizeof(struct http_transaction *));
        if (unlikely(bytes_written == -1)){
            log_error("write() error: %s", strerror(errno));
            return NULL;
        }
    }

    return NULL;
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
//    //struct epoll_event eventos[UINT8_MAX];
//    struct epoll_event eventos[UINT16_MAX];
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
//	n = epoll_wait(epfd, eventos, UINT16_MAX, -1);  // -1 = block indefinitely
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


static void *nf_tx(void *arg){

    struct epoll_event event[UINT8_MAX]; /* TODO: Use Macro */
    struct http_transaction *txn = NULL;
    ssize_t bytes_read;
    uint8_t i;
    int n_fds;
    int epfd;
    int ret;
    int ret_io;

    epfd = epoll_create1(0);
    if (unlikely(epfd == -1)){
        log_error("epoll_create1() error: %s", strerror(errno));
        return NULL;
    }

    for (i = 0; i < sigbpf_cfg->nf[fn_id - 1].n_threads; i++){
        ret = set_nonblocking(pipefd_tx[i][0]);
        if (unlikely(ret == -1)){
            return NULL;
        }

        event[0].events = EPOLLIN;
        event[0].data.fd = pipefd_tx[i][0];

        ret = epoll_ctl(epfd, EPOLL_CTL_ADD, pipefd_tx[i][0], &event[0]);
        if (unlikely(ret == -1)){
            log_error("epoll_ctl() error: %s", strerror(errno));
            return NULL;
        }
    }

    while (1){
        n_fds = epoll_wait(epfd, event, sigbpf_cfg->nf[fn_id - 1].n_threads, -1);
        if (unlikely(n_fds == -1)){
            log_error("epoll_wait() error: %s", strerror(errno));
            return NULL;
        }

        for (i = 0; i < n_fds; i++){
            bytes_read = read(event[i].data.fd, &txn, sizeof(struct http_transaction *));
            if (unlikely(bytes_read == -1)){
                log_error("read() error: %s", strerror(errno));
                return NULL;
            }

            log_debug("Route id: %u, Hop Count %u, Next Hop: %u, Next Fn: %u, \
                Caller Fn: %s (#%u), RPC Handler: %s()",
                      txn->route_id, txn->hop_count, sigbpf_cfg->route[txn->route_id].hop[txn->hop_count], txn->next_fn,
                      txn->caller_nf, txn->caller_fn, txn->rpc_handler);

	    ret_io = transfer_ownership(txn->addr, txn->next_fn, &map_fd, pid, matriz[txn->next_fn][1], hash_t);
            if (unlikely(ret_io == -1)){
                log_error("io_tx() error");
                return NULL;
            }
        }
    }

    return NULL;
}


void preheat_mempool(void* ptr, size_t size) {
    volatile char* p = (volatile char*)ptr;
    for (size_t i = 0; i < size; i += 4096) {
        p[i] = p[i]; // Toca em cada página de 4KB para forçar o mapeamento físico
    }
}

/* TODO: Cleanup on errors */
static int nf(uint8_t nf_id)
{
    pthread_t thread_worker[UINT8_MAX];
    pthread_t thread_rx;
    pthread_t thread_tx;
    uint8_t i;
    int ret;

    fn_id = nf_id;

    matriz[nf_id][1] = pid;
    if( sigbpf_update_map("pid_map", fn_id, pid, &mapa_fd) < 0  ){
        printf("Erro ao atualizar mapa\n");
                return 0;
    }

    int index = (HASH_SIZE - 1) & pid;
    hash_t[index].pid = pid ;
    hash_t[index].index = index;

    char temp[256];
    char *dir_temp = getenv("SIGBPF");
    sprintf(temp, "%s/dados/pid_map", dir_temp);
    int fd_map_signal = bpf_obj_get(temp);

    int pid_front, key_front = 1;
    if( bpf_map_lookup_elem(fd_map_signal, &key_front, &pid_front ) < 0 ){
	    perror("Error to lookup the gateway's PID");
	    exit(1);
    }

    matriz[1][1] = pid_front;
    sigval_t data_send;
    data_send.sival_ptr = (void *)(uintptr_t)nf_id;
    if(sigqueue(pid_front, SIGRTMIN+2, data_send) < 0){
        perror("Error to send signal to gateway");
    }

    siginfo_t data_rcv;
    if( sigwaitinfo(&set, &data_rcv) ){
        log_info("Sinal recebido do gateway!");
        lookup_map(&fd_map_signal, matriz);
    }

    index = (HASH_SIZE - 1) & pid_front;
    hash_t[index].pid = pid_front;
    hash_t[index].index = index;


    for (i = 0; i < sigbpf_cfg->nf[fn_id - 1].n_threads; i++)
    {
        ret = pipe(pipefd_rx[i]);
        if (unlikely(ret == -1))
        {
            log_error("pipe() error: %s", strerror(errno));
            return -1;
        }

        ret = pipe(pipefd_tx[i]);
        if (unlikely(ret == -1))
        {
            log_error("pipe() error: %s", strerror(errno));
            return -1;
        }
    }

    
    int cpu_id = 12;
    ret = pthread_create(&thread_rx, NULL, &nf_rx, NULL);
    if (unlikely(ret != 0))
    {
        log_error("pthread_create() error: %s", strerror(ret));
        return -1;
    }
    //set_cpu_affinity(thread_tx, cpu_id);
    
    ret = pthread_create(&thread_tx, NULL, &nf_tx, NULL);
    if (unlikely(ret != 0))
    {
        log_error("pthread_create() error: %s", strerror(ret));
        return -1;
    }
    //set_cpu_affinity(thread_tx, cpu_id);

    
    for (i = 0; i < sigbpf_cfg->nf[fn_id - 1].n_threads; i++)
    {
        ret = pthread_create(&thread_worker[i], NULL, &nf_worker, (void *)(uint64_t)i);
        if (unlikely(ret != 0))
        {
            log_error("pthread_create() error: %s", strerror(ret));
            return -1;
		}

		set_cpu_affinity(thread_worker[i], cpu_id);
	}

    
    for (i = 0; i < sigbpf_cfg->nf[fn_id - 1].n_threads; i++)
    {
        ret = pthread_join(thread_worker[i], NULL);
        if (unlikely(ret != 0))
        {
            log_error("pthread_join() error: %s", strerror(ret));
            return -1;
        }
    }

    ret = pthread_join(thread_rx, NULL);
    if (unlikely(ret != 0))
    {
        log_error("pthread_join() error: %s", strerror(ret));
        return -1;
    }

    ret = pthread_join(thread_tx, NULL);
    if (unlikely(ret != 0))
    {
        log_error("pthread_join() error: %s", strerror(ret));
        return -1;
    }

    for (i = 0; i < sigbpf_cfg->nf[fn_id - 1].n_threads; i++)
    {
        ret = close(pipefd_rx[i][0]);
        if (unlikely(ret == -1))
        {
            log_error("close() error: %s", strerror(errno));
            return -1;
        }

        ret = close(pipefd_rx[i][1]);
        if (unlikely(ret == -1))
        {
            log_error("close() error: %s", strerror(errno));
            return -1;
        }

        ret = close(pipefd_tx[i][0]);
        if (unlikely(ret == -1))
        {
            log_error("close() error: %s", strerror(errno));
            return -1;
        }

        ret = close(pipefd_tx[i][1]);
        if (unlikely(ret == -1))
        {
            log_error("close() error: %s", strerror(errno));
            return -1;
        }
    }

    return 0;
}


int main(int argc, char **argv){
    log_set_level_from_env();

    log_set_level(LOG_INFO);

    uint8_t nf_id;
    int ret;

    char settar_cpuf[30];
    pid = getpid();
    
    log_info("Pinning process in CPU 12...\n");
    sprintf(settar_cpuf, "taskset -cp 12 %d", pid);
    int ret_sys = system(settar_cpuf);
    if( ret_sys == -1 || ret_sys == 127 ){
            log_error("Erro ao settar CPU");
            exit(1);
    }

    sigbpf_ptr = sigbpf_ptr_mem();
    if(sigbpf_ptr == NULL){
        log_error("Error: sigbpf_ptr");
        return 1;
    }

    sigbpf_cfg = sigbpf_cfg_ptr();
    if(sigbpf_cfg == NULL){
        log_error("Error: sigbpf_cfg");
        return 1;
    }

    sigemptyset(&set);       
    //sigaddset(&set, SIGRTMIN+1); 
    sigaddset(&set, SIGBPF);
    sigaddset(&set, SIGRTMIN+2);
    sigprocmask(SIG_BLOCK, &set, NULL);

    errno = 0;
    nf_id = strtol(argv[argc-1], NULL, 10);
    if (unlikely(errno != 0 || nf_id < 1))
    {
        log_error("Invalid value for Network Function ID");
        goto error_1;
    }

    ret = nf(nf_id);
    if (unlikely(ret == -1))
    {
        log_error("nf() error");
        goto error_1;
    }

    return 0;

error_1:
    printf("Error: nf()\n");
}
