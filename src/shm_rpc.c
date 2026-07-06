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

#include "sigbpf.h"

#include "./include/shm_rpc.h"
#include "./log/log.h"



char defaultCurrency[5] = "CAD";

void returnResponse(struct http_transaction *txn){
    txn->hop_count += 100;
    txn->next_fn = GATEWAY;

	//log_info("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);

    // PrintSupportedCurrencies(txn);
    // PrintListProductsResponse(txn);
    // PrintGetCartResponse(txn);
    // PrintProductView(txn);
    // PrintGetProductResponse(txn);
    // PrintListRecommendationsResponse(txn);
    // PrintGetQuoteResponse(txn);
    // PrintAdResponse(txn);
    PrintTotalPrice(txn);
}

void chooseAd(struct http_transaction *txn){
    strcpy(txn->rpc_handler, "GetAds");
    
    /**************************/
    txn->caller_fn = FRONTEND;
    txn->next_fn = AD_SVC;
    /**************************/

	//log_info("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
	//printf("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);

    txn->hop_count++;
}

void getCurrencies(struct http_transaction *txn){
    strcpy(txn->rpc_handler, "GetSupportedCurrencies");

    /**************************/
    txn->caller_fn = FRONTEND;
    txn->next_fn = CURRENCY_SVC;
    /**************************/
	//log_info("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
	//printf("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
	
    txn->hop_count++;
}

// Get a list of products
void getProducts(struct http_transaction *txn){
    strcpy(txn->rpc_handler, "ListProducts");

    /**************************/
    txn->caller_fn = FRONTEND;
    txn->next_fn = PRODUCTCATA_SVC;
    /**************************/
	//log_info("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
	//printf("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);

    txn->hop_count++;
}

void getCart(struct http_transaction *txn){
    strcpy(txn->get_cart_request.UserId, "ucr-students");

    strcpy(txn->rpc_handler, "GetCart");
    
    /**************************/
    txn->caller_fn = FRONTEND;
    txn->next_fn = CART_SVC;
    /**************************/
	//log_info("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
	//printf("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
	
    txn->hop_count++;
}

// Convert currency for a list of products
void convertCurrencyOfProducts(struct http_transaction *txn){
    if (strcmp(defaultCurrency, "USD") == 0){
        log_debug("Default Currency is USD. Skip convertCurrency");
        int i = 0;
        for (i = 0; i < txn->list_products_response.num_products; i++)
        {
            txn->product_view[i].Item  = txn->list_products_response.Products[i];
            txn->product_view[i].Price = txn->list_products_response.Products[i].PriceUsd;
        }
        // returnResponse(txn);
        txn->hop_count++;
        return;
    }
    else{

        log_debug("Default Currency is %s. Do convertCurrency", defaultCurrency);
        if (txn->productViewCntr != 0){
            txn->product_view[txn->productViewCntr - 1].Item  = txn->list_products_response.Products[txn->productViewCntr - 1];
            txn->product_view[txn->productViewCntr - 1].Price = txn->currency_conversion_result;
        }

        int size = sizeof(txn->product_view) / sizeof(txn->product_view[0]);
        if (txn->productViewCntr < size)
        {
            strcpy(txn->currency_conversion_req.ToCode, defaultCurrency);
            strcpy(txn->currency_conversion_req.From.CurrencyCode,
                   txn->list_products_response.Products[txn->productViewCntr].PriceUsd.CurrencyCode);
            txn->currency_conversion_req.From.Units =
                txn->list_products_response.Products[txn->productViewCntr].PriceUsd.Units;
            txn->currency_conversion_req.From.Nanos =
                txn->list_products_response.Products[txn->productViewCntr].PriceUsd.Nanos;

            strcpy(txn->rpc_handler, "Convert");
	    
	    /**************************/
            txn->caller_fn = FRONTEND;
            txn->next_fn = CURRENCY_SVC;
	    /**************************/
	//log_info("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
	//printf("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);

            txn->productViewCntr++;
            return;
        }
        else
        {
            // returnResponse(txn);
            txn->hop_count++;
            return;
        }
    }
}

