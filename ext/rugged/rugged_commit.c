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
extern VALUE rb_cRuggedRepo;
extern VALUE rb_cRuggedSignature;
VALUE rb_cRuggedCommit;

/*
 *  call-seq:
 *    commit.message -> msg
 *
 *  Return the message of this commit. This includes the full body of the
 *  message, with the short description, detailed descritpion, and any
 *  optional footers or signatures after it.
 *
 *  In Ruby 1.9+, the returned string will be encoded with the encoding
 *  specified in the +Encoding+ header of the commit, if available.
 *
 *    commit.message #=> "add a lot of RDoc docs\n\nthis includes docs for commit and blob"
 */
static VALUE rb_git_commit_message_GET(VALUE self)
{
	git_commit *commit;
	rb_encoding *encoding = rb_utf8_encoding();
	const char *encoding_name;
	const char *message;

	Data_Get_Struct(self, git_commit, commit);

	message = git_commit_message(commit);
	encoding_name = git_commit_message_encoding(commit);
	if (encoding_name != NULL)
		encoding = rb_enc_find(encoding_name);

	return rb_enc_str_new(message, strlen(message), encoding);
}

/*
 *  call-seq:
 *    commit.committer -> signature
 *
 *  Return the signature for the committer of this +commit+. The signature
 *  is returned as a +Hash+ containing +:name+, +:email+ of the author
 *  and +:time+ of the change.
 *
 *  The committer of a commit is the person who actually applied the changes
 *  of the commit; in most cases it's the same as the author.
 *
 *  In Ruby 1.9+, the returned string will be encoded with the encoding
 *  specified in the +Encoding+ header of the commit, if available.
 *
 *    commit.committer #=> {:email=>"tanoku@gmail.com", :time=>Tue Jan 24 05:42:45 UTC 2012, :name=>"Vicent Mart\303\255"}
 */
static VALUE rb_git_commit_committer_GET(VALUE self)
{
	git_commit *commit;
	Data_Get_Struct(self, git_commit, commit);

	return rugged_signature_new(
		git_commit_committer(commit),
		git_commit_message_encoding(commit));
}

/*
 *  call-seq:
 *    commit.author -> signature
 *
 *  Return the signature for the author of this +commit+. The signature
 *  is returned as a +Hash+ containing +:name+, +:email+ of the author
 *  and +:time+ of the change.
 *
 *  The author of the commit is the person who intially created the changes.
 *
 *  In Ruby 1.9+, the returned string will be encoded with the encoding
 *  specified in the +Encoding+ header of the commit, if available.
 *
 *    commit.author #=> {:email=>"tanoku@gmail.com", :time=>Tue Jan 24 05:42:45 UTC 2012, :name=>"Vicent Mart\303\255"}
 */
static VALUE rb_git_commit_author_GET(VALUE self)
{
	git_commit *commit;
	Data_Get_Struct(self, git_commit, commit);

	return rugged_signature_new(
		git_commit_author(commit),
		git_commit_message_encoding(commit));
}

/*
 *  call-seq:
 *    commit.epoch_time -> int
 *
 *  Return the time when this commit was made effective. This is the same value
 *  as the +:time+ attribute for +commit.committer+, but represented as an +Integer+
 *  value in seconds since the Epoch.
 *
 *    commit.time #=> 1327383765
 */
static VALUE rb_git_commit_epoch_time_GET(VALUE self)
{
	git_commit *commit;
	Data_Get_Struct(self, git_commit, commit);

	return ULONG2NUM(git_commit_time(commit));
}

/*
 *  call-seq:
 *    commit.tree -> tree
 *
 *  Return the tree pointed at by this +commit+. The tree is
 *  returned as a +Rugged::Tree+ object.
 *
 *    commit.tree #=> #<Rugged::Tree:0x10882c680>
 */
static VALUE rb_git_commit_tree_GET(VALUE self)
{
	git_commit *commit;
	git_tree *tree;
	VALUE owner;
	int error;

	Data_Get_Struct(self, git_commit, commit);
	owner = rugged_owner(self);

	error = git_commit_tree(&tree, commit);
	rugged_exception_check(error);

	return rugged_object_new(owner, (git_object *)tree);
}

/*
 *  call-seq:
 *    commit.tree_id -> oid
 *
 *  Return the tree oid pointed at by this +commit+. The tree is
 *  returned as a String object.
 *
 *    commit.tree_id #=> "f148106ca58764adc93ad4e2d6b1d168422b9796"
 */
