/*
 * The MIT License
 *
 * Copyright (c) 2014 GitHub, Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "rugged.h"

extern VALUE rb_mRugged;
extern VALUE rb_cRuggedObject;
extern VALUE rb_cRuggedCommit;
VALUE rb_cRuggedWalker;
VALUE rb_cRuggedCommitStats;

static void rb_git_walk__free(git_revwalk *walk)
{
	git_revwalk_free(walk);
}

VALUE rugged_walker_new(VALUE klass, VALUE owner, git_revwalk *walk)
{
	VALUE rb_walk = Data_Wrap_Struct(klass, NULL, &rb_git_walk__free, walk);
	rugged_set_owner(rb_walk, owner);
	return rb_walk;
}

static void push_commit_oid(git_revwalk *walk, const git_oid *oid, int hide)
{
	int error;
	if (hide) error = git_revwalk_hide(walk, oid);
	else error = git_revwalk_push(walk, oid);
	rugged_exception_check(error);
}

static void push_commit_ref(git_revwalk *walk, const char *ref, int hide)
{
	int error;
	if (hide) error = git_revwalk_hide_ref(walk, ref);
	else error = git_revwalk_push_ref(walk, ref);
	rugged_exception_check(error);
}

static void push_commit_1(git_revwalk *walk, VALUE rb_commit, int hide)
{
	if (rb_obj_is_kind_of(rb_commit, rb_cRuggedObject)) {
		git_object *object;
		Data_Get_Struct(rb_commit, git_object, object);

		push_commit_oid(walk, git_object_id(object), hide);
		return;
	}

	Check_Type(rb_commit, T_STRING);

	if (RSTRING_LEN(rb_commit) == 40) {
		git_oid commit_oid;
		if (git_oid_fromstr(&commit_oid, RSTRING_PTR(rb_commit)) == 0) {
			push_commit_oid(walk, &commit_oid, hide);
			return;
		}
	}

	push_commit_ref(walk, StringValueCStr(rb_commit), hide);
}

static void push_commit(git_revwalk *walk, VALUE rb_commit, int hide)
{
	if (rb_type(rb_commit) == T_ARRAY) {
		long i;
		for (i = 0; i < RARRAY_LEN(rb_commit); ++i)
			push_commit_1(walk, rb_ary_entry(rb_commit, i), hide);

		return;
	}

	push_commit_1(walk, rb_commit, hide);
}

/*
 *  call-seq:
 *    Walker.new(repository) -> walker
 *
 *  Create a new +Walker+ instance able to walk commits found
 *  in +repository+, which is a <tt>Rugged::Repository</tt> instance.
 */
static VALUE rb_git_walker_new(VALUE klass, VALUE rb_repo)
{
	git_repository *repo;
	git_revwalk *walk;
	int error;

	Data_Get_Struct(rb_repo, git_repository, repo);

	error = git_revwalk_new(&walk, repo);
	rugged_exception_check(error);

	return rugged_walker_new(klass, rb_repo, walk);;
}

/*
 *  call-seq:
 *    walker.push(commit) -> nil
 *
 *  Push one new +commit+ to start the walk from. +commit+ must be a
 *  +String+ with the OID of a commit in the repository, or a <tt>Rugged::Commit</tt>
 *  instance.
 *
 *  More than one commit may be pushed to the walker (to walk several
 *  branches simulataneously).
 *
 *  Duplicate pushed commits will be ignored; at least one commit must have been
 *  pushed as a starting point before the walk can begin.
 *
 *    walker.push("92b22bbcb37caf4f6f53d30292169e84f5e4283b")
 */
static VALUE rb_git_walker_push(VALUE self, VALUE rb_commit)
{
	git_revwalk *walk;
	Data_Get_Struct(self, git_revwalk, walk);
	push_commit(walk, rb_commit, 0);
	return Qnil;
}

static VALUE rb_git_walker_push_range(VALUE self, VALUE range)
{
	git_revwalk *walk;
	Data_Get_Struct(self, git_revwalk, walk);
	rugged_exception_check(git_revwalk_push_range(walk, StringValueCStr(range)));
	return Qnil;
}

/*
 *  call-seq:
 *    walker.hide(commit) -> nil
 *
 *  Hide the given +commit+ (and all its parents) from the
 *  output in the revision walk.
 */
