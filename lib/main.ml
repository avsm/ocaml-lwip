open Lwip

let g () = Gc.compact()

let accept_fn pcb =
    print_endline "accept_fn: start";
    g()

let lwip_main () =
    print_endline "lwip_main in ocaml: start";
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
