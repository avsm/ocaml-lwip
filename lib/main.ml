open Lwt
open Lwt_unix
open Lwip
open Printf

let host_ip = ( 192, 168, 0, 2 )
let host_netmask = ( 255, 255, 255, 0 )
let host_gw = ( 192, 168, 0, 1 )

let g () = Gc.compact()

let stop_tcp_timer = ref false
let rec timer_tcp_loop () =
    lwt () = Lwt_unix.sleep 0.1 in
    timer_tcp ();
    if !stop_tcp_timer then return () else
    timer_tcp_loop ()

let stop_ip_timer = ref false
let rec timer_ip_loop () = 
    lwt () = Lwt_unix.sleep 1. in
    timer_ip_reass ();
    if !stop_ip_timer then return () else
    timer_ip_loop ()

let stop_etharp_timer = ref false
let rec timer_etharp_loop () = 
    lwt () = Lwt_unix.sleep 5. in
    timer_etharp ();
    if !stop_etharp_timer then return () else
    timer_etharp_loop ()

let stop_netif_timer = ref false
let rec netif_select_loop netif =
    lwt () = Lwt_unix.sleep 0.01 in
    let _ = netif_select netif in
    if !stop_netif_timer then return () else
    netif_select_loop netif

type tcp_state = {
    rx_notify: unit -> unit;
    tx_notify: unit -> unit;
    rx_cond: unit Lwt_condition.t;
    tx_cond: unit Lwt_condition.t;
}

let accept_fn listen_q listen_cond pcb =
    print_endline "accept_fn: start";
    g();
    listen_q := pcb :: !listen_q;
    let rx_cond = Lwt_condition.create () in
    let tx_cond = Lwt_condition.create () in
    let rx_notify () = print_endline "rx_notify"; Lwt_condition.signal rx_cond () in
    let tx_notify () = print_endline "tx_notify"; Lwt_condition.signal tx_cond () in
    tcp_set_state pcb { 
      rx_cond = rx_cond; tx_cond = tx_cond;
      rx_notify = rx_notify; tx_notify = tx_notify };
    print_endline "accept_fn: done";
    Lwt_condition.signal listen_cond ()

let rec listen_forever listen_q listen_cond pcb connection_fn =
    (* the listen q gets filled with new connections *)
    lwt () = 
      if List.length !listen_q = 0 then
        Lwt_condition.wait listen_cond
    else
        return () in
    (* woken up as listen_q has more *)
    Printf.printf "listen_forever: woken up: %d\n%!" (List.length !listen_q);
    (* be careful with the listen q here as no locking, so musnt
       call into lwip too early, with connection_fn will likely do *)
    let rec spawn_threads acc =
       match !listen_q with
       | [] -> acc
       | hd :: tl ->
            listen_q := tl;
            tcp_accepted hd;
            let t = connection_fn hd in
            spawn_threads (t :: acc)
    in
    let accepts = spawn_threads [] in
    let listener = listen_forever listen_q listen_cond pcb connection_fn in
    print_endline "joining";
    Lwt.join (listener :: accepts)

let process_connection pcb =
    print_endline "process_connection: start";
    let state = tcp_get_state pcb in
    let rec read_and_echo () = 
       if tcp_read_len pcb > 0 then (
          let buf = tcp_read pcb in
          print_endline ("read_and_echo: " ^ buf);
          let wr = tcp_write pcb buf in
          printf "wr=%d\n" wr
       );
       print_endline "process_connection: waiting rx_cond";
       Lwt_condition.wait state.rx_cond >>
       read_and_echo ()
    in
    read_and_echo ()
    
let lwip_main () =
    print_endline "lwip_main in ocaml: start";
    lwip_init ();
    let netif = netif_new host_ip host_netmask host_gw in
    g();
    netif_set_default netif;
    g();
    netif_set_up netif;
    g();
    print_endline "netif: complete";
    let pcb = tcp_new () in
    g();
    let ip = ( 0, 0, 0, 0 ) in
    tcp_bind pcb ip 7;
    g();
    let listen_q = ref [] in
    let listen_cond = Lwt_condition.create () in
    tcp_listen pcb (accept_fn listen_q listen_cond);
    g();
    let timers = [ timer_tcp_loop () ; timer_ip_loop (); timer_etharp_loop (); netif_select_loop netif ] in
    let listener = listen_forever listen_q listen_cond pcb process_connection in
    Lwt.join (listener :: timers)

let _ = 
    Lwt_main.run (lwip_main ())