static VALUE rb_git_walker_hide(VALUE self, VALUE rb_commit)
{
	git_revwalk *walk;
	Data_Get_Struct(self, git_revwalk, walk);
	push_commit(walk, rb_commit, 1);
	return Qnil;
}

/*
 *  call-seq:
 *    walker.sorting(sort_mode) -> nil
 *
 *  Change the sorting mode for the revision walk.
 *
 *  This will cause +walker+ to be reset.
 */
static VALUE rb_git_walker_sorting(VALUE self, VALUE ruby_sort_mode)
{
	git_revwalk *walk;
	Data_Get_Struct(self, git_revwalk, walk);
	git_revwalk_sorting(walk, FIX2INT(ruby_sort_mode));
	return Qnil;
}

/*
 *  call-seq:
 *    walker.simplify_first_parent() -> nil
 *
 *  Simplify the walk to the first parent of each commit.
 */
static VALUE rb_git_walker_simplify_first_parent(VALUE self)
{
	git_revwalk *walk;
	Data_Get_Struct(self, git_revwalk, walk);
	git_revwalk_simplify_first_parent(walk);
	return Qnil;
}

/*
 *  call-seq:
 *    walker.reset -> nil
 *
 *  Remove all pushed and hidden commits and reset the +walker+
 *  back into a blank state.
 */
static VALUE rb_git_walker_reset(VALUE self)
{
	git_revwalk *walk;
	Data_Get_Struct(self, git_revwalk, walk);
	git_revwalk_reset(walk);
	return Qnil;
}

struct walk_options {
	VALUE rb_owner;
	VALUE rb_options;

	git_repository *repo;
	git_revwalk *walk;
	int oid_only, stats_only, no_merges;
	uint64_t offset, limit;
};

static void load_walk_limits(struct walk_options *w, VALUE rb_options)
{
	VALUE rb_value = rb_hash_lookup(rb_options, CSTR2SYM("offset"));
	if (!NIL_P(rb_value)) {
		Check_Type(rb_value, T_FIXNUM);
		w->offset = FIX2ULONG(rb_value);
	}

	rb_value = rb_hash_lookup(rb_options, CSTR2SYM("limit"));
	if (!NIL_P(rb_value)) {
		Check_Type(rb_value, T_FIXNUM);
		w->limit = FIX2ULONG(rb_value);
	}

	rb_value = rb_hash_lookup(rb_options, CSTR2SYM("no_merges"));
	if (RTEST(rb_value)) {
		w->no_merges = 1;
	}

	rb_value = rb_hash_lookup(rb_options, CSTR2SYM("stats_only"));
	if (RTEST(rb_value)) {
		w->no_merges = 1;
		w->stats_only = 1;
	}
}

static VALUE load_all_options(VALUE _payload)
{
	struct walk_options *w = (struct walk_options *)_payload;
	VALUE rb_options = w->rb_options;
	VALUE rb_show, rb_hide, rb_sort, rb_simp, rb_oid_only;

	load_walk_limits(w, rb_options);

	rb_sort = rb_hash_lookup(rb_options, CSTR2SYM("sort"));
	if (!NIL_P(rb_sort)) {
		Check_Type(rb_sort, T_FIXNUM);
		git_revwalk_sorting(w->walk, FIX2INT(rb_sort));
	}

	rb_show = rb_hash_lookup(rb_options, CSTR2SYM("show"));
	if (!NIL_P(rb_show)) {
		push_commit(w->walk, rb_show, 0);
	}

	rb_hide = rb_hash_lookup(rb_options, CSTR2SYM("hide"));
	if (!NIL_P(rb_hide)) {
		push_commit(w->walk, rb_hide, 1);
	}

	rb_simp = rb_hash_lookup(rb_options, CSTR2SYM("simplify"));
	if (RTEST(rb_simp)) {
		git_revwalk_simplify_first_parent(w->walk);
	}

	rb_oid_only = rb_hash_lookup(rb_options, CSTR2SYM("oid_only"));
	if (RTEST(rb_oid_only)) {
		w->oid_only = 1;
	}

	return Qnil;
}

typedef struct commit_stats {
	size_t adds, dels;
	git_signature *committer, *author;
	git_oid oid;
} commit_stats;

static int rb_git_walker_stats_cb(
	const git_diff_delta *delta,
	const git_diff_hunk *hunk,
	const git_diff_line *line,
	void *payload)
{
	commit_stats *stats = payload;

	switch (line->origin) {
	case GIT_DIFF_LINE_ADDITION: stats->adds++; break;
	case GIT_DIFF_LINE_DELETION: stats->dels++; break;
	default: break;
	}

	return GIT_OK;
}

