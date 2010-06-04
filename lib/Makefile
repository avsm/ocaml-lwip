OCAMLMAKEFILE=OCamlMakefile

SOURCES=lwip_stubs.c lwip.ml main.ml
RESULT=lwip
INCDIRS = ../lwip/src/include/ ../lwip/src/include/ipv4 ../lib_test/include ../lib_test 
PACKS=lwt lwt.unix lwt.syntax
PREDS=camlp4o
LIBDIRS=../lib_test
CLIBS=lwip4
RESULT=echop

#lwiplib.o: ncl
#	ocamlopt -output-obj -o $@ -I +react react.cmx -I +lwt unix.cmxa lwt.cmxa lwt_unix.cmxa lwip.cmx main.cmx

include $(OCAMLMAKEFILE)