static __always_inline char *find_char(const char *s, char c) {
    if (!s) return NULL;
    while (*s && *s != c)
        s++;
    return *s ? (char*)s : NULL;
}
// Get a single product
void getProduct(struct http_transaction *txn){
    //char *query = httpQueryParser(txn->request);
    
   
    //char aux[HTTP_MSG_LENGTH_MAX];	
    //char *query = httpQueryParser(txn->request, aux, HTTP_MSG_LENGTH_MAX);
    char *query = httpQueryParser(txn->request);
    if (!query) {
            //if (!aux) {
            log_error("httpQueryParser retornou NULL");
            exit(1);
            //return;
    }

    
    //char *query = httpQueryParser(txn->request);
    //if (!txn || !txn->request) {
    //    log_error("insertCart: invalid txn or request");
    //    returnResponse(txn);
    //    return;
    //} 
    
    //char aux[HTTP_MSG_LENGTH_MAX];
    ////char *query = httpQueryParser(txn->request, aux, HTTP_MSG_LENGTH_MAX);
    //if ( !query ) {
    //        log_error("httpQueryParser retornou NULL");
    //        exit(1);
    //        //return;
    //}
    
   //log_info("Query: %s", query);

    char *req = txn->request;
    ////char tmp[600]; 
    //char tmp[HTTP_MSG_LENGTH_MAX+1]; 
    ////strcpy(tmp, req);
    //strncpy(tmp, req, sizeof(tmp) -1);
    //tmp[sizeof(tmp) -1] = '\0';
    //
    //char *saveptr = NULL;
    ////char *start_of_path = strtok(tmp, " ");
    //char *start_of_path = strtok_r(tmp, " ", &saveptr);
    //if( unlikely( start_of_path == NULL) ){
    //    log_error("start_of_path == NULL, erro no strtok");
    //    returnResponse(txn);
    //    return;
    //	//exit(1);
    //}

    ////start_of_path = strtok(NULL, " ");
    //start_of_path = strtok_r(NULL, " ", &saveptr);
    //if( unlikely(start_of_path == NULL)){
    //    log_error("start_of_path == NULL, erro no strtok");
    //    returnResponse(txn);
    //    return;
    //	//exit(1);
    //}
    ////printf("%s\n", start_of_path); 

    ////char *query = strchr(start_of_path, '?') + 1;
    //char *query = find_char(start_of_path, '?');
    ////if( query == NULL){
    //if( unlikely(!query  || *(query+1) == '\0')){
    //	log_error("query == NULL, erro em strchr");
    //    returnResponse(txn);
    //    return;
    //    //exit(1);
    //}
    ////char temp_req[HTTP_MSG_LENGTH_MAX];
    ////strcpy(temp_req, txn->request);
    ////strncpy(temp_req, txn->request, sizeof(txn->request));

    //query +=1;
 

    //log_info("DPS do hhttpQueryParser");

    //char req[HTTP_MSG_LENGTH_MAX];
    //strncpy(req, txn->request, sizeof(txn->request));

    if (strstr(req, "/1/cart?") != NULL && strstr(req, "POST")){

        char *start_of_product_id = strtok(query, "&");
        //char *start_of_product_id = strtok(aux, "&");
	//char *ID = strchr(start_of_product_id, '=') +1;
        
	strcpy(txn->get_product_request.Id, strchr(start_of_product_id, '=') + 1);
	//strcpy(txn->get_product_request.Id, ID);
        //strncpy(txn->get_product_request.Id,  ID, sizeof(*ID));
        log_debug("Product ID: %s", txn->get_product_request.Id);
        // returnResponse(txn); return;
    }
    else if (strstr(req, "/1/product") != NULL){

        //char *start_of_product_id = strtok(aux, "&");
        strcpy(txn->get_product_request.Id, query);
        //strncpy(txn->get_product_request.Id, query, sizeof(*query));
        //strcpy(txn->get_product_request.Id, start_of_product_id);
        log_debug("Product ID: %s", txn->get_product_request.Id);
    }
    else{

        log_warn("HTTP Query cannot be parsed!");
        log_warn("\t#### %s", query);
        //log_warn("\t#### %s", aux);
        returnResponse(txn);
	//free(query);
        return;
    }

    //strcpy(txn->rpc_handler, "GetProduct");
    strncpy(txn->rpc_handler, "GetProduct", 11);
 
    /**************************/
    txn->caller_fn = FRONTEND;
    txn->next_fn   = PRODUCTCATA_SVC;
    /**************************/

	//log_info("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
    txn->hop_count++;

    //free(query);
}

