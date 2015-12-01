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
extern VALUE rb_cRuggedTagAnnotation;
extern VALUE rb_cRuggedTree;
extern VALUE rb_cRuggedCommit;
extern VALUE rb_cRuggedBlob;
extern VALUE rb_cRuggedRepo;

VALUE rb_cRuggedObject;

git_otype rugged_otype_get(VALUE self)
{
	git_otype type = GIT_OBJ_BAD;

	if (NIL_P(self))
		return GIT_OBJ_ANY;

	switch (TYPE(self)) {
	case T_STRING:
		type = git_object_string2type(StringValueCStr(self));
		break;

	case T_FIXNUM:
		type = FIX2INT(self);
		break;

	case T_SYMBOL: {
		ID t = SYM2ID(self);

		if (t == rb_intern("commit"))
			type = GIT_OBJ_COMMIT;
		else if (t == rb_intern("tree"))
			type = GIT_OBJ_TREE;
		else if (t == rb_intern("tag"))
			type = GIT_OBJ_TAG;
		else if (t == rb_intern("blob"))
			type = GIT_OBJ_BLOB;
	   }
	}

	if (!git_object_typeisloose(type))
		rb_raise(rb_eTypeError, "Invalid Git object type specifier");

	return type;
}

VALUE rugged_otype_new(git_otype t)
{
	switch (t) {
		case GIT_OBJ_COMMIT:
			return CSTR2SYM("commit");
		case GIT_OBJ_TAG:
			return CSTR2SYM("tag");
		case GIT_OBJ_TREE:
			return CSTR2SYM("tree");
		case GIT_OBJ_BLOB:
			return CSTR2SYM("blob");
		default:
			return Qnil;
	}
}

int rugged_oid_get(git_oid *oid, git_repository *repo, VALUE p)
{
	git_object *object;
	int error;

	if (rb_obj_is_kind_of(p, rb_cRuggedObject)) {
		Data_Get_Struct(p, git_object, object);
		git_oid_cpy(oid, git_object_id(object));
	} else {
		Check_Type(p, T_STRING);

		/* Fast path: see if the 40-char string is an OID */
		if (RSTRING_LEN(p) == 40 &&
			git_oid_fromstr(oid, RSTRING_PTR(p)) == 0)
			return GIT_OK;

		if ((error = git_revparse_single(&object, repo, StringValueCStr(p))))
			return error;

		git_oid_cpy(oid, git_object_id(object));
		git_object_free(object);
	}

	return GIT_OK;
}

git_object *rugged_object_get(git_repository *repo, VALUE object_value, git_otype type)
{
	git_object *object = NULL;

	if (rb_obj_is_kind_of(object_value, rb_cRuggedObject)) {
		git_object *owned_obj = NULL;
		Data_Get_Struct(object_value, git_object, owned_obj);
		git_object_dup(&object, owned_obj);
	} else {
		int error;

		Check_Type(object_value, T_STRING);

		/* Fast path: if we have a 40-char string, just perform the lookup directly */
		if (RSTRING_LEN(object_value) == 40) {
			git_oid oid;

			/* If it's not an OID, we can still try the revparse */
			if (git_oid_fromstr(&oid, RSTRING_PTR(object_value)) == 0) {
				error = git_object_lookup(&object, repo, &oid, type);
				rugged_exception_check(error);
				return object;
			}
		}

		/* Otherwise, assume the string is a revlist and try to parse it */
		error = git_revparse_single(&object, repo, StringValueCStr(object_value));
		rugged_exception_check(error);
	}

	assert(object);

	if (type != GIT_OBJ_ANY && git_object_type(object) != type)
		rb_raise(rb_eArgError, "Object is not of the required type");

	return object;
}

static void rb_git_object__free(git_object *object)
{
	git_object_free(object);
}

