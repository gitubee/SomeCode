/*****************************************************************************
 *
 * Copyright (C) 2001 Uppsala University and Ericsson AB.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Erik Nordström, <erik.nordstrom@it.uu.se>
 *          
 *
 *****************************************************************************/

#ifdef NS_PORT
#include "ns-2/aodv-uu.h"



#else
#include <netinet/in.h>
#include <string.h>
#include "aodv_hello.h"
#include "aodv_timeout.h"
#include "aodv_rrep.h"
#include "aodv_rreq.h"
#include "routing_table.h"
#include "timer_queue.h"
#include "params.h"
#include "aodv_socket.h"
#include "defs.h"
#include "debug.h"

extern int unidir_hack, receive_n_hellos, hello_jittering, optimized_hellos;
static struct timer hello_timer;

#endif


/* #define DEBUG_HELLO */


// 根据ip地址在结构体中找到结构体的下标，返回下标
int NS_CLASS find_node(char* addr){
	int i;
	for(i=0;i<10;i++){
		fprintf(stderr, "find: n_info[%d] = %s\n",i,n_info[i].c_n);
		if(strcmp(n_info[i].c_n, addr) == 0){
			fprintf(stderr, "	* find node result: %d\n",i);
			return(i);
		}
	}
	// this node does not exist
	fprintf(stderr, "	* find node not exist (-2)\n");
	return(-2);
}
// 在结构体中新分配一个来存储这个节点的信息
int NS_CLASS give_node(char* addr){
	int i;
	for(i=0;i<10;i++){
		fprintf(stderr, "give: n_info[%d] = %s\n",i,n_info[i].c_n);
		if(strlen(n_info[i].c_n) == 0){
			fprintf(stderr, "	* give node result:%d\n",i);
			strcpy(n_info[i].c_n, addr);
			return(i);
		}
	}
	// space not enough
	fprintf(stderr, "	* give node no space (-3)\n");
	return(-3);
}
// ni_1是之前时刻
float NS_CLASS calc_1(int index){
	int i=0, j=0, num_1=0, num_2=0, minus=0;
	float result=0.0;
	fprintf(stderr, "***** in ni_1:\n");
	for(i=0;i<10;i++){
		if(strlen((n_info[index].ni_1)[i]) != 0){
			fprintf(stderr, "	(n_info[%d].ni_1)[%d]=%s\n", index, i, (n_info[index].ni_1)[i]);
			num_1++;
		}
	}
	fprintf(stderr, "***** in ni_2:\n");
	for(i=0;i<10;i++){
		if(strlen((n_info[index].ni_2)[i]) != 0){
			fprintf(stderr, "	(n_info[%d].ni_2)[%d]=%s\n", index, i, (n_info[index].ni_2)[i]);
			num_2++;
		}
	}
	for(i=0;i<num_1;i++){
		for(j=0;j<num_2;j++){
			if(strcmp((n_info[index].ni_1)[i], (n_info[index].ni_2)[j]) == 0){
				fprintf(stderr, "	SAME: %s\n",(n_info[index].ni_1)[i]);
				minus++;
			}
		}
	}
	num_1 == 0 ? (result = -2) : (result = 1-minus/num_1);
	fprintf(stderr, "	* num_1 = %d\n",num_1);
	fprintf(stderr, "	* num_2 = %d\n",num_2);
	fprintf(stderr, "	* minus = %d\n",minus);
	return(result);
}
// ni_2是之前时刻
float NS_CLASS calc_2(int index){
	int i=0, j=0, num_1=0, num_2=0, minus=0;
	float result=0.0;
	fprintf(stderr, "***** in ni_1:\n");
	for(i=0;i<10;i++){
		if(strlen(n_info[index].ni_1[i]) != 0){
			fprintf(stderr, "	(n_info[%d].ni_1)[%d]=%s\n", index, i, (n_info[index].ni_1)[i]);
			num_1++;
		}
	}
	fprintf(stderr, "***** in ni_2:\n");
	for(i=0;i<10;i++){
		if(strlen(n_info[index].ni_2[i]) != 0){
			fprintf(stderr, "	(n_info[%d].ni_2)[%d]=%s\n", index, i, (n_info[index].ni_2)[i]);
			num_2++;
		}
	}
	for(j=0;j<num_2;j++){
		for(i=0;i<num_1;i++){
			if(strcmp((n_info[index].ni_1)[i], (n_info[index].ni_2)[j]) == 0){
				fprintf(stderr, "	SAME: %s\n",(n_info[index].ni_2)[i]);
				minus++;
			}
		}
	}
	num_2 == 0 ? (result = -2) : (result = 1-minus/num_2);
	fprintf(stderr, "	* num_1 = %d\n",num_1);
	fprintf(stderr, "	* num_2 = %d\n",num_2);
	fprintf(stderr, "	* minus = %d\n",minus);
	return(result);
}