void getRecommendations(struct http_transaction *txn){
    strcpy(txn->list_recommendations_request.ProductId, txn->get_product_request.Id);

    strcpy(txn->rpc_handler, "ListRecommendations");

    /**************************/
    txn->caller_fn = FRONTEND;
    txn->next_fn = RECOMMEND_SVC;
    /**************************/
	//log_info("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
	//printf("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
    txn->hop_count++;
}

// Convert currency for a product
void convertCurrencyOfProduct(struct http_transaction *txn){
    if (strcmp(defaultCurrency, "USD") == 0)
    {
        log_debug("Default Currency is USD. Skip convertCurrencyOfProduct");
        txn->product_view[0].Item = txn->get_product_response;
        txn->product_view[0].Price = txn->get_product_response.PriceUsd;

        getRecommendations(txn);
    }
    else
    {
        log_debug("Default Currency is %s. Do convertCurrencyOfProduct", defaultCurrency);
        if (txn->productViewCntr != 0)
        {
            txn->product_view[txn->productViewCntr - 1].Item = txn->get_product_response;
            txn->product_view[txn->productViewCntr - 1].Price = txn->currency_conversion_result;

            getRecommendations(txn);
        }

        int size = 1;
        if (txn->productViewCntr < size)
        {
            strcpy(txn->currency_conversion_req.ToCode, defaultCurrency);
            strcpy(txn->currency_conversion_req.From.CurrencyCode, txn->get_product_response.PriceUsd.CurrencyCode);
            txn->currency_conversion_req.From.Units = txn->get_product_response.PriceUsd.Units;
            txn->currency_conversion_req.From.Nanos = txn->get_product_response.PriceUsd.Nanos;

            strcpy(txn->rpc_handler, "Convert");
	    
	    /**************************/
            txn->caller_fn = FRONTEND;
            txn->next_fn = CURRENCY_SVC;
	    /**************************/

	//log_info("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
	//printf("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
            txn->productViewCntr++;
        }
    }
    return;
}



