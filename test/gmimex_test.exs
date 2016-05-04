defmodule GmimexTest do
  use ExUnit.Case
  doctest Gmimex

  setup_all do
    IO.puts "Restore emails"
    GmimexTest.Helpers.restore_from_backup
    :ok
  end


  test "json of simple email" do
    {:ok, json} = Gmimex.get_json Path.expand("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS")
    assert json["to"] == [%{"address" => "blue@tester.ch"}]
    assert json["date"] == "Thu, 24 Sep 2015 13:55:49 +0200"
    assert json["from"] == [%{"address" => "bonsplans@newsletter.voyages-sncf.com", "name" => "Voyages-sncf.com"}]
    assert json["subject"] == "PETITS PRIX : 2 millions de billets a prix Prem's avec TGV et Intercites !"
  end


  test "extended json of simple email" do
    {:ok, json} = Gmimex.get_json Path.expand("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS"), content: false
    assert json["to"] == [%{"address" => "blue@tester.ch"}]
    assert json["date"] == "Thu, 24 Sep 2015 13:55:49 +0200"
    assert json["from"] == [%{"address" => "bonsplans@newsletter.voyages-sncf.com", "name" => "Voyages-sncf.com"}]
    assert json["subject"] == "PETITS PRIX : 2 millions de billets a prix Prem's avec TGV et Intercites !"
    assert Enum.any?(json["flags"], &(&1 == :flagged))
    assert Enum.any?(json["flags"], &(&1 == :replied))
    assert Enum.any?(json["flags"], &(&1 == :seen))
    assert !Enum.any?(json["flags"], &(&1 == :attachments))
  end


  test "multiple emails json" do
    messages = [
      Path.expand("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS"),
      Path.expand("test/data/test.com/aaa/new/1444073250_1.24235.brumbrum,U=1098,FMD5=7e33429f656f1e6e9d79b29c3f82c57e"),
      Path.expand("test/data/test.com/aaa/cur/1447089870_2.27636.brumbrum,U=1634,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,S")
    ]
    {:ok, json_list} = Gmimex.get_json messages, content: false
    assert Enum.count(messages) == Enum.count(json_list)
  end


  test "find file" do
    # we take an existing email but, replace the 'cur' dir to 'new' to see if we still get the correct email
    path = Path.expand("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS")
    new_path = path |> String.replace("cur", "new")

    {:ok, email_path} = Gmimex.find_email_path(new_path)

    assert email_path == path
  end


  test "move to cur and back" do
    GmimexTest.Helpers.restore_from_backup
    # we take an existing email but, replace the 'cur' dir to 'new' to see if we still get the correct email
    path = Path.expand("test/data/test.com/aaa/new/1444073250_1.24235.brumbrum,U=1098,FMD5=7e33429f656f1e6e9d79b29c3f82c57e")
    new_path = Path.expand("test/data/test.com/aaa/cur/1444073250_1.24235.brumbrum,U=1098,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,")

    {:ok, res_path} = Gmimex.move_to_cur path
    assert new_path, res_path
    assert File.exists? new_path
    refute File.exists? path

    {:ok, res_path} = Gmimex.move_to_new new_path
    assert new_path, res_path
    assert File.exists? path
    refute File.exists? new_path
  end


  test "move to Drafts and back" do
    GmimexTest.Helpers.restore_from_backup
    base_path = Path.expand("test/data/test.com/aaa")
    path = Path.expand("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS")
    expected_path = Path.expand("test/data/test.com/aaa/Drafts/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS")

    {:ok, res_path} = Gmimex.move_message_to_folder(base_path, path, folder: "/Drafts")
    assert expected_path == res_path
    assert File.exists?(expected_path)
    refute File.exists?(path)

    {:ok, res_path} = Gmimex.move_message_to_folder(base_path, res_path, folder: ".")
    assert path == res_path
    assert File.exists?(path)
    refute File.exists?(expected_path)
  end


  test "seen!" do
    path = Path.expand("test/data/test.com/aaa/new/1447089870_2.27636.brumbrum,U=1634,FMD5=7e33429f656f1e6e9d79b29c3f82c57e")
    expected_path = Path.expand("test/data/test.com/aaa/cur/1447089870_2.27636.brumbrum,U=1634,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,S")
    res_path = Gmimex.seen! path
    assert File.exists?(res_path)
    assert expected_path == res_path
    GmimexTest.Helpers.restore_from_backup
  end


  test "seen! 2" do
    path = Path.expand("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS")
    expected_path = Path.expand("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS")
    res_path = Gmimex.seen! path
    assert File.exists?(res_path)
    assert expected_path == res_path
    GmimexTest.Helpers.restore_from_backup
  end


  test "unseen! with moving from new to cur" do
    path = Path.expand("test/data/test.com/aaa/new/1447089870_2.27636.brumbrum,U=1634,FMD5=7e33429f656f1e6e9d79b29c3f82c57e")
    expected_path = Path.expand("test/data/test.com/aaa/cur/1447089870_2.27636.brumbrum,U=1634,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,")
    res_path = Gmimex.seen! path, value: false
    assert File.exists?(res_path)
    assert expected_path == res_path
    GmimexTest.Helpers.restore_from_backup
  end


  test "unseen! 2" do
    path = Path.expand("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS")
    expected_path = Path.expand("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FR")
    res_path = Gmimex.seen!(path, value: false)
    assert File.exists?(res_path)
    assert expected_path == res_path
    GmimexTest.Helpers.restore_from_backup
  end


  test "get_json with list" do
    path = Path.expand("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS")
    {:ok, [email]} = Gmimex.get_json([path])
    # assert Map.has_key?(email["text"], "preview")
  end


  test "read folder and count the number of emails" do
    path = Path.expand(Path.expand("test/data/test.com/aaa"))
    sorted_emails = Gmimex.read_folder(path)
    {:ok, new_file_listings} =  File.ls("test/data/test.com/aaa/cur")
    assert Enum.count(new_file_listings) == Enum.count(sorted_emails)
    GmimexTest.Helpers.restore_from_backup
  end


  test "read folder and check that new is now empty" do
    path = Path.expand(Path.expand("test/data/test.com/aaa"))
    sorted_emails = Gmimex.read_folder(path)
    {:ok, new_file_listings} =  File.ls("test/data/test.com/aaa/tmp")
    assert new_file_listings == [".gitignore"]
    GmimexTest.Helpers.restore_from_backup
  end


  test "read folder with to_idx > number of emails present" do
    path = Path.expand(Path.expand("test/data/test.com/aaa"))
    sorted_emails = Gmimex.read_folder(path, 0, 10000)
    {:ok, new_file_listings} =  File.ls("test/data/test.com/aaa/cur")
    assert Enum.count(new_file_listings) == Enum.count(sorted_emails)
    GmimexTest.Helpers.restore_from_backup
  end


  test "read folder twice" do
    path = Path.expand(Path.expand("test/data/test.com/aaa"))
    # Read the folder twice! Because the new emails are moved around,
    # to see if it also works with an empty new directory
    sorted_emails = Gmimex.read_folder(path)
    sorted_emails = Gmimex.read_folder(path)
    {:ok, new_file_listings} =  File.ls("test/data/test.com/aaa/tmp")
    assert new_file_listings == [".gitignore"]
    first_email =  List.first(sorted_emails)
    assert first_email["subject"] == "Atrachment"
    GmimexTest.Helpers.restore_from_backup
  end


  test "folder list" do
    mailbox_path = Path.expand(Path.expand("test/data/test.com/aaa"))

    folders = Gmimex.folder_list(mailbox_path)
    assert Enum.member?(folders, "/")
    assert Enum.member?(folders, "/.Drafts")
  end


  # test "folder list with stats" do
  #   mailbox_path = Path.expand(Path.expand("test/data/test.com/aaa"))

  #   folders = Gmimex.folder_list_with_stats(mailbox_path)
  #   assert folders == [%{path: "/", unread_messages: 5, new_messages: 3, total_messages: 6, read_messages: 1, unread_messages: 5},
  #                      %{new_messages: 0, path: "/.Drafts", read_messages: 0, total_messages: 0, unread_messages: 0}]
  # end

end
