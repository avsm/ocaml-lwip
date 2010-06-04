open Lwt
open Lwt_unix
open Lwip
open Printf

let host_ip = ( 192, 168, 0, 2 )
let host_netmask = ( 255, 255, 255, 0 )
let host_gw = ( 192, 168, 0, 1 )

let g () = Gc.compact()

let accept_fn listen_q pcb =
    print_endline "accept_fn: start";
    g();
    listen_q := pcb :: !listen_q;
    print_endline "accept_fn: done"

let rec listen_forever listen_q pcb connection_fn =
    (* the listen q gets filled with new connections *)
    let t,u = Lwt.wait () in
    lwt x = Lwt.join [ t ] in
    (* woken up as listen_q has more *)
    Printf.printf "listen_forever: woken up: %d\n%!" (List.length !listen_q);
    (* be careful with the listen q here as no locking, so musnt
       call into lwip too early, with connection_fn will likely do *)
    let rec spawn_threads acc =
       match !listen_q with
       | [] -> acc
       | hd :: tl ->
            listen_q := tl;
            let t = connection_fn hd in
            spawn_threads (t :: acc)
    in
    let accepts = spawn_threads [] in
    let listener = listen_forever listen_q pcb connection_fn in
    Lwt.join (listener :: accepts)

let process_connection pcb =
    print_endline "process_connection: start";
    lwt x = Lwt_unix.sleep 4. in
    print_endline "process_connection: end";
    return ()
    
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
    tcp_listen pcb (accept_fn listen_q);
    g();
    listen_forever listen_q pcb process_connection

let _ = 
    Lwt_main.run (lwip_main ())