VALUE rugged_object_new(VALUE owner, git_object *object)
{
	VALUE klass, rb_object;

	switch (git_object_type(object))
	{
		case GIT_OBJ_COMMIT:
			klass = rb_cRuggedCommit;
			break;

		case GIT_OBJ_TAG:
			klass = rb_cRuggedTagAnnotation;
			break;

		case GIT_OBJ_TREE:
			klass = rb_cRuggedTree;
			break;

		case GIT_OBJ_BLOB:
			klass = rb_cRuggedBlob;
			break;

		default:
			rb_raise(rb_eTypeError, "Invalid type for Rugged::Object");
			return Qnil; /* never reached */
	}

	rb_object = Data_Wrap_Struct(klass, NULL, &rb_git_object__free, object);
	rugged_set_owner(rb_object, owner);
	return rb_object;
}


static git_otype class2otype(VALUE klass)
{
	if (RTEST(rb_class_inherited_p(klass, rb_cRuggedCommit)))
        return GIT_OBJ_COMMIT;

	if (RTEST(rb_class_inherited_p(klass, rb_cRuggedTagAnnotation)))
		return GIT_OBJ_TAG;

	if (RTEST(rb_class_inherited_p(klass, rb_cRuggedBlob)))
		return GIT_OBJ_BLOB;

	if (RTEST(rb_class_inherited_p(klass, rb_cRuggedTree)))
		return GIT_OBJ_TREE;

	return GIT_OBJ_BAD;
}

/*
 *  call-seq:
 *    Object.new(repo, oid) -> object
 *    Object.lookup(repo, oid) -> object
 *
 *  Find and return the git object inside +repo+ with the given +oid+.
 *
 *  +oid+ can either have be the complete, 40 character string or any
 *  unique prefix.
 */
VALUE rb_git_object_lookup(VALUE klass, VALUE rb_repo, VALUE rb_hex)
{
	git_object *object;
	git_otype type;
	git_oid oid;
	int error;
	int oid_length;

	git_repository *repo;

	type = class2otype(klass);

	if (type == GIT_OBJ_BAD)
		type = GIT_OBJ_ANY;

	Check_Type(rb_hex, T_STRING);
	oid_length = (int)RSTRING_LEN(rb_hex);

	rugged_check_repo(rb_repo);

	if (oid_length > GIT_OID_HEXSZ)
		rb_raise(rb_eTypeError, "The given OID is too long");

	Data_Get_Struct(rb_repo, git_repository, repo);

	error = git_oid_fromstrn(&oid, RSTRING_PTR(rb_hex), oid_length);
	rugged_exception_check(error);

	if (oid_length < GIT_OID_HEXSZ)
		error = git_object_lookup_prefix(&object, repo, &oid, oid_length, type);
	else
		error = git_object_lookup(&object, repo, &oid, type);

	rugged_exception_check(error);

	return rugged_object_new(rb_repo, object);
}

/*
 *  call-seq:
 *    Object.exists?(repo, oid) -> true or false
 *
 *  Validate the git object with the given oid is existed or not.
 *
 *  +oid+ can either have be the complete, 40 character string or any
 *  unique prefix.
 */

VALUE rb_git_object_exists(VALUE klass, VALUE rb_repo, VALUE rb_hex)
{
	git_object *object;
	git_otype type;
	git_oid oid;
	int error;
	int oid_length;

	git_repository *repo;

	type = class2otype(klass);

	if (type == GIT_OBJ_BAD)
		type = GIT_OBJ_ANY;

	Check_Type(rb_hex, T_STRING);
	oid_length = (int)RSTRING_LEN(rb_hex);

	rugged_check_repo(rb_repo);

	if (oid_length > GIT_OID_HEXSZ)
		return Qfalse;

	Data_Get_Struct(rb_repo, git_repository, repo);

	error = git_oid_fromstrn(&oid, RSTRING_PTR(rb_hex), oid_length);
	if (error != GIT_OK) return Qfalse;

	error = git_object_lookup_prefix(&object, repo, &oid, oid_length, type);
	if (error != GIT_OK) return Qfalse;

	git_object_free(object);
	return Qtrue;
}

VALUE rugged_object_rev_parse(VALUE rb_repo, VALUE rb_spec, int as_obj)
{
	git_object *object;
	const char *spec;
	int error;
	git_repository *repo;
	VALUE ret;

	Check_Type(rb_spec, T_STRING);
	spec = RSTRING_PTR(rb_spec);

	rugged_check_repo(rb_repo);

	Data_Get_Struct(rb_repo, git_repository, repo);

	error = git_revparse_single(&object, repo, spec);
	rugged_exception_check(error);

	if (as_obj) {
		return rugged_object_new(rb_repo, object);
	}

	ret = rugged_create_oid(git_object_id(object));
	git_object_free(object);
	return ret;
}