static VALUE rb_git_commit_tree_id_GET(VALUE self)
{
	git_commit *commit;
	const git_oid *tree_id;

	Data_Get_Struct(self, git_commit, commit);

	tree_id = git_commit_tree_id(commit);

	return rugged_create_oid(tree_id);
}

/*
 *  call-seq:
 *    commit.parents -> [commit, ...]
 *
 *  Return the parent(s) of this commit as an array of +Rugged::Commit+
 *  objects. An array is always returned even when the commit has only
 *  one or zero parents.
 *
 *    commit.parents #=> => [#<Rugged::Commit:0x108828918>]
 *    root.parents #=> []
 */
static VALUE rb_git_commit_parents_GET(VALUE self)
{
	git_commit *commit;
	git_commit *parent;
	unsigned int n, parent_count;
	VALUE ret_arr, owner;
	int error;

	Data_Get_Struct(self, git_commit, commit);
	owner = rugged_owner(self);

	parent_count = git_commit_parentcount(commit);
	ret_arr = rb_ary_new2((long)parent_count);

	for (n = 0; n < parent_count; n++) {
		error = git_commit_parent(&parent, commit, n);
		rugged_exception_check(error);
		rb_ary_push(ret_arr, rugged_object_new(owner, (git_object *)parent));
	}

	return ret_arr;
}

/*
 *  call-seq:
 *    commit.parent -> commit
 *
 *  Return the first parent of this commit as +Rugged::Commit+
 *  object. Nil will be returned if the commit has no parent.
 *
 *  Don't try to apply this function on merge commit, which has multiple parents.
 *
 *    commit.parent #=> => #<Rugged::Commit:0x108828918>
 *    root.parents #=> nil
 */
static VALUE rb_git_commit_parent_GET(VALUE self)
{
	git_commit *commit;
	git_commit *parent;
	VALUE owner;
	int error;

	Data_Get_Struct(self, git_commit, commit);
	owner = rugged_owner(self);

	if (git_commit_parentcount(commit) > 0) {
		error = git_commit_parent(&parent, commit, 0);
		rugged_exception_check(error);
		return rugged_object_new(owner, (git_object *)parent);
	} else {
		return Qnil;
	}
}

/*
 *  call-seq:
 *    commit.parent_ids -> [oid, ...]
 *
 *  Return the parent oid(s) of this commit as an array of oid String
 *  objects. An array is always returned even when the commit has only
 *  one or zero parents.
 *
 *    commit.parent_ids #=> => ["2cb831a8aea28b2c1b9c63385585b864e4d3bad1", ...]
 *    root.parent_ids #=> []
 */
static VALUE rb_git_commit_parent_ids_GET(VALUE self)
{
	git_commit *commit;
	const git_oid *parent_id;
	unsigned int n, parent_count;
	VALUE ret_arr;

	Data_Get_Struct(self, git_commit, commit);

	parent_count = git_commit_parentcount(commit);
	ret_arr = rb_ary_new2((long)parent_count);

	for (n = 0; n < parent_count; n++) {
		parent_id = git_commit_parent_id(commit, n);
		if (parent_id) {
			rb_ary_push(ret_arr, rugged_create_oid(parent_id));
		}
	}

	return ret_arr;
}

/*
 *  call-seq:
 *    commit.amend(data = {}) -> oid
 *
 *  Amend a commit object, with the given +data+
 *  arguments, passed as a +Hash+:
 *
 *  - +:message+: a string with the full text for the commit's message
 *  - +:committer+ (optional): a hash with the signature for the committer,
 *    defaults to the signature from the configuration
 *  - +:author+ (optional): a hash with the signature for the author,
 *    defaults to the signature from the configuration
 *  - +:tree+: the tree for this amended commit, represented as a <tt>Rugged::Tree</tt>
 *    instance or an OID +String+.
 *  - +:update_ref+ (optional): a +String+ with the name of a reference in the
 *  repository which should be updated to point to this amended commit (e.g. "HEAD")
 *
 *  When the amended commit is successfully written to disk, its +oid+ will be
 *  returned as a hex +String+.
 *
 *    author = {:email=>"tanoku@gmail.com", :time=>Time.now, :name=>"Vicent Mart\303\255"}
 *
 *    commit.amend(
 *      :author => author,
 *      :message => "Updated Hello world\n\n",
 *      :committer => author,
 *      :tree => some_tree) #=> "f148106ca58764adc93ad4e2d6b1d168422b9796"
 */
