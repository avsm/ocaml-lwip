
type tcp_pcb
type netif
type ipv4_addr = int * int * int * int

external netif_new: ipv4_addr -> ipv4_addr -> ipv4_addr -> netif = "caml_netif_new"
external netif_set_default: netif -> unit = "caml_netif_set_default"
external netif_set_up: netif -> unit = "caml_netif_set_up"
external netif_select: netif -> int = "caml_netif_select"

external tcp_new: unit -> tcp_pcb = "caml_tcp_new"
external tcp_bind: tcp_pcb -> ipv4_addr -> int -> unit = "caml_tcp_bind"
external tcp_listen: tcp_pcb -> (tcp_pcb -> unit) -> unit = "caml_tcp_listen"
external tcp_accepted: tcp_pcb -> unit = "caml_tcp_accepted"