void insertCart(struct http_transaction *txn){
    
    //PrintPlaceOrderRequest(txn);
    
    char *query = httpQueryParser(txn->request);
    if ( unlikely(!txn || !txn->request)) {
        log_error("insertCart: invalid txn or request");
        //returnResponse(txn);
        return;
    } 
    
    //char aux[HTTP_MSG_LENGTH_MAX];
    ////char *query = httpQueryParser(txn->request, aux, HTTP_MSG_LENGTH_MAX);
    //if ( !query ) {
    //        log_error("httpQueryParser retornou NULL");
    //        exit(1);
    //        //return;
    //}
    
   //log_info("Query: %s", query);

    char *req = txn->request;
    ////char tmp[600]; 
    //char tmp[HTTP_MSG_LENGTH_MAX+1]; 
    ////strcpy(tmp, req);
    //strncpy(tmp, req, sizeof(tmp) -1);
    //tmp[sizeof(tmp) -1] = '\0';
    //
    //char *saveptr = NULL;
    ////char *start_of_path = strtok(tmp, " ");
    //char *start_of_path = strtok_r(tmp, " ", &saveptr);
    //if( unlikely( start_of_path == NULL)){
    //    log_error("start_of_path == NULL, erro no strtok");
    //    //returnResponse(txn);
    //    return;
    //	//exit(1);
    //}

    ////start_of_path = strtok(NULL, " ");
    //start_of_path = strtok_r(NULL, " ", &saveptr);
    //if( unlikely(start_of_path == NULL )){
    //    log_error("start_of_path == NULL, erro no strtok");
    //    //returnResponse(txn);
    //    return;
    //	//exit(1);
    //}
    ////printf("%s\n", start_of_path); 

    ////char *query = strchr(start_of_path, '?') + 1;
    //char *query = find_char(start_of_path, '?');
    ////if( query == NULL){
    //if( unlikely(!query  || *(query+1) == '\0')){
    //	log_error("query == NULL, erro em strchr");
    //    //returnResponse(txn);
    //    return;
    //    //exit(1);
    //}
    ////char temp_req[HTTP_MSG_LENGTH_MAX];
    ////strcpy(temp_req, txn->request);
    ////strncpy(temp_req, txn->request, sizeof(txn->request));

    //query +=1;
    AddItemRequest *in = &txn->add_item_request;

    if (strstr(req, "/1/cart?") != NULL && strstr(req, "POST")){

        // log_debug("Query : %s", query);
        //log_info("Query : %s", query);
	
        char *start_of_quantity = strchr(query, '&') + 1;
        //char *start_of_quantity = find_char(query, '&') + 1;
	if(unlikely(!start_of_quantity)){
		log_error("start_of_quantity == NULL, erro com strchr");
        	//returnResponse(txn);
		return;
		//exit(1);
	}
        
	//in->Item.Quantity = atoi(strchr(start_of_quantity, '=') + 1);
	in->Item.Quantity = atoi(find_char(start_of_quantity, '=') + 1);
        //strcpy(txn->get_product_request.Id, strchr(start_of_product_id, '=') + 1);
        
	//log_debug("Product Quantity: %d", in->Item.Quantity);
        //log_info("Product Quantity: %d", in->Item.Quantity);
        // product_id=66VCHSJNUP&quantity=1
    }
    else{

        log_warn("HTTP Query cannot be parsed!");
        log_warn("\t#### %s", query);
        returnResponse(txn);
	//free(query);
        return;
    }

    strcpy(in->UserId, "ucr-students");
    strcpy(in->Item.ProductId, txn->get_product_request.Id);

    strcpy(txn->rpc_handler, "AddItem");

    /**************************/
    if (txn->caller_fn != FRONTEND)
    	txn->caller_fn = FRONTEND;
    txn->next_fn = CART_SVC;
    /**************************/

	//log_info("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
	//printf("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
    txn->hop_count++;

    //free(query);
}

void getShippingQuote(struct http_transaction *txn){
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

    /**************************/
    txn->caller_fn = FRONTEND;
    txn->next_fn = SHIPPING_SVC;
    /**************************/

	//log_info("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
    txn->hop_count++;
}

// Convert currency for products in the cart
void convertCurrencyOfCart(struct http_transaction *txn){
    if (strcmp(defaultCurrency, "USD") == 0)
    {
        log_debug("Default Currency is USD. Skip convertCurrencyOfCart");
        calculateTotalPrice(txn);
        return;
    }
    else
    {
        if (txn->cartItemCurConvertCntr != 0)
        {
            txn->cart_item_view[txn->cartItemCurConvertCntr - 1].Price = txn->currency_conversion_result;
        }

        if (txn->cartItemCurConvertCntr < txn->cartItemViewCntr)
        {
            log_debug("Default Currency is %s. Do convertCurrencyOfCart", defaultCurrency);
            strcpy(txn->currency_conversion_req.ToCode, defaultCurrency);
            txn->currency_conversion_req.From = txn->cart_item_view[txn->cartItemCurConvertCntr].Price;

            strcpy(txn->rpc_handler, "Convert");
	    /**************************/
            txn->caller_fn = FRONTEND;
            txn->next_fn = CURRENCY_SVC;
	    /**************************/

	//log_info("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
	//printf("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
            txn->cartItemCurConvertCntr++;
            return;
        }
        else
        {
            calculateTotalPrice(txn);
            return;
        }
    }
}