static VALUE rb_git_commit_amend(VALUE self, VALUE rb_data)
{
	VALUE rb_message, rb_tree, rb_ref, owner;
	int error = 0;
	git_commit *commit_to_amend;
	char *message = NULL;
	git_tree *tree = NULL;
	git_signature *author = NULL, *committer = NULL;
	git_oid commit_oid;
	git_repository *repo;
	const char *update_ref = NULL;

	Check_Type(rb_data, T_HASH);

	Data_Get_Struct(self, git_commit, commit_to_amend);

	owner = rugged_owner(self);
	Data_Get_Struct(owner, git_repository, repo);

	rb_ref = rb_hash_aref(rb_data, CSTR2SYM("update_ref"));
	if (!NIL_P(rb_ref)) {
		Check_Type(rb_ref, T_STRING);
		update_ref = StringValueCStr(rb_ref);
	}

	rb_message = rb_hash_aref(rb_data, CSTR2SYM("message"));
	if (!NIL_P(rb_message)) {
		Check_Type(rb_message, T_STRING);
		message = StringValueCStr(rb_message);
	}

	rb_tree = rb_hash_aref(rb_data, CSTR2SYM("tree"));
	if (!NIL_P(rb_tree))
		tree = (git_tree *)rugged_object_get(repo, rb_tree, GIT_OBJ_TREE);

	if (!NIL_P(rb_hash_aref(rb_data, CSTR2SYM("committer")))) {
		committer = rugged_signature_get(
			rb_hash_aref(rb_data, CSTR2SYM("committer")), repo
		);
	}

	if (!NIL_P(rb_hash_aref(rb_data, CSTR2SYM("author")))) {
		author = rugged_signature_get(
			rb_hash_aref(rb_data, CSTR2SYM("author")), repo
		);
	}

	error = git_commit_amend(
		&commit_oid,
		commit_to_amend,
		update_ref,
		author,
		committer,
		NULL,
		message,
		tree);

	git_signature_free(author);
	git_signature_free(committer);

	git_object_free((git_object *)tree);

	rugged_exception_check(error);

	return rugged_create_oid(&commit_oid);
}

/*
 *  call-seq:
 *    Commit.create(repository, data = {}) -> oid
 *
 *  Write a new +Commit+ object to +repository+, with the given +data+
 *  arguments, passed as a +Hash+:
 *
 *  - +:message+: a string with the full text for the commit's message
 *  - +:committer+ (optional): a hash with the signature for the committer,
 *    defaults to the signature from the configuration
 *  - +:author+ (optional): a hash with the signature for the author,
 *    defaults to the signature from the configuration
 *  - +:parents+: an +Array+ with zero or more parents for this commit,
 *    represented as <tt>Rugged::Commit</tt> instances, or OID +String+.
 *  - +:tree+: the tree for this commit, represented as a <tt>Rugged::Tree</tt>
 *    instance or an OID +String+.
 *  - +:update_ref+ (optional): a +String+ with the name of a reference in the
 *    repository which should be updated to point to this commit (e.g. "HEAD")
 *
 *  When the commit is successfully written to disk, its +oid+ will be
 *  returned as a hex +String+.
 *
 *    author = {:email=>"tanoku@gmail.com", :time=>Time.now, :name=>"Vicent Mart\303\255"}
 *
 *    Rugged::Commit.create(r,
 *      :author => author,
 *      :message => "Hello world\n\n",
 *      :committer => author,
 *      :parents => ["2cb831a8aea28b2c1b9c63385585b864e4d3bad1"],
 *      :tree => some_tree) #=> "f148106ca58764adc93ad4e2d6b1d168422b9796"
 */