static void rb_git_walker_commit_stats__free(commit_stats *stats) {
	git_signature_free(stats->committer);
	git_signature_free(stats->author);
	xfree(stats);
}

struct apply_walk_options_args {
	git_commit *commit;
	struct walk_options *options;
};

static VALUE apply_walk_options(VALUE _payload) {
	int error;
	struct walk_options *w;
	commit_stats *stats;
	git_commit *commit, *right_commit;
	git_tree *tree, *right_tree;
	git_diff *diff;
	git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;

	w = ((struct apply_walk_options_args *)_payload)->options;
	commit = ((struct apply_walk_options_args *)_payload)->commit;

	if (w->no_merges && git_commit_parentcount(commit) > 1)
		return Qnil;

	if (w->stats_only) {
		error = git_commit_tree(&tree, commit);
		rugged_exception_check(error);

		stats = xmalloc(sizeof(commit_stats));
		stats->adds = stats->dels = 0;
		git_signature_dup(&stats->committer, git_commit_committer(commit));
		git_signature_dup(&stats->author, git_commit_author(commit));
		git_oid_cpy(&stats->oid, git_commit_id(commit));

		error = git_commit_parent(&right_commit, commit, 0);
		if (error == GIT_OK) {
			error = git_commit_tree(&right_tree, right_commit);
			git_commit_free(right_commit);
		}
		else if (error == GIT_ENOTFOUND) {
			right_tree = NULL;
			error = GIT_OK;
		}

		if (error != GIT_OK) {
			xfree(stats);
			git_tree_free(tree);
			rugged_exception_check(error);
		}

		error = git_diff_tree_to_tree(&diff, w->repo, right_tree, tree, &diff_opts);
		git_tree_free(tree);
		if (right_tree) git_tree_free(right_tree);
		if (error == GIT_OK) {
			error = git_diff_foreach(diff, NULL, NULL, NULL, rb_git_walker_stats_cb, stats);
			git_diff_free(diff);
		}
		if (error != GIT_OK) {
			xfree(stats);
			rugged_exception_check(error);
		}
		return Data_Wrap_Struct(rb_cRuggedCommitStats, NULL, rb_git_walker_commit_stats__free, stats);
	} else {
		return rugged_object_new(w->rb_owner, (git_object *)commit);
	}
}

static VALUE do_walk(VALUE _payload)
{
	struct walk_options *w = (struct walk_options *)_payload;
	int error, exception = 0;
	git_oid commit_oid;

	while ((error = git_revwalk_next(&commit_oid, w->walk)) == 0) {
		if (w->offset > 0) {
			w->offset--;
			continue;
		}

		if (w->oid_only) {
			rb_yield(rugged_create_oid(&commit_oid));
		} else {
			VALUE result;
			git_commit *commit;
			struct apply_walk_options_args args;

			error = git_commit_lookup(&commit, w->repo, &commit_oid);
			rugged_exception_check(error);

			args.commit = commit;
			args.options = w;

			result = rb_protect(apply_walk_options, (VALUE)&args, &exception);

			if (result == Qnil) {
				git_commit_free(commit);
				continue;
			}
			if (exception) {
				git_commit_free(commit);
				rb_jump_tag(exception);
			}

			rb_yield(result);
		}

		if (--w->limit == 0)
			break;
	}

	if (error != GIT_ITEROVER)
		rugged_exception_check(error);

	return Qnil;
}

