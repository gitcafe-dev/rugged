#include "rugged.h"

extern VALUE rb_cRuggedCommit;
VALUE rb_cRuggedCommitStats;

typedef struct commit_stats commit_stats;

struct rb_git_commit_stats_cb_args {
    int adds, dels;
    char *path_only;
};

static int git_commit_stats_cb(
    const git_diff_delta *delta,
    const git_diff_hunk *hunk,
    const git_diff_line *line,
    void *payload)
{
    struct rb_git_commit_stats_cb_args *args = payload;

    if (!args->path_only || (strcmp(args->path_only, delta->old_file.path) == 0 && strcmp(args->path_only, delta->new_file.path) == 0)) {
        switch (line->origin) {
        case GIT_DIFF_LINE_ADDITION: args->adds++; break;
        case GIT_DIFF_LINE_DELETION: args->dels++; break;
        default: break;
        }
    }

    return GIT_OK;
}

static void git_commit_stats__free(commit_stats *stats) {
    git_signature_free(stats->committer);
    git_signature_free(stats->author);
    xfree(stats);
}

VALUE rugged_commit_stats_new(struct commit_stats *stats) {
    return Data_Wrap_Struct(rb_cRuggedCommitStats, NULL, git_commit_stats__free, stats);
}

int git_commit_stats_of(git_repository *repo, git_tree *tree, git_tree *parent_tree,
                        const char *path_only, size_t *adds, size_t *dels) {
    int error;
    git_diff *diff;
    git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;
    struct rb_git_commit_stats_cb_args args;

    error = git_diff_tree_to_tree(&diff, repo, parent_tree, tree, &diff_opts);
    if (error == GIT_OK) {
        args.adds = args.dels = 0;
        args.path_only = path_only;
        error = git_diff_foreach(diff, NULL, NULL, NULL, git_commit_stats_cb, &args);
        git_diff_free(diff);
        *adds = args.adds;
        *dels = args.dels;
    }
    return error;
}

VALUE rugged_commit_stats_of(git_repository *repo, git_commit *commit, const char *path_only) {
    int error;
    commit_stats *stats;
    git_tree *tree, *parent_tree;
    git_commit *parent_commit;

    error = git_commit_tree(&tree, commit);
    rugged_exception_check(error);

    stats = xmalloc(sizeof(commit_stats));
    stats->adds = stats->dels = 0;
    git_signature_dup(&stats->committer, git_commit_committer(commit));
    git_signature_dup(&stats->author, git_commit_author(commit));
    git_oid_cpy(&stats->oid, git_commit_id(commit));

    // Commit must not be merge commit here!
    error = git_commit_parent(&parent_commit, commit, 0);
    if (error == GIT_OK) {
        error = git_commit_tree(&parent_tree, parent_commit);
        git_commit_free(parent_commit);
    } else if (error == GIT_ENOTFOUND) { // If commit has no parent, still ok
        parent_tree = NULL;
        error = GIT_OK;
    }

    if (error != GIT_OK) {
        xfree(stats);
        git_tree_free(tree);
        rugged_exception_check(error);
    }

    error = git_commit_stats_of(repo, tree, parent_tree, path_only, &stats->adds, &stats->dels);
    git_tree_free(tree);
    if (parent_tree) git_tree_free(parent_tree);
    if (error != GIT_OK) {
        xfree(stats);
        rugged_exception_check(error);
    }

    return rugged_commit_stats_new(stats);
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

void Init_rugged_commit_stat(void)
{
    rb_cRuggedCommitStats = rb_define_class_under(rb_cRuggedCommit, "Stats", rb_cObject);
    rb_define_method(rb_cRuggedCommitStats, "adds", rb_git_commit_stats_adds_GET, 0);
    rb_define_method(rb_cRuggedCommitStats, "dels", rb_git_commit_stats_dels_GET, 0);
    rb_define_method(rb_cRuggedCommitStats, "committer", rb_git_commit_stats_committer_GET, 0);
    rb_define_method(rb_cRuggedCommitStats, "author", rb_git_commit_stats_author_GET, 0);
    rb_define_method(rb_cRuggedCommitStats, "oid", rb_git_commit_stats_oid_GET, 0);
}
