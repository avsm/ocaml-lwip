/*
 * Copyright (c) 2010 Anil Madhavapeddy <anil@recoil.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <lwip/api.h>
#include <lwip/tcp.h>

#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/fail.h>
#include <caml/alloc.h>
#include <caml/custom.h>
#include <caml/signals.h>
#include <stdio.h>

/* XXX: these are just for lib_test, need to be
   abstracted out for MirageOS */
#include <mintapif.h>
#include <netif/etharp.h>

enum tcp_states
{
   TCP_NONE = 0,
   TCP_LISTEN,
   TCP_ACCEPTED,
   TCP_CLOSING
};

typedef struct tcp_desc {
   u8_t state;        /* TCP state */
   u8_t retries;      /* */
   struct pbuf *rx;   /* pbuf receive queue */
} tcp_desc;

typedef struct tcp_wrap {
    struct tcp_pcb *pcb;
    value v;          /* either accept callback or state record */
    tcp_desc *desc;
} tcp_wrap;

#define Tcp_wrap_val(x) (*((tcp_wrap **)(Data_custom_val(x))))

static tcp_wrap *
tcp_wrap_alloc(struct tcp_pcb *pcb)
{
    fprintf(stderr, "tcp_wrap_alloc\n");
    tcp_wrap *tw = caml_stat_alloc(sizeof(tcp_wrap));
    tw->pcb = pcb;
    tw->v = 0;
    tw->desc = caml_stat_alloc(sizeof(tcp_desc));
    tw->desc->state = TCP_NONE;
    tw->desc->rx = NULL;
    tw->desc->retries = 0;
    return tw;
}

static void
tcp_wrap_finalize(value v_tw)
{
    fprintf(stderr, "tcp_wrap_finalize\n");
    tcp_wrap *tw = Tcp_wrap_val(v_tw);
    if (tw->pcb) {
        tcp_close(tw->pcb);
        tw->pcb = NULL;
    }
    if (tw->desc)
        free(tw->desc);
    if (tw->v)
        caml_remove_generational_global_root(&tw->v);
    free(tw);
}

CAMLprim
caml_tcp_new(value v_unit)
{
    CAMLparam1(v_unit);
    CAMLlocal1(v_tw);
    fprintf(stderr, "tcp_new\n");
    tcp_wrap *tw;
    struct tcp_pcb *pcb = tcp_new();
    if (pcb == NULL)
        caml_failwith("tcp_new: unable to alloc pcb");
    v_tw = caml_alloc_final(2, tcp_wrap_finalize, 1, 100);
    Tcp_wrap_val(v_tw) = NULL;
    tw = tcp_wrap_alloc(pcb);
    Tcp_wrap_val(v_tw) = tw;
    CAMLreturn(v_tw);
}

CAMLprim
caml_tcp_bind(value v_tw, value v_ip, value v_port)
{
    CAMLparam3(v_tw, v_ip, v_port);
    struct ip_addr ip;
    u16_t port = Int_val(v_port);
    err_t e;
    fprintf(stderr, "cam_tcp_bind\n");
    tcp_wrap *tw = Tcp_wrap_val(v_tw);
    IP4_ADDR(&ip, Int_val(Field(v_ip, 0)), Int_val(Field(v_ip, 1)), 
        Int_val(Field(v_ip, 2)), Int_val(Field(v_ip,3)));
    e = tcp_bind(tw->pcb, &ip, port);
    if (e != ERR_OK)
        caml_failwith("tcp_bind: unable to bind");
    CAMLreturn(Val_unit);
}

err_t
tcp_recv_cb(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    tcp_wrap *tw = (tcp_wrap *)arg;
    err_t ret_err;
    if (p == NULL) {
        fprintf(stderr, "tcp_recv_cb: p==NULL, state->CLOSING\n");
        tw->desc->state = TCP_CLOSING;
        /* TODO: flush rx/tx queues to application */
        ret_err = ERR_OK;
    } else if (err != ERR_OK) {
        fprintf(stderr, "tcp_recv_cb: err != ERR_OK\n");
        tw->desc->state = TCP_CLOSING;
        ret_err = ERR_OK;
    } else {
        if (tw->desc->rx == NULL) {
            value ret_unit;
            fprintf(stderr, "tcp_recv_cb: rx first packet\n");
            tw->desc->rx = p; 
            ret_unit = caml_callback(Field(tw->v, 0), Val_unit);
            ret_err = ERR_OK;
        } else if (tw->desc->state == TCP_ACCEPTED) {
            struct pbuf *ptr;
            fprintf(stderr, "tcp_recv_cb: rx chaining packet\n");
            ptr = tw->desc->rx;
            pbuf_chain(ptr, p);
            ret_err = ERR_OK;
        } else if (tw->desc->state == TCP_CLOSING) {
            fprintf(stderr, "tcp_recv_cb: rx TCP_CLOSING noop\n");
            ret_err = ERR_OK;
        } else {
            fprintf(stderr, "tcp_recv_cb: rx unknown else; state=%d\n", tw->desc->state);
            ret_err = ERR_OK;
        }
    }
    return ret_err;
}

