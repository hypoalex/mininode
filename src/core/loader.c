/*
 *  Node.js-like module loading framework for Duktape
 *
 *  https://nodejs.org/api/modules.html
 */


#include "duktape.h"
#include "mininode.h"
#include "builtin_hash.h"

static duk_int_t mn__eval_module_source(duk_context *ctx, void *udata);

static void mn__push_module_object(duk_context *ctx, const char *id, duk_bool_t main);

static duk_ret_t mn__unimplemented_module(duk_context *ctx) {
  (void) duk_error(ctx, DUK_ERR_TYPE_ERROR, "module is unimplemented");
  return 0;
}

static duk_bool_t
mn__get_cached_module(duk_context *ctx, const char *id) {
	duk_push_global_stash(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "requireCache");
	if (duk_get_prop_string(ctx, -1, id)) {
		duk_remove(ctx, -2);
		duk_remove(ctx, -2);
		return 1;
	} else {
		duk_pop_3(ctx);
		return 0;
	}
}

/*
 * Place a `module` object on the top of the value stack into the require cache
 * based on its `.id` property.  As a convenience to the caller, leave the
 * object on top of the value stack afterwards.
 */
static void
mn__put_cached_module(duk_context *ctx) {
	/* [ ... module ] */
	duk_push_global_stash(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "requireCache");
	duk_dup(ctx, -3);

	/* [ ... module stash req_cache module ] */

	(void) duk_get_prop_string(ctx, -1, "id");
	duk_dup(ctx, -2);
	duk_put_prop(ctx, -4);

	duk_pop_3(ctx);  /* [ ... module ] */
}

static void
mn__del_cached_module(duk_context *ctx, const char *id) {
	duk_push_global_stash(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "requireCache");
	duk_del_prop_string(ctx, -1, id);
	duk_pop_2(ctx);
}

static duk_ret_t
mn__handle_require(duk_context *ctx) {
	const char *id;
	const char *parent_id;
	builtin *module = NULL;
	duk_idx_t module_idx;
	duk_idx_t stash_idx;
	duk_int_t ret;

	duk_push_global_stash(ctx);
	stash_idx = duk_normalize_index(ctx, -1);

	duk_push_current_function(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "moduleId");
	parent_id = duk_require_string(ctx, -1);
	(void) parent_id;  /* not used directly; suppress warning */

	/* [ id stash require parent_id ] */

	id = duk_require_string(ctx, 0);

	/*
	 * The fast path here is for built-in modules, we just do a 
	 * hash lookup and call the entry point if we find one.
	 */
	module = find_builtin(id, strlen(id));

	if (module) {
		duk_pop_n(ctx, 3); /* Clean up the stack! */
		module->loader(ctx);
		return 1; /* Return the module object. */
	}

	(void) duk_get_prop_string(ctx, stash_idx, "\xff" "modResolve");
	duk_dup(ctx, 0);   /* module ID */
	duk_dup(ctx, -3);  /* parent ID */
	duk_call(ctx, 2);

	/* [ ... stash ... resolved_id ] */

	id = duk_require_string(ctx, -1);

	if (mn__get_cached_module(ctx, id)) {
		goto have_module;  /* use the cached module */
	}

	mn__push_module_object(ctx, id, 0 /*main*/);
	mn__put_cached_module(ctx);  /* module remains on stack */

	/*
	 *  From here on out, we have to be careful not to throw.  If it can't be
	 *  avoided, the error must be caught and the module removed from the
	 *  require cache before rethrowing.  This allows the application to
	 *  reattempt loading the module.
	 */

	module_idx = duk_normalize_index(ctx, -1);

	/* [ ... stash ... resolved_id module ] */

	(void) duk_get_prop_string(ctx, stash_idx, "\xff" "modLoad");
	duk_dup(ctx, -3);  /* resolved ID */
	(void) duk_get_prop_string(ctx, module_idx, "exports");
	duk_dup(ctx, module_idx);
	ret = duk_pcall(ctx, 3);
	if (ret != DUK_EXEC_SUCCESS) {
		mn__del_cached_module(ctx, id);
		duk_throw(ctx);  /* rethrow */
	}

	if (duk_is_string(ctx, -1)) {
		duk_int_t ret;

		/* [ ... module source ] */

		ret = duk_safe_call(ctx, mn__eval_module_source, NULL, 2, 1);

		if (ret != DUK_EXEC_SUCCESS) {
			mn__del_cached_module(ctx, id);
			duk_throw(ctx);  /* rethrow */
		}
	} else if (duk_is_undefined(ctx, -1)) {
		duk_pop(ctx);
	} else {
		mn__del_cached_module(ctx, id);
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "invalid module load callback return value");
	}

	/* fall through */

 have_module:
	/* [ ... module ] */

	(void) duk_get_prop_string(ctx, -1, "exports");
	return 1;
}

static void
mn__push_require_function(duk_context *ctx, const char *id) {
	duk_push_c_function(ctx, mn__handle_require, 1);
	duk_push_string(ctx, "name");
	duk_push_string(ctx, "require");
	duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE);
	duk_push_string(ctx, id);
	duk_put_prop_string(ctx, -2, "\xff" "moduleId");

	/* require.cache */
	duk_push_global_stash(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "requireCache");
	duk_put_prop_string(ctx, -3, "cache");
	duk_pop(ctx);

	/* require.main */
	duk_push_global_stash(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "mainModule");
	duk_put_prop_string(ctx, -3, "main");
	duk_pop(ctx);
}

