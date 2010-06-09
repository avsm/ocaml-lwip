
type tcp_pcb
type ipv4_addr = int * int * int * int

type netif
external netif_new: ipv4_addr -> ipv4_addr -> ipv4_addr -> netif = "caml_netif_new"
external netif_set_default: netif -> unit = "caml_netif_set_default"
external netif_set_up: netif -> unit = "caml_netif_set_up"
external netif_select: netif -> int = "caml_netif_select"

external tcp_new: unit -> tcp_pcb = "caml_tcp_new"
external tcp_bind: tcp_pcb -> ipv4_addr -> int -> unit = "caml_tcp_bind"
external tcp_listen: tcp_pcb -> (tcp_pcb -> unit) -> unit = "caml_tcp_listen"
external tcp_accepted: tcp_pcb -> unit = "caml_tcp_accepted"
external tcp_set_state: tcp_pcb -> 'a -> unit = "caml_tcp_set_state"
external tcp_get_state: tcp_pcb -> 'a = "caml_tcp_get_state"
external tcp_recved: tcp_pcb -> int -> unit = "caml_tcp_recved"

external tcp_rx_read: tcp_pcb -> string = "caml_tcp_rx_read"
external tcp_rx_len: tcp_pcb -> int = "caml_tcp_rx_len"

external timer_tcp: unit -> unit = "caml_timer_tcp"
external timer_ip_reass: unit -> unit = "caml_timer_ip_reass"
external timer_etharp: unit -> unit = "caml_timer_etharp"

external lwip_init: unit -> unit = "caml_lwip_init"

