
type tcp_pcb
type ipv4_addr = int * int * int * int

external tcp_new: unit -> tcp_pcb = "caml_tcp_new"
external tcp_bind: tcp_pcb -> ipv4_addr -> int -> unit = "caml_tcp_bind"
external tcp_listen: tcp_pcb -> (tcp_pcb -> unit) -> unit = "caml_tcp_listen"
external tcp_accepted: tcp_pcb -> unit = "caml_tcp_accepted"