/*
 *  call-seq:
 *    Rugged::Walker.walk(repo, options={}) { |commit| block }
 *
 *	Create a Walker object, initialize it with the given options
 *	and perform a walk on the repository; the lifetime of the
 *	walker is bound to the call and it is immediately cleaned
 *	up after the walk is over.
 *
 *	The following options are available:
 *
 *	- +sort+: Sorting mode for the walk (or an OR combination
 *	of several sorting modes).
 *
 *	- +show+: Tips of the repository that will be walked;
 *	this is necessary for the walk to yield any results.
 *	A tip can be a 40-char object ID, an existing +Rugged::Commit+
 *	object, a reference, or an +Array+ of several of these
 *	(if you'd like to walk several tips in parallel).
 *
 *	- +hide+: Same as +show+, but hides the given tips instead
 *	so they don't appear on the walk.
 *
 *	- +oid_only+: if +true+, the walker will yield 40-char OIDs
 *	for each commit, instead of real +Rugged::Commit+ objects.
 *	Defaults to +false+.
 *
 *	- +simplify+: if +true+, the walk will be simplified
 *	to the first parent of each commit.
 *
 *  - +no_merges+: if +true+, only commit with only-one parent will
 *  be yielded.
 *	Defaults to +false+.
 *
 *	- +stats_only+: if +true+, the walker will yield a stats object
 *  with additions and deletions for each no-merges commit, instead of
 *  a real +Rugged::Commit+ objects. This option implies +no_merges+.
 *	Defaults to +false+.
 *
 *	Example:
 *
 *    Rugged::Walker.walk(repo,
 *		show: "92b22bbcb37caf4f6f53d30292169e84f5e4283b",
 *		sort: Rugged::SORT_DATE|Rugged::SORT_TOPO,
 *		oid_only: true) do |commit_oid|
 *			puts commit_oid
 *		end
 *
 *  generates:
 *
 *    92b22bbcb37caf4f6f53d30292169e84f5e4283b
 *    6b750d5800439b502de669465b385e5f469c78b6
 *    ef9207141549f4ffcd3c4597e270d32e10d0a6bc
 *    cb75e05f0f8ac3407fb3bd0ebd5ff07573b16c9f
 *    ...
 */
static VALUE rb_git_walk(int argc, VALUE *argv, VALUE self)
{
	VALUE rb_repo, rb_options;
	struct walk_options w;
	int exception = 0;

	rb_scan_args(argc, argv, "10:", &rb_repo, &rb_options);

	if (!rb_block_given_p()) {
		ID iter_method = ID2SYM(rb_intern("walk"));
		return rb_funcall(self, rb_intern("to_enum"), 3,
			iter_method, rb_repo, rb_options);
	}

	Data_Get_Struct(rb_repo, git_repository, w.repo);
	rugged_exception_check(git_revwalk_new(&w.walk, w.repo));

	w.rb_owner = rb_repo;
	w.rb_options = rb_options;

	w.oid_only = 0;
	w.stats_only = 0;
	w.offset = 0;
	w.limit = UINT64_MAX;

	if (!NIL_P(w.rb_options))
		rb_protect(load_all_options, (VALUE)&w, &exception);

	if (!exception)
		rb_protect(do_walk, (VALUE)&w, &exception);

	git_revwalk_free(w.walk);

	if (exception)
		rb_jump_tag(exception);

	return Qnil;
}

static VALUE rb_git_walk_with_opts(int argc, VALUE *argv, VALUE self, int oid_only)
{
	VALUE rb_options;
	struct walk_options w;

	rb_scan_args(argc, argv, "01", &rb_options);

	if (!rb_block_given_p()) {
		ID iter_method = ID2SYM(rb_intern(oid_only ? "each_oid" : "each"));
		return rb_funcall(self, rb_intern("to_enum"), 2, iter_method, rb_options);
	}

	Data_Get_Struct(self, git_revwalk, w.walk);
	w.repo = git_revwalk_repository(w.walk);

	w.rb_owner = rugged_owner(self);
	w.rb_options = Qnil;

	w.oid_only = oid_only;
	w.stats_only = 0;
	w.offset = 0;
	w.limit = UINT64_MAX;

	if (!NIL_P(rb_options))
		load_walk_limits(&w, rb_options);

	return do_walk((VALUE)&w);
}

/*
 *  call-seq:
 *    walker.each { |commit| block }
 *    walker.each -> Iterator
 *
 *  Perform the walk through the repository, yielding each
 *  one of the commits found as a <tt>Rugged::Commit</tt> instance
 *  to +block+.
 *
 *  If no +block+ is given, an +Iterator+ will be returned.
 *
 *  The walker must have been previously set-up before a walk can be performed
 *  (i.e. at least one commit must have been pushed).
 *
 *    walker.push("92b22bbcb37caf4f6f53d30292169e84f5e4283b")
 *    walker.each { |commit| puts commit.oid }
 *
 *  generates:
 *
 *    92b22bbcb37caf4f6f53d30292169e84f5e4283b
 *    6b750d5800439b502de669465b385e5f469c78b6
 *    ef9207141549f4ffcd3c4597e270d32e10d0a6bc
 *    cb75e05f0f8ac3407fb3bd0ebd5ff07573b16c9f
 *    ...
 */
