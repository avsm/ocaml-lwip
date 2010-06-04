OCAMLMAKEFILE=OCamlMakefile

SOURCES=lwip_stubs.c lwip.ml main.ml
RESULT=lwip
INCDIRS = ../lwip/src/include/ ../lwip/src/include/ipv4 ../lib_test/include ../lib_test 

lwiplib.o: ncl
	ocamlopt -output-obj -o $@ lwip.cmx main.cmx

include $(OCAMLMAKEFILE)
