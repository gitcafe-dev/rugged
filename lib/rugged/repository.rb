module Rugged
  # Repository is an interface into a Git repository on-disk. It's the primary
  # interface between your app and the main Git objects Rugged makes available
  # to you.
  class Repository
    # Pretty formatting of a Repository.
    #
    # Returns a very pretty String.
    def inspect
      "#<Rugged::Repository:#{object_id} {path: #{path.inspect}}>"
    end

    # Get the most recent commit from this repo.
    #
    # Returns a Rugged::Commit object.
    def last_commit
      self.head.target
    end

    # Checkout the specified branch, reference or commit.
    #
    # target - A revparse spec for the branch, reference or commit to check out.
    # options - Options passed to #checkout_tree.
    def checkout(target, options = {})
      options[:strategy] ||= :safe
      options.delete(:paths)

      return checkout_head(options) if target == "HEAD"

      if target.kind_of?(Rugged::Branch)
        branch = target
      else
        branch = branches[target]
      end

      if branch
        self.checkout_tree(branch.target, options)

        if branch.remote?
          references.create("HEAD", branch.target_id, force: true)
        else
          references.create("HEAD", branch.canonical_name, force: true)
        end
      else
        commit = Commit.lookup(self, self.rev_parse_oid(target))
        references.create("HEAD", commit.oid, force: true)
        self.checkout_tree(commit, options)
      end
    end

    def diff(left, right, opts = {})
      left = rev_parse(left) if left.kind_of?(String)
      right = rev_parse(right) if right.kind_of?(String)

      if !left.is_a?(Rugged::Tree) && !left.is_a?(Rugged::Commit) && !left.nil?
        raise TypeError, "Expected a Rugged::Tree or Rugged::Commit instance"
      end

      if !right.is_a?(Rugged::Tree) && !right.is_a?(Rugged::Commit) && !right.nil?
        raise TypeError, "Expected a Rugged::Tree or Rugged::Commit instance"
      end

      if left
        left.diff(right, opts)
      elsif right
        right.diff(left, opts.merge(:reverse => !opts[:reverse]))
      end
    end

    def diff_workdir(left, opts = {})
      left = rev_parse(left) if left.kind_of?(String)

      if !left.is_a?(Rugged::Tree) && !left.is_a?(Rugged::Commit)
        raise TypeError, "Expected a Rugged::Tree or Rugged::Commit instance"
      end

      left.diff_workdir(opts)
    end

    # Walks over a set of commits using Rugged::Walker.
    #
    # from    - The String SHA1 to push onto Walker to begin our walk.
    # sorting - The sorting order of the commits, as defined in the README.
    # opts    - The walker options.
    # block   - A block that we pass into walker#each.
    #
    # Returns nothing if called with a block, otherwise returns an instance of
    # Enumerable::Enumerator containing Rugged::Commit objects.
    def walk(from, sorting=Rugged::SORT_DATE, opts={}, &block)
      walker = Rugged::Walker.new(self)
      walker.sorting(sorting)
      walker.push(from)
      walker.each(opts, &block)
    end

    # Look up a SHA1.
    #
    # Returns one of the four classes that inherit from Rugged::Object.
    def lookup(oid)
      Rugged::Object.lookup(self, oid)
    end

    # Look up a SHA1.
    #
    # Returns Rugged::Commit.
    def lookup_commit(oid)
      Rugged::Commit.lookup(self, oid)
    end

    # Look up a SHA1.
    #
    # Returns Rugged::Tree.
    def lookup_tree(oid)
      Rugged::Tree.lookup(self, oid)
    end

    # Look up a SHA1.
    #
    # Returns Rugged::Blob.
    def lookup_blob(oid)
      Rugged::Blob.lookup(self, oid)
    end

    # Look up a SHA1.
    #
    # Returns Rugged::Tag.
    def lookup_tag(oid)
      Rugged::Tag::Annotation.lookup(self, oid)
    end

    # Look up a commit by a revision string.
    #
    # Returns Rugged::Commit, it will automatically dereference tag until a commit is got.
    def commitish(spec)
      Rugged::Object.commitish(self, spec)
    end

    # Look up a tree by a revision string.
    #
    # Returns Rugged::Tree, it will automatically dereference tag until a commit is got.
    def treeish(spec)
      Rugged::Object.treeish(self, spec)
    end

    # Look up an object by a revision string.
    #
    # Returns one of the four classes that inherit from Rugged::Object.
    def rev_parse(spec)
      Rugged::Object.rev_parse(self, spec)
    end

    # Look up an object by a revision string.
    #
    # Returns the oid of the matched object as a String
    def rev_parse_oid(spec)
      Rugged::Object.rev_parse_oid(self, spec)
    end

    # Look up a single reference by name.
    #
    # Example:
    #
    #   repo.ref 'refs/heads/master'
    #   # => #<Rugged::Reference:2199125780 {name: "refs/heads/master",
    #          target: "25b5d3b40c4eadda8098172b26c68cf151109799"}>
    #
    # Returns a Rugged::Reference.
    def ref(ref_name)
      references[ref_name]
    end

    def refs(glob = nil)
      references.each(glob)
    end

    def references
      @references ||= ReferenceCollection.new(self)
    end

    def ref_names(glob = nil)
      references.each_name(glob)
    end

    # All the tags in the repository.
    #
    # Returns an TagCollection containing all the tags.
    def tags
      @tags ||= TagCollection.new(self)
    end

    # All the remotes in the repository.
    #
    # Returns a Rugged::RemoteCollection containing all the Rugged::Remote objects
    # in the repository.
    def remotes
      @remotes ||= RemoteCollection.new(self)
    end

    # All the branches in the repository
    #
    # Returns an BranchCollection containing Rugged::Branch objects
    def branches
      @branches ||= BranchCollection.new(self)
    end

    # All the submodules in the repository
    #
    # Returns an SubmoduleCollection containing Rugged::Submodule objects
    def submodules
      @submodules ||= SubmoduleCollection.new(self)
    end

    # Create a new branch in the repository
    #
    # name - The name of the branch (without a full reference path)
    # sha_or_ref - The target of the branch; either a String representing
    # an OID or a reference name, or a Rugged::Object instance.
    #
    # Returns a Rugged::Branch object
    def create_branch(name, sha_or_ref = "HEAD")
      case sha_or_ref
      when Rugged::Object
        target = sha_or_ref.oid
      else
        target = rev_parse_oid(sha_or_ref)
      end

      branches.create(name, target)
    end

    # Get the blob at a path for a specific revision.
    #
    # revision - The String SHA1.
    # path     - The String file path.
    #
    # Returns a String.
    def blob_at(revision, path)
      tree = Rugged::Commit.lookup(self, revision).tree
      begin
        blob_data = tree.path(path)
      rescue Rugged::TreeError
        return nil
      end
      blob = Rugged::Blob.lookup(self, blob_data[:oid])
      (blob.type == :blob) ? blob : nil
    end

    def fetch(remote_or_url, *args)
      unless remote_or_url.kind_of? Remote
        remote_or_url = remotes[remote_or_url] || remotes.create_anonymous(remote_or_url)
      end

      remote_or_url.fetch(*args)
    end

    # Push a list of refspecs to the given remote.
    #
    # refspecs - A list of refspecs that should be pushed to the remote.
    #
    # Returns a hash containing the pushed refspecs as keys and
    # any error messages or +nil+ as values.
    def push(remote_or_url, *args)
      unless remote_or_url.kind_of? Remote
        remote_or_url = remotes[remote_or_url] || remotes.create_anonymous(remote_or_url)
      end

      remote_or_url.push(*args)
    end
  end
end