static void
mn__push_module_object(duk_context *ctx, const char *id, duk_bool_t main) {
	duk_push_object(ctx);

	/* Set this as the main module, if requested */
	if (main) {
		duk_push_global_stash(ctx);
		duk_dup(ctx, -2);
		duk_put_prop_string(ctx, -2, "\xff" "mainModule");
		duk_pop(ctx);
	}
	/*
	 * Node.js uses the canonicalized filename of a module for both 
	 * module.id and module.filename.  We have no concept of a file
	 * system here, so just use the module ID for both values.
	 */
	duk_push_string(ctx, id);
	duk_dup(ctx, -1);
	duk_put_prop_string(ctx, -3, "filename");
	duk_put_prop_string(ctx, -2, "id");

	/* module.exports = {} */
	duk_push_object(ctx);
	duk_put_prop_string(ctx, -2, "exports");

	/* module.loaded = false */
	duk_push_false(ctx);
	duk_put_prop_string(ctx, -2, "loaded");

	/* module.require */
	mn__push_require_function(ctx, id);
	duk_put_prop_string(ctx, -2, "require");
}

static duk_int_t
mn__eval_module_source(duk_context *ctx, void *udata) {

	/*
	 *  Stack: [ ... module source ]
	 */

	(void) udata;

	/* Wrap the module code in a function expression.  This is the simplest
	 * way to implement CommonJS closure semantics and matches the behavior of
	 * e.g. Node.js.
	 */
	duk_push_string(ctx, "(function(exports,require,module,__filename,__dirname){");
	duk_dup(ctx, -2);  /* source */
	duk_push_string(ctx, "})");
	duk_concat(ctx, 3);

	/* [ ... module source func_src ] */

	(void) duk_get_prop_string(ctx, -3, "filename");
	duk_compile(ctx, DUK_COMPILE_EVAL);
	duk_call(ctx, 0);

	/* [ ... module source func ] */

	/* Set name for the wrapper function. */
	duk_push_string(ctx, "name");
	duk_push_string(ctx, "main");
	duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_FORCE);

	/* call the function wrapper */
	(void) duk_get_prop_string(ctx, -3, "exports");   /* exports */
	(void) duk_get_prop_string(ctx, -4, "require");   /* require */
	duk_dup(ctx, -5);                                 /* module */
	(void) duk_get_prop_string(ctx, -6, "filename");  /* __filename */
	duk_push_undefined(ctx);                          /* __dirname */
	duk_call(ctx, 5);

	/* [ ... module source result(ignore) ] */

	/* module.loaded = true */
	duk_push_true(ctx);
	duk_put_prop_string(ctx, -4, "loaded");

	/* [ ... module source retval ] */

	duk_pop_2(ctx);

	/* [ ... module ] */

	return 1;
}

/* Load a module as the 'main' module. */
duk_ret_t
mn_module_eval_main(duk_context *ctx, const char *filename) {
	/*
	 *  Stack: [ ... source ]
	 */

	mn__push_module_object(ctx, filename, 1 /*main*/);
	/* [ ... source module ] */

	duk_dup(ctx, 0);
	/* [ ... source module source ] */

	return duk_safe_call(ctx, mn__eval_module_source, NULL, 2, 1);
}

void
mn_module_loader_init(duk_context *ctx) {
	/*
	 * Stack: [ ... options ] => [ ... ]
	 */
	duk_idx_t options_idx;

	/* error on next line before setting up requireCache */
	duk_require_object_coercible(ctx, -1);
	options_idx = duk_require_normalize_index(ctx, -1);
	/* 
	 * Initialize the require cache to a fresh object.
	 */
	duk_push_global_stash(ctx);
	duk_push_object(ctx);
	duk_put_prop_string(ctx, -2, "\xff" "requireCache");
	duk_pop(ctx);
	/*
	 * Stash callbacks for later use.
	 */
	duk_push_global_stash(ctx);
	duk_get_prop_string(ctx, options_idx, "resolve");
	duk_require_function(ctx, -1);
	duk_put_prop_string(ctx, -2, "\xff" "modResolve");
	duk_get_prop_string(ctx, options_idx, "load");
	duk_require_function(ctx, -1);
	duk_put_prop_string(ctx, -2, "\xff" "modLoad");
	duk_pop(ctx);
	/*
	 * Stash main module.
	 */
	duk_push_global_stash(ctx);
	duk_push_undefined(ctx);
	duk_put_prop_string(ctx, -2, "\xff" "mainModule");
	duk_pop(ctx);
	/*
	 * register `require` as a global function.
	 */
	duk_push_global_object(ctx);
	duk_push_string(ctx, "require");
	mn__push_require_function(ctx, "");
	duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE |
	                      DUK_DEFPROP_SET_WRITABLE |
	                      DUK_DEFPROP_SET_CONFIGURABLE);
	duk_pop(ctx);
	duk_pop(ctx); /* pop argument */
}