static int rugged_commitish(git_repository* repo, const char *spec, git_commit **commit) {
	int error;
	git_object *object;
	git_tag *tag;

	if ((error = git_revparse_single(&object, repo, spec)) != GIT_OK)
		return error;
	while (git_object_type(object) == GIT_OBJ_TAG) {
		tag = (git_tag *) object;
		error = git_object_lookup(&object, repo, git_tag_target_id(tag), GIT_OBJ_ANY);
		git_tag_free(tag);
		if (error != GIT_OK)
			return error;
	}

	if (git_object_type(object) != GIT_OBJ_COMMIT) {
		git_object_free(object);
		giterr_set(GITERR_INVALID,
			"The requested type does not match the type in ODB");
		return GIT_ENOTFOUND;
	} else {
		*commit = (git_commit *) object;
		return GIT_OK;
	}
}

static VALUE rugged_commitish_or_treeish(VALUE rb_repo, VALUE rb_spec, int is_commit, int as_obj) {
	int error;
	git_repository *repo;
	git_commit *commit;
	git_tree *tree;
	git_object *object;
	const char *spec;
	VALUE ret;

	Check_Type(rb_spec, T_STRING);
	spec = StringValueCStr(rb_spec);

	rugged_check_repo(rb_repo);
	Data_Get_Struct(rb_repo, git_repository, repo);

	error = rugged_commitish(repo, spec, &commit);
	rugged_exception_check(error);
	object = (git_object *) commit;

	if (!is_commit) {
		error = git_commit_tree(&tree, commit);
		git_commit_free(commit);
		rugged_exception_check(error);
		object = (git_object *) tree;
	}

	if (!as_obj) {
		ret = rugged_create_oid(git_object_id(object));
		git_object_free(object);
	} else {
		ret = rugged_object_new(rb_repo, object);
	}
	return ret;
}

/*
 *  call-seq: Object.commitish(repo, str) -> object
 *
 *  Find and return a single commit inside +repo+ as specified by the
 *  git revision string +str+.
 *
 */
VALUE rb_git_object_commitish(VALUE klass, VALUE rb_repo, VALUE rb_spec) {
	return rugged_commitish_or_treeish(rb_repo, rb_spec, 1, 1);
}

/*
 *  call-seq: Object.commitish_id(repo, str) -> object
 *
 *  Find and return a single commit oid inside +repo+ as specified by the
 *  git revision string +str+.
 *
 */
VALUE rb_git_object_commitish_id(VALUE klass, VALUE rb_repo, VALUE rb_spec) {
	return rugged_commitish_or_treeish(rb_repo, rb_spec, 1, 0);
}

/*
 *  call-seq: Object.treeish(repo, str) -> object
 *
 *  Find and return the root tree from commit inside +repo+ as specified by the
 *  git revision string +str+.
 *
 */
VALUE rb_git_object_treeish(VALUE klass, VALUE rb_repo, VALUE rb_spec) {
	return rugged_commitish_or_treeish(rb_repo, rb_spec, 0, 1);
}

/*
 *  call-seq: Object.treeish_id(repo, str) -> object
 *
 *  Find and return the root tree oid from commit inside +repo+ as specified by the
 *  git revision string +str+.
 *
 */
VALUE rb_git_object_treeish_id(VALUE klass, VALUE rb_repo, VALUE rb_spec) {
	return rugged_commitish_or_treeish(rb_repo, rb_spec, 0, 0);
}

/*
 *  call-seq: Object.rev_parse(repo, str) -> object
 *
 *  Find and return a single object inside +repo+ as specified by the
 *  git revision string +str+.
 *
 *  See http://git-scm.com/docs/git-rev-parse.html#_specifying_revisions or
 *  <code>man gitrevisions</code> for information on the accepted syntax.
 *
 *  Raises a Rugged::InvalidError if +str+ does not contain a valid revision string.
 */
