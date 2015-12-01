require "test_helper"
require 'base64'

class ObjectTest < Rugged::TestCase
  def setup
    @repo = FixtureRepo.from_rugged("testrepo.git")
  end

  def test_lookup_can_lookup_any_object_type
    blob = Rugged::Object.lookup(@repo, "fa49b077972391ad58037050f2a75f74e3671e92")
    assert_instance_of Rugged::Blob, blob

    commit = Rugged::Object.lookup(@repo, "8496071c1b46c854b31185ea97743be6a8774479")
    assert_instance_of Rugged::Commit, commit

    tag = Rugged::Object.lookup(@repo, "0c37a5391bbff43c37f0d0371823a5509eed5b1d")
    assert_instance_of Rugged::Tag::Annotation, tag

    tree = Rugged::Object.lookup(@repo, "c4dc1555e4d4fa0e0c9c3fc46734c7c35b3ce90b")
    assert_instance_of Rugged::Tree, tree

    subclass = Class.new(Rugged::Object)

    blob = subclass.lookup(@repo, "fa49b077972391ad58037050f2a75f74e3671e92")
    assert_instance_of Rugged::Blob, blob

    commit = subclass.lookup(@repo, "8496071c1b46c854b31185ea97743be6a8774479")
    assert_instance_of Rugged::Commit, commit

    tag = subclass.lookup(@repo, "0c37a5391bbff43c37f0d0371823a5509eed5b1d")
    assert_instance_of Rugged::Tag::Annotation, tag

    tree = subclass.lookup(@repo, "c4dc1555e4d4fa0e0c9c3fc46734c7c35b3ce90b")
    assert_instance_of Rugged::Tree, tree
  end

  def test_fail_to_lookup_inexistant_object
    assert_raises Rugged::OdbError do
      @repo.lookup("a496071c1b46c854b31185ea97743be6a8774479")
    end
  end

  def test_exists_can_validate_any_object_type
    assert Rugged::Blob.exists?(@repo, "fa49b077")
    assert Rugged::Commit.exists?(@repo, "8496071c")
    assert Rugged::Tag::Annotation.exists?(@repo, "0c37a539")
    assert Rugged::Tree.exists?(@repo, "c4dc1555")
    assert !Rugged::Commit.exists?(@repo, "fa49b077")
  end

  def test_peel_to_specified_type
    object = Rugged::Object.peel(@repo, '8496071c')
    assert_instance_of Rugged::Tree, object
    assert_equal object.oid, '181037049a54a1eb5fab404658a3a250b44335d7'
    assert_equal Rugged::Object.peel_oid(@repo, '8496071c'), '181037049a54a1eb5fab404658a3a250b44335d7'
    assert Rugged::Object.can_peel?(@repo, '8496071c')
    assert Rugged::Tree.can_peel?(@repo, '8496071c')

    object = Rugged::Commit.peel(@repo, '8496071c')
    assert_instance_of Rugged::Commit, object
    assert_equal object.oid, '8496071c1b46c854b31185ea97743be6a8774479'
    assert_equal Rugged::Commit.peel_oid(@repo, '8496071c'), '8496071c1b46c854b31185ea97743be6a8774479'
    assert Rugged::Commit.can_peel?(@repo, '8496071c')

    assert_raises Rugged::ObjectError do
      Rugged::Object.peel(@repo, '18103704')
    end

    assert_raises Rugged::ReferenceError do
      Rugged::Object.peel(@repo, '00000000')
    end

    object = Rugged::Commit.peel(@repo, 'v0.9')
    assert_instance_of Rugged::Commit, object
    assert_equal object.oid, '5b5b025afb0b4c913b4c338a42934a3863bf3644'
    assert_equal Rugged::Commit.peel_oid(@repo, 'v0.9'), '5b5b025afb0b4c913b4c338a42934a3863bf3644'
    assert Rugged::Commit.can_peel?(@repo, 'v0.9')

    object = Rugged::Tree.peel(@repo, 'v0.9')
    assert_instance_of Rugged::Tree, object
    assert_equal object.oid, 'f60079018b664e4e79329a7ef9559c8d9e0378d1'
    assert_equal Rugged::Tree.peel_oid(@repo, 'v0.9'), 'f60079018b664e4e79329a7ef9559c8d9e0378d1'
    assert Rugged::Tree.can_peel?(@repo, 'v0.9')
    assert !Rugged::Tag::Annotation.can_peel?(@repo, 'v0.9')

    object = Rugged::Commit.peel(@repo, 'v1.0')
    assert_instance_of Rugged::Commit, object
    assert_equal object.oid, '5b5b025afb0b4c913b4c338a42934a3863bf3644'
    assert_equal Rugged::Commit.peel_oid(@repo, 'v1.0'), '5b5b025afb0b4c913b4c338a42934a3863bf3644'
    assert Rugged::Commit.can_peel?(@repo, 'v1.0')

    object = Rugged::Tree.peel(@repo, 'v1.0')
    assert_instance_of Rugged::Tree, object
    assert_equal object.oid, 'f60079018b664e4e79329a7ef9559c8d9e0378d1'
    assert_equal Rugged::Tree.peel_oid(@repo, 'v1.0'), 'f60079018b664e4e79329a7ef9559c8d9e0378d1'
    assert Rugged::Tree.can_peel?(@repo, 'v1.0')

    object = Rugged::Tag::Annotation.peel(@repo, 'v1.0')
    assert_instance_of Rugged::Tag::Annotation, object
    assert_equal object.oid, '0c37a5391bbff43c37f0d0371823a5509eed5b1d'
    assert_equal Rugged::Tag::Annotation.peel_oid(@repo, 'v1.0'), '0c37a5391bbff43c37f0d0371823a5509eed5b1d'
    assert Rugged::Tag::Annotation.can_peel?(@repo, 'v1.0')
  end

  def test_lookup_object
    obj = @repo.lookup("8496071c1b46c854b31185ea97743be6a8774479")
    assert_equal :commit, obj.type
    assert_equal '8496071c1b46c854b31185ea97743be6a8774479', obj.oid
  end

  def test_objects_are_the_same
    obj = @repo.lookup("8496071c1b46c854b31185ea97743be6a8774479")
    obj2 = @repo.lookup("8496071c1b46c854b31185ea97743be6a8774479")
    assert_equal obj, obj2
  end

  def test_read_raw_data
    obj = @repo.lookup("8496071c1b46c854b31185ea97743be6a8774479")
    assert obj.read_raw
  end

  def test_lookup_by_rev
    obj = @repo.rev_parse("v1.0")
    assert "0c37a5391bbff43c37f0d0371823a5509eed5b1d", obj.oid
    obj = @repo.rev_parse("v1.0^1")
    assert "8496071c1b46c854b31185ea97743be6a8774479", obj.oid
  end

  def test_lookup_oid_by_rev
    oid = @repo.rev_parse_oid("v1.0")
    assert "0c37a5391bbff43c37f0d0371823a5509eed5b1d", oid
    @repo.rev_parse_oid("v1.0^1")
    assert "8496071c1b46c854b31185ea97743be6a8774479", oid
  end
end