static VALUE rb_git_commit_create(VALUE self, VALUE rb_repo, VALUE rb_data)
{
	VALUE rb_message, rb_tree, rb_parents, rb_ref;
	VALUE rb_err_obj = Qnil;
	int parent_count, i, error = 0;
	const git_commit **parents = NULL;
	git_commit **free_list = NULL;
	git_tree *tree;
	git_signature *author, *committer;
	git_oid commit_oid;
	git_repository *repo;
	const char *update_ref = NULL;

	Check_Type(rb_data, T_HASH);

	rugged_check_repo(rb_repo);
	Data_Get_Struct(rb_repo, git_repository, repo);

	rb_ref = rb_hash_aref(rb_data, CSTR2SYM("update_ref"));
	if (!NIL_P(rb_ref)) {
		Check_Type(rb_ref, T_STRING);
		update_ref = StringValueCStr(rb_ref);
	}

	rb_message = rb_hash_aref(rb_data, CSTR2SYM("message"));
	Check_Type(rb_message, T_STRING);

	committer = rugged_signature_get(
		rb_hash_aref(rb_data, CSTR2SYM("committer")), repo
	);

	author = rugged_signature_get(
		rb_hash_aref(rb_data, CSTR2SYM("author")), repo
	);

	rb_parents = rb_hash_aref(rb_data, CSTR2SYM("parents"));
	Check_Type(rb_parents, T_ARRAY);

	rb_tree = rb_hash_aref(rb_data, CSTR2SYM("tree"));
	tree = (git_tree *)rugged_object_get(repo, rb_tree, GIT_OBJ_TREE);

	parents = alloca(RARRAY_LEN(rb_parents) * sizeof(void *));
	free_list = alloca(RARRAY_LEN(rb_parents) * sizeof(void *));
	parent_count = 0;

	for (i = 0; i < (int)RARRAY_LEN(rb_parents); ++i) {
		VALUE p = rb_ary_entry(rb_parents, i);
		git_commit *parent = NULL;
		git_commit *free_ptr = NULL;

		if (NIL_P(p))
			continue;

		if (TYPE(p) == T_STRING) {
			git_oid oid;

			error = git_oid_fromstr(&oid, StringValueCStr(p));
			if (error < GIT_OK)
				goto cleanup;

			error = git_commit_lookup(&parent, repo, &oid);
			if (error < GIT_OK)
				goto cleanup;

			free_ptr = parent;

		} else if (rb_obj_is_kind_of(p, rb_cRuggedCommit)) {
			Data_Get_Struct(p, git_commit, parent);
		} else {
			rb_err_obj = rb_exc_new2(rb_eTypeError, "Invalid type for parent object");
			goto cleanup;
		}

		parents[parent_count] = parent;
		free_list[parent_count] = free_ptr;
		parent_count++;
	}

	error = git_commit_create(
		&commit_oid,
		repo,
		update_ref,
		author,
		committer,
		NULL,
		StringValueCStr(rb_message),
		tree,
		parent_count,
		parents);

cleanup:
	git_signature_free(author);
	git_signature_free(committer);

	git_object_free((git_object *)tree);

	for (i = 0; i < parent_count; ++i)
		git_object_free((git_object *)free_list[i]);

	if (!NIL_P(rb_err_obj))
		rb_exc_raise(rb_err_obj);

	rugged_exception_check(error);

	return rugged_create_oid(&commit_oid);
}

/*
 *  call-seq:
 *    commit.to_mbox(options = {}) -> str
 *
 *  Returns +commit+'s contents formatted to resemble UNIX mailbox format.
 *
 *  Does not (yet) support merge commits.
 *
 *  The following options can be passed in the +options+ Hash:
 *
 *  :patch_no ::
 *    Number for this patch in the series. Defaults to +1+.
 *
 *  :total_patches ::
 *    Total number of patches in the series. Defaults to +1+.
 *
 *  :exclude_subject_patch_marker ::
 *    If set to true, no "[PATCH]" marker will be
 *    added to the beginning of the subject line.
 *
 *  Additionally, you can also pass the same options as for Rugged::Tree#diff.
 */