long NS_CLASS hello_jitter()
{
    if (hello_jittering) {
#ifdef NS_PORT
	return (long) (((float) Random::integer(RAND_MAX + 1) / RAND_MAX - 0.5)
		       * JITTER_INTERVAL);
#else
	return (long) (((float) random() / RAND_MAX - 0.5) * JITTER_INTERVAL);
#endif
    } else
	return 0;
}

void NS_CLASS hello_start()
{
    if (hello_timer.used)
	return;

    gettimeofday(&this_host.fwd_time, NULL);
    DEBUG(LOG_DEBUG, 0, "Starting to send HELLOs!");
	//fprintf(stderr, "-> *** hello start!\n");
    timer_init(&hello_timer, &NS_CLASS hello_send, NULL);

    hello_send(NULL);
}

void NS_CLASS hello_stop()
{
    DEBUG(LOG_DEBUG, 0,
	  "No active forwarding routes - stopped sending HELLOs!");
    timer_remove(&hello_timer);
}

void NS_CLASS hello_send(void *arg)
{
    fprintf(stderr, "-> *** hello send!\n");
    RREP *rrep;
    AODV_ext *ext = NULL;
    u_int8_t flags = 0;
    struct in_addr dest;
    long time_diff, jitter;
    struct timeval now;
    int msg_size = RREP_SIZE;
    int i;

    int index;
    int past_num;
    int prst_num;
    float change;


    gettimeofday(&now, NULL);

    if (optimized_hellos &&
	timeval_diff(&now, &this_host.fwd_time) > ACTIVE_ROUTE_TIMEOUT) {
	hello_stop();
	return;
    }

    time_diff = timeval_diff(&now, &this_host.bcast_time);
    jitter = hello_jitter();

    /* This check will ensure we don't send unnecessary hello msgs, in case
       we have sent other bcast msgs within HELLO_INTERVAL */
    if (time_diff >= HELLO_INTERVAL) {

	for (i = 0; i < MAX_NR_INTERFACES; i++) {
	    if (!DEV_NR(i).enabled)
		continue;
#ifdef DEBUG_HELLO
	    DEBUG(LOG_DEBUG, 0, "sending Hello to 255.255.255.255");
#endif


	    /*   neighbor change rate   */
		fprintf(stderr, "*****************************************************************************\n");
		fprintf(stderr, "	-> current node:%s\n",ip_to_str(DEV_NR(i).ipaddr));			
		find_node(ip_to_str(DEV_NR(i).ipaddr)) == -2 ? (index = give_node(ip_to_str(DEV_NR(i).ipaddr))) : (index = find_node(ip_to_str(DEV_NR(i).ipaddr)));		
		fprintf(stderr, "	-> index = %d\n", index);
		rt_calc_neighbor(index);
		fprintf(stderr, "	-> past = %d\n", n_info[index].past);
		n_info[index].past == 1 ? (change = calc_1(index)) : (change = calc_2(index));	
		n_info[index].past = 3 - n_info[index].past;
		fprintf(stderr, "	-> past(new) = %d\n", n_info[index].past);
		fprintf(stderr, "	-> neighbor change = %lf\n", change);
		fprintf(stderr, "*****************************************************************************\n");

	    rrep = rrep_create(flags, 0, 0, DEV_NR(i).ipaddr,
			       this_host.seqno,
			       DEV_NR(i).ipaddr,
			       ALLOWED_HELLO_LOSS * HELLO_INTERVAL);

	    /* Assemble a RREP extension which contain our neighbor set... */
	    if (unidir_hack) {
		int i;

		if (ext)
		    ext = AODV_EXT_NEXT(ext);
		else
		    ext = (AODV_ext *) ((char *) rrep + RREP_SIZE);

		ext->type = RREP_HELLO_NEIGHBOR_SET_EXT;
		ext->length = 0;

		for (i = 0; i < RT_TABLESIZE; i++) {
		    list_t *pos;
		    list_foreach(pos, &rt_tbl.tbl[i]) {
			rt_table_t *rt = (rt_table_t *) pos;
			/* If an entry has an active hello timer, we assume
			   that we are receiving hello messages from that
			   node... */
			if (rt->hello_timer.used) {
#ifdef DEBUG_HELLO
			    DEBUG(LOG_INFO, 0,
				  "Adding %s to hello neighbor set ext",
				  ip_to_str(rt->dest_addr));
#endif
			    memcpy(AODV_EXT_DATA(ext), &rt->dest_addr,
				   sizeof(struct in_addr));
			    ext->length += sizeof(struct in_addr);
			}
		    }
		}
		if (ext->length)
		    msg_size = RREP_SIZE + AODV_EXT_SIZE(ext);
	    }
	    dest.s_addr = AODV_BROADCAST;
	    aodv_socket_send((AODV_msg *) rrep, dest, msg_size, 1, &DEV_NR(i));
	}

	timer_set_timeout(&hello_timer, HELLO_INTERVAL + jitter);
    } else {
	if (HELLO_INTERVAL - time_diff + jitter < 0)
	    timer_set_timeout(&hello_timer,
			      HELLO_INTERVAL - time_diff - jitter);
	else
	    timer_set_timeout(&hello_timer,
			      HELLO_INTERVAL - time_diff + jitter);
    }
}