static VALUE rb_git_walker_each(int argc, VALUE *argv, VALUE self)
{
	return rb_git_walk_with_opts(argc, argv, self, 0);
}

/*
 *  call-seq:
 *    walker.each_oid { |commit| block }
 *    walker.each_oid -> Iterator
 *
 *  Perform the walk through the repository, yielding each
 *  one of the commit oids found as a <tt>String</tt>
 *  to +block+.
 *
 *  If no +block+ is given, an +Iterator+ will be returned.
 *
 *  The walker must have been previously set-up before a walk can be performed
 *  (i.e. at least one commit must have been pushed).
 *
 *    walker.push("92b22bbcb37caf4f6f53d30292169e84f5e4283b")
 *    walker.each { |commit_oid| puts commit_oid }
 *
 *  generates:
 *
 *    92b22bbcb37caf4f6f53d30292169e84f5e4283b
 *    6b750d5800439b502de669465b385e5f469c78b6
 *    ef9207141549f4ffcd3c4597e270d32e10d0a6bc
 *    cb75e05f0f8ac3407fb3bd0ebd5ff07573b16c9f
 *    ...
 */
static VALUE rb_git_walker_each_oid(int argc, VALUE *argv, VALUE self)
{
	return rb_git_walk_with_opts(argc, argv, self, 1);
}

static VALUE rb_git_commit_stats_adds_GET(VALUE self) {
	commit_stats *stats;
	Data_Get_Struct(self, commit_stats, stats);
	return INT2FIX((int) stats->adds);
}

static VALUE rb_git_commit_stats_dels_GET(VALUE self) {
	commit_stats *stats;
	Data_Get_Struct(self, commit_stats, stats);
	return INT2FIX((int) stats->dels);
}

static VALUE rb_git_commit_stats_committer_GET(VALUE self) {
	commit_stats *stats;
	Data_Get_Struct(self, commit_stats, stats);
	return rugged_signature_new(stats->committer, "BINARY");
}

static VALUE rb_git_commit_stats_author_GET(VALUE self) {
	commit_stats *stats;
	Data_Get_Struct(self, commit_stats, stats);
	return rugged_signature_new(stats->author, "BINARY");
}

static VALUE rb_git_commit_stats_oid_GET(VALUE self) {
	commit_stats *stats;
	Data_Get_Struct(self, commit_stats, stats);
	return rugged_create_oid(&stats->oid);
}

void Init_rugged_revwalk(void)
{
	rb_cRuggedWalker = rb_define_class_under(rb_mRugged, "Walker", rb_cObject);
	rb_define_singleton_method(rb_cRuggedWalker, "new", rb_git_walker_new, 1);
	rb_define_singleton_method(rb_cRuggedWalker, "walk", rb_git_walk, -1);

	rb_define_method(rb_cRuggedWalker, "push", rb_git_walker_push, 1);
	rb_define_method(rb_cRuggedWalker, "push_range", rb_git_walker_push_range, 1);
	rb_define_method(rb_cRuggedWalker, "each", rb_git_walker_each, -1);
	rb_define_method(rb_cRuggedWalker, "each_oid", rb_git_walker_each_oid, -1);
	rb_define_method(rb_cRuggedWalker, "walk", rb_git_walker_each, -1);
	rb_define_method(rb_cRuggedWalker, "hide", rb_git_walker_hide, 1);
	rb_define_method(rb_cRuggedWalker, "reset", rb_git_walker_reset, 0);
	rb_define_method(rb_cRuggedWalker, "sorting", rb_git_walker_sorting, 1);
	rb_define_method(rb_cRuggedWalker, "simplify_first_parent", rb_git_walker_simplify_first_parent, 0);

	rb_cRuggedCommitStats = rb_define_class_under(rb_cRuggedCommit, "Stats", rb_cObject);
	rb_define_method(rb_cRuggedCommitStats, "adds", rb_git_commit_stats_adds_GET, 0);
	rb_define_method(rb_cRuggedCommitStats, "dels", rb_git_commit_stats_dels_GET, 0);
	rb_define_method(rb_cRuggedCommitStats, "committer", rb_git_commit_stats_committer_GET, 0);
	rb_define_method(rb_cRuggedCommitStats, "author", rb_git_commit_stats_author_GET, 0);
	rb_define_method(rb_cRuggedCommitStats, "oid", rb_git_commit_stats_oid_GET, 0);
}