err_t
tcp_sent_cb(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    tcp_wrap *tw = (tcp_wrap *)arg;
    err_t ret_err;

    if (len > 0) {
        /* No error, so just notify the application that the send
           succeeded and wake up any blocked listeners */
        value v_unit;
        v_unit = caml_callback(Field(tw->v, 1), Val_unit);
        ret_err = ERR_OK;
    } else {
        /* XXX write error. do something interesting */
        ret_err = ERR_MEM;
    }
    return ret_err;
}

err_t 
tcp_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    err_t ret_err;
    tcp_wrap *tw;
    tcp_wrap *ltw = (tcp_wrap *)arg;
    value v_state, v_tw;

    fprintf(stderr, "tcp_accept_cb: ");

    tcp_setprio(newpcb, TCP_PRIO_MIN);   
    fprintf(stderr, "tcp snd buf = %d CONST=%d\n", tcp_sndbuf(newpcb), TCP_SND_BUF);

    v_tw = caml_alloc_final(2, tcp_wrap_finalize, 1, 100);
    Tcp_wrap_val(v_tw) = NULL;
    tw = tcp_wrap_alloc(newpcb);
    tw->desc->state = TCP_ACCEPTED;
    Tcp_wrap_val(v_tw) = tw;
    tcp_arg(tw->pcb, tw);
    tcp_recv(newpcb, tcp_recv_cb);
    tcp_sent(newpcb, tcp_sent_cb);
    fprintf(stderr, "state=%d\n", tw->desc->state); 
    v_state = caml_callback(ltw->v, v_tw);
    return ERR_OK;
}

CAMLprim
caml_tcp_set_state(value v_tw, value v_arg)
{
    CAMLparam2(v_tw, v_arg);
    struct tcp_wrap *tw = Tcp_wrap_val(v_tw);
    if (tw->v)
        failwith("caml_tcp_set_state: cannot change tw->v");
    tw->v = v_arg;
    caml_register_generational_global_root(&tw->v);
    CAMLreturn(Val_unit);
}

CAMLprim
caml_tcp_get_state(value v_tw)
{
    CAMLparam1(v_tw);
    struct tcp_wrap *tw = Tcp_wrap_val(v_tw);
    if (!tw->v)
        failwith("caml_tcp_get_state: null\n");
    CAMLreturn(tw->v);
}

CAMLprim
caml_tcp_listen(value v_tw, value v_accept_cb)
{
    CAMLparam2(v_tw, v_accept_cb);
    fprintf(stderr, "caml_tcp_listen\n");
    tcp_wrap *tw = Tcp_wrap_val(v_tw);
    struct tcp_pcb *new_pcb;
    new_pcb = tcp_listen(tw->pcb);
    if (new_pcb == NULL)
        caml_failwith("tcp_listen: unable to listen");
    fprintf(stderr, "tcp_listen called\n");
    /* XXX realloc a new tcp pcb wrapper so we can construct tcp_listen_pcb in ocaml */
    tw->pcb = new_pcb;  /* tcp_listen will deallocate the old pcb */
    tw->v = v_accept_cb;
    caml_register_generational_global_root(&tw->v);
    tcp_arg(tw->pcb, tw);
    tw->desc->state = TCP_LISTEN;
    fprintf(stderr, "calling tcp_accept\n");
    tcp_accept(tw->pcb, tcp_accept_cb);
    CAMLreturn(Val_unit);
}

CAMLprim 
caml_tcp_accepted(value v_tw)
{
    CAMLparam1(v_tw);
    struct tcp_wrap *tw = Tcp_wrap_val(v_tw);
    fprintf(stderr, "caml_tcp_accepted\n");
    tw->desc->state = TCP_ACCEPTED;
    tcp_accepted(tw->pcb);
    CAMLreturn(Val_unit);
}

// NetIF support

#define Netif_wrap_val(x) (*((struct netif **)(Data_custom_val(x))))
static void
netif_finalize(value v_netif)
{
    struct netif *netif = Netif_wrap_val(v_netif);
    fprintf(stderr, "netif_finalize\n");
    free(netif);
}