static VALUE rb_git_commit_to_mbox(int argc, VALUE *argv, VALUE self)
{
	git_buf email_patch = { NULL };
	git_repository *repo;
	git_commit *commit;
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	git_diff_format_email_flags_t flags = GIT_DIFF_FORMAT_EMAIL_NONE;

	VALUE rb_repo = rugged_owner(self), rb_email_patch = Qnil, rb_val, rb_options;

	int error;
	size_t patch_no = 1, total_patches = 1;

	rb_scan_args(argc, argv, ":", &rb_options);

	rugged_check_repo(rb_repo);
	Data_Get_Struct(rb_repo, git_repository, repo);

	Data_Get_Struct(self, git_commit, commit);

	if (!NIL_P(rb_options)) {
		Check_Type(rb_options, T_HASH);

		rb_val = rb_hash_aref(rb_options, CSTR2SYM("patch_no"));
		if (!NIL_P(rb_val))
			patch_no = NUM2INT(rb_val);

		rb_val = rb_hash_aref(rb_options, CSTR2SYM("total_patches"));
		if (!NIL_P(rb_val))
			total_patches = NUM2INT(rb_val);

		if (RTEST(rb_hash_aref(rb_options, CSTR2SYM("exclude_subject_patch_marker"))))
			flags |= GIT_DIFF_FORMAT_EMAIL_EXCLUDE_SUBJECT_PATCH_MARKER;

		rugged_parse_diff_options(&opts, rb_options);
	}

	error = git_diff_commit_as_email(
		&email_patch,
		repo,
		commit,
		patch_no,
		total_patches,
		flags,
		&opts);

	if (error) goto cleanup;

	rb_email_patch = rb_enc_str_new(email_patch.ptr, email_patch.size, rb_utf8_encoding());

	cleanup:

	xfree(opts.pathspec.strings);
	git_buf_free(&email_patch);
	rugged_exception_check(error);

	return rb_email_patch;
}

typedef struct commit_stat_task {
	git_tree *tree, *parent_tree;
	struct commit_stats *stats;
} commit_stat_task;

typedef struct commit_stat_tasks {
	git_repository *repo;
	pthread_mutex_t mutex;
	int top; // Make sure the top is thread-safe
	size_t tasks_nr;
	struct commit_stat_task *tasks;
} commit_stat_tasks;

static void* commit_stat_task_proc(void *_payload) {
	int error = 0;
	commit_stat_tasks *payload = (commit_stat_tasks *) _payload;
	commit_stat_task *current_task;
	struct commit_stats *current_stats;
	git_repository *repo = payload->repo;

	current_stats = xmalloc(sizeof(struct commit_stats));

	for (;;) {
	    if ((error = pthread_mutex_lock(&payload->mutex))) {
	    	giterr_set(GITERR_OS, "Failed to lock mutex");
	    	error = -1;
	    	break;
	    }
	    if (payload->top >= payload->tasks_nr) {
		    current_task = NULL;
	    } else {
		    current_task = &payload->tasks[payload->top];
		    payload->top++;
	    }
	    if ((error = pthread_mutex_unlock(&payload->mutex))) {
	    	giterr_set(GITERR_OS, "Failed to unlock mutex");
	    	error = -1;
	    	break;
	    }
	    if (current_task == NULL)
	    	break;
	    error = git_commit_stats_of(repo, current_task->tree, current_task->parent_tree, NULL,
	    	                        &current_task->stats->adds, &current_task->stats->dels);
	    if (error != GIT_OK)
	    	break;
	}

	xfree(current_stats);
	return (void *)(long) error;
}

