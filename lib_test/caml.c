
#include <caml/mlvalues.h>
#include <caml/callback.h>
#include <stdio.h>
#include <err.h>

void 
caml_init(void)
{
    char *argv[] = { "lwip", NULL };
    value *cb;
    fprintf(stderr, "caml_main\n");
    caml_main(argv);
    cb = caml_named_value("lwip_main");
    if (!cb)
        err(1, "no lwip_main callback found\n");
    fprintf(stderr, "caml_cb: lwip_main\n");
    caml_callback(*cb, Val_unit);
}
