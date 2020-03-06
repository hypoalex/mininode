#include <stdio.h>
#include "mininode.h"

/*
 *  Duktape/C functions providing Node.js 'errors' module functionality.
 *  See https://nodejs.org/dist/v6.9.2/docs/api/errors.html
 */

/* Implementation TBD */

/*
 *  Module initialization
 */

const duk_function_list_entry mn_bi_errors_funcs[] = {
	{ NULL, NULL, 0 }
};

const duk_number_list_entry mn_bi_errors_consts[] = {
	{ NULL, 0.0 }
};

duk_ret_t
mn_bi_errors(duk_context *ctx) {
	duk_push_global_object(ctx);
	duk_put_function_list(ctx, -1, mn_bi_errors_funcs);
	duk_put_number_list(ctx, -1, mn_bi_errors_consts);
	return 1;
}