static VALUE rb_git_commit_stats(VALUE klass, VALUE rb_repo, VALUE rb_commits) {
	long error = 0;
	int os_errno;
	size_t i, j, arrlen, nr_threads;
	VALUE rb_commit, rb_results = Qnil;
	git_repository *repo;
	git_commit *commit, *parent_commit;
	pthread_t *threads;
	commit_stat_tasks tasks;

	Check_Type(rb_commits, T_ARRAY);
	arrlen = RARRAY_LEN(rb_commits);

	rugged_check_repo(rb_repo);
	Data_Get_Struct(rb_repo, git_repository, repo);

	nr_threads = git_online_cpus();
	nr_threads = arrlen < nr_threads ? arrlen : nr_threads;

	threads = xmalloc(sizeof(pthread_t) * nr_threads);

	tasks.tasks_nr = arrlen;
	tasks.repo = repo;
	tasks.top = 0;
	tasks.tasks = xmalloc(sizeof(commit_stat_task) * arrlen);
	if ((os_errno = pthread_mutex_init(&tasks.mutex, NULL))) {
		goto WRONG;
	}
	memset(tasks.tasks, 0, sizeof(commit_stat_task) * arrlen);

	for (i = 0; i < arrlen; ++i) {
		rb_commit = rb_ary_entry(rb_commits, i);
		Data_Get_Struct(rb_commit, git_commit, commit);

		error = git_commit_parent(&parent_commit, commit, 0);
		if (error == GIT_ENOTFOUND) {
			parent_commit = NULL;
			error = GIT_OK;
		} else if (error != GIT_OK) {
			goto WRONG;
		}

		if (parent_commit != NULL) {
			error = git_commit_tree(&tasks.tasks[i].parent_tree, parent_commit);
			git_commit_free(parent_commit);
		}
		if (error == GIT_OK)
			error = git_commit_tree(&tasks.tasks[i].tree, commit);
		if (error != GIT_OK)
			goto WRONG;

		tasks.tasks[i].stats = xmalloc(sizeof(struct commit_stats));
		memset(tasks.tasks[i].stats, 0, sizeof(struct commit_stats));
		error = git_signature_dup(&tasks.tasks[i].stats->committer, git_commit_committer(commit));
		if (error == GIT_OK) error = git_signature_dup(&tasks.tasks[i].stats->author, git_commit_author(commit));
		if (error != GIT_OK)
			goto WRONG;

		git_oid_cpy(&tasks.tasks[i].stats->oid, git_commit_id(commit));
	}

	for (i = 0; i < nr_threads; ++i) {
	    if ((os_errno = pthread_create(&threads[i], NULL, commit_stat_task_proc, &tasks))) {
	    	for (j = 0; j < i; ++j)
	    		pthread_cancel(threads[j]);
			goto WRONG;
	    }
	}
	for (i = 0; i < nr_threads; ++i) {
	    if ((os_errno = pthread_join(threads[i], (void *) &error))) {
	    	for (j = i + 1; j < nr_threads; ++j)
	    		pthread_cancel(threads[j]);
			goto WRONG;
	    }
	    if (error != GIT_OK) {
	    	for (j = i + 1; j < nr_threads; ++j)
	    		pthread_cancel(threads[j]);
			goto WRONG;
	    }
	}

	rb_results = rb_ary_new2(arrlen);
	for (i = 0; i < arrlen; ++i)
		rb_ary_push(rb_results, rugged_commit_stats_new(tasks.tasks[i].stats));

	goto CLEAN;

WRONG:
	for (i = 0; i < arrlen; ++i) {
		if (tasks.tasks[i].stats != NULL) {
			if (tasks.tasks[i].stats->committer != NULL)
				git_signature_free(tasks.tasks[i].stats->committer);
			if (tasks.tasks[i].stats->author != NULL)
				git_signature_free(tasks.tasks[i].stats->author);
			xfree(tasks.tasks[i].stats);
		}
	}
CLEAN:
	for (i = 0; i < arrlen; ++i) {
		if (tasks.tasks[i].tree != NULL)
			git_tree_free(tasks.tasks[i].tree);
		if (tasks.tasks[i].parent_tree != NULL)
			git_tree_free(tasks.tasks[i].parent_tree);
	}
	pthread_mutex_destroy(&tasks.mutex);
	xfree(tasks.tasks);
	xfree(threads);
	if (os_errno) {
		VALUE rb_errno = INT2FIX(os_errno);
		rb_exc_raise(rb_class_new_instance(1, &rb_errno, rb_eSystemCallError));
	}
	rugged_exception_check(error);
	return rb_results;
}

/*
 *  call-seq:
 *    Commit.diff_between_repos(repo1, commit1, repo2, commit2) -> commit_ids
 *
 *  Return a list of commit ids which are descendant of commit2 in repo2,
 *  but are not descendant of commit1 in repo1.
 *
 */