/* Process a hello message */
void NS_CLASS hello_process(RREP * hello, int rreplen, unsigned int ifindex)
{
    fprintf(stderr, "- Processing Hello message.\n");
    u_int32_t hello_seqno, timeout, hello_interval = HELLO_INTERVAL;
    u_int8_t state, flags = 0;
    struct in_addr ext_neighbor, hello_dest;
    rt_table_t *rt;
    AODV_ext *ext = NULL;
    int i;
    struct timeval now;

    gettimeofday(&now, NULL);
    hello_dest.s_addr = hello->dest_addr;
    hello_seqno = ntohl(hello->dest_seqno);

    rt = rt_table_find(hello_dest);

    if (rt)
	flags = rt->flags;

    if (unidir_hack)
	flags |= RT_UNIDIR;

    /* Check for hello interval extension: */
    ext = (AODV_ext *) ((char *) hello + RREP_SIZE);

    while (rreplen > (int) RREP_SIZE) {
	switch (ext->type) {
	case RREP_HELLO_INTERVAL_EXT:
	    if (ext->length == 4) {
		memcpy(&hello_interval, AODV_EXT_DATA(ext), 4);
		hello_interval = ntohl(hello_interval);
#ifdef DEBUG_HELLO
		DEBUG(LOG_INFO, 0, "Hello extension interval=%lu!",
		      hello_interval);
#endif

	    } else
		alog(LOG_WARNING, 0,
		     __FUNCTION__, "Bad hello interval extension!");
	    break;
	case RREP_HELLO_NEIGHBOR_SET_EXT:

#ifdef DEBUG_HELLO
	    DEBUG(LOG_INFO, 0, "RREP_HELLO_NEIGHBOR_SET_EXT");
#endif
	    for (i = 0; i < ext->length; i = i + 4) {
		ext_neighbor.s_addr =
		    *(in_addr_t *) ((char *) AODV_EXT_DATA(ext) + i);

		if (ext_neighbor.s_addr == DEV_IFINDEX(ifindex).ipaddr.s_addr)
		    flags &= ~RT_UNIDIR;
	    }
	    break;
	default:
	    alog(LOG_WARNING, 0, __FUNCTION__,
		 "Bad extension!! type=%d, length=%d", ext->type, ext->length);
	    ext = NULL;
	    break;
	}
	if (ext == NULL)
	    break;

	rreplen -= AODV_EXT_SIZE(ext);
	ext = AODV_EXT_NEXT(ext);
    }
    fprintf(stderr, "*** Received HELLO from %s, seqno %lu\n", ip_to_str(hello_dest), hello_seqno);

#ifdef DEBUG_HELLO
    DEBUG(LOG_DEBUG, 0, "rcvd HELLO from %s, seqno %lu",
	  ip_to_str(hello_dest), hello_seqno);
#endif
    /* This neighbor should only be valid after receiving 3
       consecutive hello messages... */
    if (receive_n_hellos)
	state = INVALID;
    else
	state = VALID;

    timeout = ALLOWED_HELLO_LOSS * hello_interval + ROUTE_TIMEOUT_SLACK;

    if (!rt) {
	/* No active or expired route in the routing table. So we add a
	   new entry... */

	rt = rt_table_insert(hello_dest, hello_dest, 1,
			     hello_seqno, timeout, state, flags, ifindex);

	if (flags & RT_UNIDIR) {
	    DEBUG(LOG_INFO, 0, "%s new NEIGHBOR, link UNI-DIR",
		  ip_to_str(rt->dest_addr));
	} else {
	    DEBUG(LOG_INFO, 0, "%s new NEIGHBOR!", ip_to_str(rt->dest_addr));
	}
	rt->hello_cnt = 1;

    } else {

	if ((flags & RT_UNIDIR) && rt->state == VALID && rt->hcnt > 1) {
	    goto hello_update;
	}

	if (receive_n_hellos && rt->hello_cnt < (receive_n_hellos - 1)) {
	    if (timeval_diff(&now, &rt->last_hello_time) <
		(long) (hello_interval + hello_interval / 2))
		rt->hello_cnt++;
	    else
		rt->hello_cnt = 1;

	    memcpy(&rt->last_hello_time, &now, sizeof(struct timeval));
	    return;
	}
	rt_table_update(rt, hello_dest, 1, hello_seqno, timeout, VALID, flags);
    }

  hello_update:

    hello_update_timeout(rt, &now, ALLOWED_HELLO_LOSS * hello_interval);
    return;
}


#define HELLO_DELAY 50		/* The extra time we should allow an hello
				   message to take (due to processing) before
				   assuming lost . */

NS_INLINE void NS_CLASS hello_update_timeout(rt_table_t * rt,
					     struct timeval *now, long time)
{
    timer_set_timeout(&rt->hello_timer, time + HELLO_DELAY);
    memcpy(&rt->last_hello_time, now, sizeof(struct timeval));
}
