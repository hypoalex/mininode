#include "mininode.h"
#include <unistd.h>
#include <fcntl.h>

duk_ret_t mn_bi_fs_access(duk_context *ctx) {
	const int nargs = duk_get_top(ctx);
	const char *path = NULL;
	uv_fs_t *req = NULL;
	int mode = 0;

	if (nargs == 3) {
		mode = duk_require_int(ctx, -2);
		path = duk_require_string(ctx, -3);
	} else if (nargs == 2){
		mode = F_OK;
		path = duk_require_string(ctx, -2);
	} else if (nargs < 2) {
		duk_pop_n(ctx, nargs);
		duk_push_error_object(
			ctx,
			DUK_ERR_TYPE_ERROR,
			"invalid arguments"
		);
		duk_push_string(ctx, "src/modules/fs/methods/access.c");
		duk_put_prop_string(ctx, -2, "fileName");
		duk_push_int(ctx, 17);
		duk_put_prop_string(ctx, -2, "lineNumber");
		duk_throw(ctx);
		return 1;
	}

	req = duk_push_fixed_buffer(ctx, sizeof(*req));
	req->data = mn_setup_req(ctx, -2);
	uv_fs_access(mn_loop, req, path, mode, mn_fs_cb);

	if (req->result != 0) { 
		mn_push_error_result(ctx, req);
		duk_throw(ctx);
		return 1;
	} else {
		return 0;
	}
}