void getCartItemInfo(struct http_transaction *txn){
    log_debug("%d items in the cart.", txn->get_cart_response.num_items);
    if (txn->get_cart_response.num_items <= 0)
    {
        log_debug("None items in the cart.");
        txn->total_price.Units = 0;
        txn->total_price.Nanos = 0;
        returnResponse(txn);
        return;
    }

    if (txn->cartItemViewCntr != 0)
    {
        txn->cart_item_view[txn->cartItemViewCntr - 1].Item = txn->get_product_response;
        txn->cart_item_view[txn->cartItemViewCntr - 1].Quantity =
            txn->get_cart_response.Items[txn->cartItemViewCntr - 1].Quantity;
        txn->cart_item_view[txn->cartItemViewCntr - 1].Price = txn->get_product_response.PriceUsd;
    }

    if (txn->cartItemViewCntr < txn->get_cart_response.num_items)
    {
        strcpy(txn->get_product_request.Id, txn->get_cart_response.Items[txn->cartItemViewCntr].ProductId);
        // log_debug("Product ID: %s", txn->get_product_request.Id);

        strcpy(txn->rpc_handler, "GetProduct");
	
	/**************************/
        txn->caller_fn = FRONTEND;
        txn->next_fn = PRODUCTCATA_SVC;
	/**************************/

	//log_info("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
	//printf("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
        txn->cartItemViewCntr++;
    }
    else
    {
        txn->cartItemCurConvertCntr = 0;
        convertCurrencyOfCart(txn);
        txn->hop_count++;
    }
}

// Convert currency for a ShippingQuote
void convertCurrencyOfShippingQuote(struct http_transaction *txn){
    if (strcmp(defaultCurrency, "USD") == 0)
    {
        log_debug("Default Currency is USD. Skip convertCurrencyOfShippingQuote");
        txn->get_quote_response.conversion_flag = true;
    }
    else
    {
        if (txn->get_quote_response.conversion_flag == true)
        {
            txn->get_quote_response.CostUsd = txn->currency_conversion_result;
            log_debug("Write back convertCurrencyOfShippingQuote");
        }
        else
        {
            log_debug("Default Currency is %s. Do convertCurrencyOfShippingQuote", defaultCurrency);
            strcpy(txn->currency_conversion_req.ToCode, defaultCurrency);
            strcpy(txn->currency_conversion_req.From.CurrencyCode, txn->get_quote_response.CostUsd.CurrencyCode);
            txn->currency_conversion_req.From.Units = txn->get_quote_response.CostUsd.Units;
            txn->currency_conversion_req.From.Nanos = txn->get_quote_response.CostUsd.Nanos;

            strcpy(txn->rpc_handler, "Convert");

	    /**************************/
	    txn->caller_fn = FRONTEND;
            txn->next_fn = CURRENCY_SVC;
	    /**************************/
	//log_info("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
	//printf("caller_fn:%d | next_fn:%d\n", txn->caller_fn, txn->next_fn);
        }
    }
    return;
}

void calculateTotalPrice(struct http_transaction *txn){
    log_debug("Calculating total price...");
    int i = 0;
    for (i = 0; i < txn->cartItemViewCntr; i++)
    {
        MultiplySlow(&txn->cart_item_view[i].Price, txn->cart_item_view[i].Quantity);
        Sum(&txn->total_price, &txn->cart_item_view[i].Price);
    }
    Sum(&txn->total_price, &txn->get_quote_response.CostUsd);
    returnResponse(txn);
    return;
}
