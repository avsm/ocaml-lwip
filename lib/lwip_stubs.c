#include <lwip/api.h>
#include <lwip/tcp.h>

#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/fail.h>
#include <caml/alloc.h>
#include <caml/custom.h>
#include <caml/signals.h>
#include <stdio.h>

enum tcp_states
{
   TCP_NONE = 0,
   TCP_ACCEPTED,
   TCP_RECEIVED,
   TCP_CLOSING
};

typedef struct tcp_desc {
   u8_t state;
   u8_t retries;
} tcp_desc;

typedef struct tcp_wrap {
    struct tcp_pcb *pcb;
    value v;
    tcp_desc *desc;
} tcp_wrap;

#define Tcp_wrap_val(x) (*((tcp_wrap **)(Data_custom_val(x))))

static tcp_wrap *
tcp_wrap_alloc(struct tcp_pcb *pcb)
{
    fprintf(stderr, "tcp_wrap_alloc\n");
    tcp_wrap *tw = caml_stat_alloc(sizeof(tcp_wrap));
    tw->pcb = pcb;
    tw->v = Val_unit;
    tw->desc = caml_stat_alloc(sizeof(tcp_desc));
    tw->desc->state = TCP_NONE;
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
tcp_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    err_t ret_err;
    tcp_wrap *tw;
    tcp_wrap *ltw = (tcp_wrap *)arg;
    value r, v_tw;

    fprintf(stderr, "tcp_accept_cb\n");

    tcp_setprio(newpcb, TCP_PRIO_MIN);   

    v_tw = caml_alloc_final(2, tcp_wrap_finalize, 1, 100);
    Tcp_wrap_val(v_tw) = NULL;
    tw = tcp_wrap_alloc(newpcb);
    tw->desc->state = TCP_ACCEPTED;
    Tcp_wrap_val(v_tw) = tw;
    
    r = caml_callback(ltw->v, v_tw);

    return ERR_OK;
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
    fprintf(stderr, "tcp_arg inc\n");
    tcp_arg(tw->pcb, tw); /* TODO: not sure if need to reregister, check */
    fprintf(stderr, "calling tcp_accept\n");
    tcp_accept(tw->pcb, tcp_accept_cb);
    CAMLreturn(Val_unit);
}

CAMLprim 
caml_tcp_accepted(value v_tw)
{
    CAMLparam1(v_tw);
    fprintf(stderr, "caml_tcp_accepted\n");
    tcp_accepted(Tcp_wrap_val(v_tw)->pcb);
    CAMLreturn(Val_unit);
}