static VALUE rb_git_commit_diff_between_repos(VALUE klass, VALUE rb_repo1, VALUE rb_commit1, VALUE rb_repo2, VALUE rb_commit2) {
	int error;
	VALUE rb_oids;
	git_commit *commit1;
	git_oid oids[2], walk_oid;
	git_oid *oid1 = &oids[0], *oid2 = &oids[1];
	git_repository *repo1, *repo2;
	git_odb *odb2;
	git_revwalk *walk;
	git_oidarray bases = {NULL, 0};

	rugged_check_repo(rb_repo1);
	Data_Get_Struct(rb_repo1, git_repository, repo1);

	rugged_check_repo(rb_repo2);
	Data_Get_Struct(rb_repo2, git_repository, repo2);

	Check_Type(rb_commit1, T_STRING);
	Check_Type(rb_commit2, T_STRING);

	if (RSTRING_LEN(rb_commit1) < GIT_OID_HEXSZ || RSTRING_LEN(rb_commit2) < GIT_OID_HEXSZ)
		rb_raise(rb_eArgError, "The given OID is too short");
	if (RSTRING_LEN(rb_commit1) > GIT_OID_HEXSZ || RSTRING_LEN(rb_commit2) > GIT_OID_HEXSZ)
		rb_raise(rb_eArgError, "The given OID is too long");

	if ((error = git_oid_fromstr(oid1, RSTRING_PTR(rb_commit1))) != GIT_OK ||
		(error = git_oid_fromstr(oid2, RSTRING_PTR(rb_commit2))) != GIT_OK)
		rugged_exception_check(error);

	error = git_repository_odb(&odb2, repo2);
	rugged_exception_check(error);

	while (oid1 != NULL && !git_odb_exists(odb2, oid1)) {
		git_oid *parent_oid;
		error = git_commit_lookup(&commit1, repo1, oid1);
		if (error != GIT_OK) {
			git_odb_free(odb2);
			rugged_exception_check(error);
		}
		parent_oid = git_commit_parent_id(commit1, 0);
		if (parent_oid != NULL)
			git_oid_cpy(oid1, parent_oid);
		else
			oid1 = NULL;
		git_commit_free(commit1);
	}
	git_odb_free(odb2);

	if (oid1 != NULL) {
		error = git_merge_bases_many(&bases, repo2, 2, oids);
		if (error == GIT_ENOTFOUND)
			oid1 = NULL;
		else if (error != GIT_OK)
			rugged_exception_check(error);
		else {
			git_oid_cpy(oid1, &bases.ids[0]);
			git_oidarray_free(&bases);
		}
	}

	error = git_revwalk_new(&walk, repo2);
	rugged_exception_check(error);
	git_revwalk_sorting(walk, GIT_SORT_TIME);
	error = git_revwalk_push(walk, oid2);
	if (error == GIT_OK && oid1 != NULL)
		error = git_revwalk_hide(walk, oid1);
	if (error != GIT_OK) {
		git_revwalk_free(walk);
		rugged_exception_check(error);
	}
	rb_oids = rb_ary_new();
	while ((error = git_revwalk_next(&walk_oid, walk)) == GIT_OK)
		rb_ary_push(rb_oids, rugged_create_oid(&walk_oid));

	git_revwalk_free(walk);
	if (error == GIT_ITEROVER)
		return rb_oids;
	else
		rugged_exception_check(error);
	return Qnil;
}

void Init_rugged_commit(void)
{
	rb_cRuggedCommit = rb_define_class_under(rb_mRugged, "Commit", rb_cRuggedObject);

	rb_define_singleton_method(rb_cRuggedCommit, "create", rb_git_commit_create, 2);
	rb_define_singleton_method(rb_cRuggedCommit, "diff_between_repos", rb_git_commit_diff_between_repos, 4);
	rb_define_singleton_method(rb_cRuggedCommit, "stats", rb_git_commit_stats, 2);

	rb_define_method(rb_cRuggedCommit, "message", rb_git_commit_message_GET, 0);
	rb_define_method(rb_cRuggedCommit, "epoch_time", rb_git_commit_epoch_time_GET, 0);
	rb_define_method(rb_cRuggedCommit, "committer", rb_git_commit_committer_GET, 0);
	rb_define_method(rb_cRuggedCommit, "author", rb_git_commit_author_GET, 0);
	rb_define_method(rb_cRuggedCommit, "tree", rb_git_commit_tree_GET, 0);

	rb_define_method(rb_cRuggedCommit, "tree_id", rb_git_commit_tree_id_GET, 0);
	rb_define_method(rb_cRuggedCommit, "tree_oid", rb_git_commit_tree_id_GET, 0);

	rb_define_method(rb_cRuggedCommit, "parents", rb_git_commit_parents_GET, 0);
	rb_define_method(rb_cRuggedCommit, "parent", rb_git_commit_parent_GET, 0);
	rb_define_method(rb_cRuggedCommit, "parent_ids", rb_git_commit_parent_ids_GET, 0);
	rb_define_method(rb_cRuggedCommit, "parent_oids", rb_git_commit_parent_ids_GET, 0);

	rb_define_method(rb_cRuggedCommit, "amend", rb_git_commit_amend, 1);

	rb_define_method(rb_cRuggedCommit, "to_mbox", rb_git_commit_to_mbox, -1);
}