VALUE rb_git_object_rev_parse(VALUE klass, VALUE rb_repo, VALUE rb_spec)
{
	return rugged_object_rev_parse(rb_repo, rb_spec, 1);
}

/*
 *  call-seq: Object.rev_parse_oid(repo, str) -> oid
 *
 *  Find and return the id of the object inside +repo+ as specified by the
 *  git revision string +str+.
 *
 *  See http://git-scm.com/docs/git-rev-parse.html#_specifying_revisions or
 *  <code>man gitrevisions</code> for information on the accepted syntax.
 *
 *  Raises a Rugged::InvalidError if +str+ does not contain a valid revision string.
 */
VALUE rb_git_object_rev_parse_oid(VALUE klass, VALUE rb_repo, VALUE rb_spec)
{
	return rugged_object_rev_parse(rb_repo, rb_spec, 0);
}

/*
 *  call-seq: object == other
 *
 *  Returns true only if +object+ and other are both instances or subclasses of
 *  Rugged::Object and have the same object id, false otherwise.
 */
static VALUE rb_git_object_equal(VALUE self, VALUE other)
{
	git_object *a, *b;

	if (!rb_obj_is_kind_of(other, rb_cRuggedObject))
		return Qfalse;

	Data_Get_Struct(self, git_object, a);
	Data_Get_Struct(other, git_object, b);

	return git_oid_cmp(git_object_id(a), git_object_id(b)) == 0 ? Qtrue : Qfalse;
}

/*
 *  call-seq: object.oid -> oid
 *
 *  Return the Object ID (a 40 character SHA1 hash) for +object+.
 */
static VALUE rb_git_object_oid_GET(VALUE self)
{
	git_object *object;
	Data_Get_Struct(self, git_object, object);
	return rugged_create_oid(git_object_id(object));
}

/*
 *  call-seq: object.type -> type
 *
 *  Returns the object's type. Can be one of +:commit+, +:tag+, +:tree+ or +:blob+.
 */
static VALUE rb_git_object_type_GET(VALUE self)
{
	git_object *object;
	Data_Get_Struct(self, git_object, object);

	return rugged_otype_new(git_object_type(object));
}

/*
 *  call-seq: object.read_raw -> raw_object
 *
 *  Returns the git object as a Rugged::OdbObject instance.
 */
static VALUE rb_git_object_read_raw(VALUE self)
{
	git_object *object;
	Data_Get_Struct(self, git_object, object);

	return rugged_raw_read(git_object_owner(object), git_object_id(object));
}

void Init_rugged_object(void)
{
	rb_cRuggedObject = rb_define_class_under(rb_mRugged, "Object", rb_cObject);
	rb_define_singleton_method(rb_cRuggedObject, "lookup", rb_git_object_lookup, 2);
	rb_define_singleton_method(rb_cRuggedObject, "exist?", rb_git_object_exists, 2);
	rb_define_singleton_method(rb_cRuggedObject, "exists?", rb_git_object_exists, 2);
	rb_define_singleton_method(rb_cRuggedObject, "rev_parse", rb_git_object_rev_parse, 2);
	rb_define_singleton_method(rb_cRuggedObject, "rev_parse_oid", rb_git_object_rev_parse_oid, 2);
	rb_define_singleton_method(rb_cRuggedObject, "commitish", rb_git_object_commitish, 2);
	rb_define_singleton_method(rb_cRuggedObject, "commitish_id", rb_git_object_commitish_id, 2);
	rb_define_singleton_method(rb_cRuggedObject, "treeish", rb_git_object_treeish, 2);
	rb_define_singleton_method(rb_cRuggedObject, "treeish_id", rb_git_object_treeish_id, 2);
	rb_define_singleton_method(rb_cRuggedObject, "new", rb_git_object_lookup, 2);

	rb_define_method(rb_cRuggedObject, "read_raw", rb_git_object_read_raw, 0);
	rb_define_method(rb_cRuggedObject, "==", rb_git_object_equal, 1);
	rb_define_method(rb_cRuggedObject, "oid", rb_git_object_oid_GET, 0);
	rb_define_method(rb_cRuggedObject, "type", rb_git_object_type_GET, 0);
}
