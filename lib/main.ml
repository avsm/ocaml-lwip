open Lwip

let host_ip = ( 192, 168, 0, 2 )
let host_netmask = ( 255, 255, 255, 0 )
let host_gw = ( 192, 168, 0, 1 )

let g () = Gc.compact()

let accept_fn pcb =
    print_endline "accept_fn: start";
    g()

let lwip_main () =
    print_endline "lwip_main in ocaml: start";
    let netif = netif_new host_ip host_netmask host_gw in
    g();
    netif_set_default netif;
    g();
    netif_set_up netif;
    g();
    let pcb = tcp_new () in
    g();
    let ip = ( 0, 0, 0, 0 ) in
    tcp_bind pcb ip 7;
    g();
    tcp_listen pcb accept_fn;
    g();
    print_endline "lwip_main: done"

let _ = 
    Callback.register "lwip_main" lwip_main;
    print_endline "callback registered"
