#include "duktape.h"
#include <unistd.h>

duk_ret_t 
mn_bi_fs_access_sync(duk_context *ctx) {
  const int nargs = duk_get_top(ctx);
  const char *path = NULL;
  int mode = 0;
	/* 
	 * This will eventually transition to something
	 * more akin to errno in the stdlib. For now,
	 * we'll do this as a sorta proof of concept.
	 */
	int lineNumber = 0;

	if (!duk_is_function(ctx, -1) || nargs < 2 || nargs > 3) {
		lineNumber = 17;
		goto sadplace;
	}

	if (nargs == 2) {
		mode = F_OK | R_OK;
		path = duk_require_string(ctx, -2);
	} else if (nargs == 3) {
		mode = duk_require_int(ctx, -2);
		path = duk_require_string(ctx, -3);
  } else {
    goto sadplace;
  }

  if (access(path, mode)) {
    
  } else {
  }

  return 0;

sadplace:
  duk_pop_n(ctx, nargs);
  duk_push_error_object(
    ctx,
    DUK_ERR_TYPE_ERROR,
		"invalid arguments"
	);
	duk_push_string(ctx, "src/modules/fs/methods/access_sync.c");
	duk_put_prop_string(ctx, -2, "fileName");
	/*
   * Blaming the branch will provide the info necessary
   * to actually fix the bug in user code, e.g., it is
   * kinda obvious that (nargs < 2 || nargs > 3) was 
	 * the underlying issue for the exception.
	 */
  duk_push_int(ctx, lineNumber);
  duk_put_prop_string(ctx, -2, "lineNumber");
		/*
		 * All Error objects should set fileName & lineNumber.
		 * This is a bit of a maintenance nightmare, but I feel
		 * it's worth the effort to maintain this convention.
		 */
  duk_throw(ctx);
		/*
		 * Note that we can't really return after we throw.
		 * Return values are used to indicate the depth of
		 * the return value stack. It's a meta thing, okay?
		 */
	return 1;

}