CAMLprim
caml_netif_new(value v_ip, value v_netmask, value v_gw)
{
    CAMLparam3(v_ip, v_netmask, v_gw);
    CAMLlocal1(v_netif);
    struct ip_addr ip, netmask, gw;
    struct netif *netif;
    fprintf(stderr, "caml_netif_new\n");

    IP4_ADDR(&ip, Int_val(Field(v_ip, 0)), Int_val(Field(v_ip, 1)), 
        Int_val(Field(v_ip, 2)), Int_val(Field(v_ip,3)));
    IP4_ADDR(&netmask, Int_val(Field(v_netmask, 0)), Int_val(Field(v_netmask, 1)), 
        Int_val(Field(v_netmask, 2)), Int_val(Field(v_netmask,3)));
    IP4_ADDR(&gw, Int_val(Field(v_gw, 0)), Int_val(Field(v_gw, 1)), 
        Int_val(Field(v_gw, 2)), Int_val(Field(v_gw,3)));

    netif = caml_stat_alloc(sizeof(struct netif));
    netif_add(netif, &ip, &netmask, &gw, NULL, mintapif_init, ethernet_input);
    v_netif = caml_alloc_final(2, netif_finalize, 1, 100);
    Netif_wrap_val(v_netif) = netif;
    
    CAMLreturn(v_netif);
}

/* Copy out all the pbufs in a chain into a string, and ack/free pbuf */
CAMLprim
caml_tcp_read(value v_tw)
{
    CAMLparam1(v_tw);
    CAMLlocal1(v_str);
    struct tcp_wrap *tw = Tcp_wrap_val(v_tw);
    struct pbuf *p = tw->desc->rx, *x = p;
    unsigned char *s;
    fprintf(stderr, "caml_tcp_rx_read\n");
    if (!x) {
        v_str = caml_alloc_string(0);
        CAMLreturn(v_str);
    }
    v_str = caml_alloc_string(p->tot_len);
    s = String_val(v_str);
    do {
        memcpy(s, x->payload, x->len);
        s += x->len;
    } while (x = x->next);
    tcp_recved(tw->pcb, p->tot_len);
    pbuf_free(p);
    tw->desc->rx = NULL;
    CAMLreturn(v_str);   
}

CAMLprim
caml_tcp_read_len(value v_tw)
{
    CAMLparam1(v_tw);
    struct tcp_wrap *tw = Tcp_wrap_val(v_tw);
    if (tw->desc->rx)
        CAMLreturn(Val_int(tw->desc->rx->tot_len));
    else
        CAMLreturn(Val_int(0));
}

CAMLprim
caml_tcp_recved(value v_tw, value v_len)
{
    CAMLparam2(v_tw, v_len);
    struct tcp_wrap *tw = Tcp_wrap_val(v_tw);
    fprintf(stderr, "caml_tcp_recved: %d\n", Int_val(v_len));
    tcp_recved(tw->pcb, Int_val(v_len));
    CAMLreturn(Val_unit);
}

CAMLprim
caml_tcp_write(value v_tw, value v_buf, value v_off, value v_len)
{
    CAMLparam4(v_tw, v_buf, v_off, v_len);
    struct tcp_wrap *tw = Tcp_wrap_val(v_tw);
    err_t err;
    /* XXX no bounds checks on off, len */
    fprintf(stderr, "tcp_write: off=%d len=%d\n", Int_val(v_off), Int_val(v_len));
    err = tcp_write(tw->pcb, String_val(v_buf)+Int_val(v_off), Int_val(v_len), 1);
    if (err == ERR_OK)
       CAMLreturn(Val_int(caml_string_length(v_buf)));
    else
       CAMLreturn(Val_int(-1));
}

CAMLprim
caml_tcp_sndbuf(value v_tw)
{
    CAMLparam1(v_tw);
    struct tcp_wrap *tw = Tcp_wrap_val(v_tw);
    CAMLreturn(Val_int(tcp_sndbuf(tw->pcb)));
}
 
// Netif

CAMLprim
caml_netif_set_default(value v_netif)
{
    CAMLparam1(v_netif);
    fprintf(stderr, "caml_netif_set_default\n");
    netif_set_default( Netif_wrap_val(v_netif) );
    CAMLreturn(Val_unit);
}

CAMLprim
caml_netif_set_up(value v_netif)
{
    CAMLparam1(v_netif);
    fprintf(stderr, "caml_netif_set_up\n");
    netif_set_up( Netif_wrap_val(v_netif) );
    CAMLreturn(Val_unit);
}

CAMLprim
caml_netif_select(value v_netif)
{
    CAMLparam1(v_netif);
    int i = mintapif_select( Netif_wrap_val(v_netif) );
    CAMLreturn(Val_int(i));
}

// Timers

CAMLprim
caml_timer_tcp(value v_unit)
{
    CAMLparam1(v_unit);
    tcp_tmr();
    CAMLreturn(Val_unit);
}

CAMLprim
caml_timer_ip_reass(value v_unit)
{
    CAMLparam1(v_unit);
    ip_reass_tmr();
    CAMLreturn(Val_unit);
}

CAMLprim
caml_timer_etharp(value v_unit)
{
    CAMLparam1(v_unit);
    etharp_tmr();
    CAMLreturn(Val_unit);
}

// LWIP core

CAMLprim
caml_lwip_init(value v_unit)
{
    CAMLparam1(v_unit);
    lwip_init ();
    CAMLreturn(Val_unit);
}

