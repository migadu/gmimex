defmodule GmimexTest do
  use ExUnit.Case
  doctest Gmimex

  setup_all do
    IO.puts "Restore emails"
    GmimexTest.Helpers.restore_from_backup
    :ok
  end


  test "json of simple email" do
    {:ok, json} = Gmimex.get_json Path.expand("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS"), flags: false
    assert json["to"] == [%{"address" => "blue@tester.ch"}]
    assert json["date"] == "Thu, 24 Sep 2015 13:55:49 +0200"
    assert json["from"] == %{"address" => "bonsplans@newsletter.voyages-sncf.com", "name" => "Voyages-sncf.com"}
    assert json["subject"] == "PETITS PRIX : 2 millions de billets a prix Prem's avec TGV et Intercites !"
  end


  test "extended json of simple email" do
    {:ok, json} = Gmimex.get_json Path.expand("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS"), flags: true, content: false
    assert json["to"] == [%{"address" => "blue@tester.ch"}]
    assert json["date"] == "Thu, 24 Sep 2015 13:55:49 +0200"
    assert json["from"] == %{"address" => "bonsplans@newsletter.voyages-sncf.com", "name" => "Voyages-sncf.com"}
    assert json["subject"] == "PETITS PRIX : 2 millions de billets a prix Prem's avec TGV et Intercites !"
    assert json["flags"]["flagged"] == true
    assert json["flags"]["replied"] == true
    assert json["flags"]["seen"] == true
    assert json["flags"]["attachments"] == true
  end


  test "multiple emails json" do
    messages = [
      Path.expand("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS"),
      Path.expand("test/data/test.com/aaa/new/1444073250_1.24235.brumbrum,U=1098,FMD5=7e33429f656f1e6e9d79b29c3f82c57e"),
      Path.expand("test/data/test.com/aaa/cur/1447089870_2.27636.brumbrum,U=1634,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,S")
    ]
    {:ok, json_list} = Gmimex.get_json messages, flags: true, content: false
    assert Enum.count(messages) == Enum.count(json_list)
  end


  test "fetch file" do
    # we take an existing email but, replace the 'cur' dir to 'new' to see if we still get the correct email
    path = Path.expand("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS")
    new_path = path |> String.replace("cur", "new")

    {:ok, json} = Gmimex.get_json new_path, flags: true, content: false
    assert json["path"] == path
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


  test "unseen!" do
    path = Path.expand("test/data/test.com/aaa/new/1447089870_2.27636.brumbrum,U=1634,FMD5=7e33429f656f1e6e9d79b29c3f82c57e")
    expected_path = Path.expand("test/data/test.com/aaa/cur/1447089870_2.27636.brumbrum,U=1634,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,")
    res_path = Gmimex.seen! path, false
    assert File.exists?(res_path)
    assert expected_path == res_path
    GmimexTest.Helpers.restore_from_backup
  end


  test "unseen! 2" do
    path = Path.expand("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS")
    expected_path = Path.expand("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FR")
    res_path = Gmimex.seen!(path, false)
    assert File.exists?(res_path)
    assert expected_path == res_path
    GmimexTest.Helpers.restore_from_backup
  end


  test "get_json with list" do
    path = Path.expand("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS")
    {:ok, [email]} = Gmimex.get_json([path])
    assert Map.has_key?(email["text"], "preview")
  end


  test "read folder" do
    base_dir = Path.expand("test/data")
    email = "aaa@test.com"
    folder = "."
    {sorted_emails, complete_emails} = Gmimex.read_folder(base_dir, email, folder)
    {:ok, new_file_listings} =  File.ls("test/data/test.com/aaa/tmp")
    assert new_file_listings == [".gitignore"]
    first_email =  List.first(sorted_emails)
    assert first_email["subject"] == "Atrachment"
    assert first_email["flags"]["attachments"] == true
    GmimexTest.Helpers.restore_from_backup
  end


  test "read folder twice" do
    base_dir = Path.expand("test/data")
    email = "aaa@test.com"
    folder = "."
    # Read the folder twice! Because the new emails are moved around,
    # to see if it also works with an empty new directory
    {sorted_emails, complete_emails} = Gmimex.read_folder(base_dir, email, folder)
    {sorted_emails, complete_emails} = Gmimex.read_folder(base_dir, email, folder)
    {:ok, new_file_listings} =  File.ls("test/data/test.com/aaa/tmp")
    assert new_file_listings == [".gitignore"]
    first_email =  List.first(sorted_emails)
    assert first_email["subject"] == "Atrachment"
    assert first_email["flags"]["attachments"] == true
    GmimexTest.Helpers.restore_from_backup
  end


  test "the previews of the selected emails" do
    base_dir = Path.expand("test/data")
    email = "aaa@test.com"
    folder = "."
    {sorted_emails, complete_emails} = Gmimex.read_folder(base_dir, email, folder, 0, 2)
    {:ok, new_file_listings} =  File.ls("test/data/test.com/aaa/tmp")
    assert new_file_listings == [".gitignore"]
    assert Enum.count(complete_emails) == 2
    first_complete_email =  List.first(complete_emails)
    first_sorted_email =  List.first(sorted_emails)
    assert first_complete_email["subject"] == first_sorted_email["subject"]
    assert Map.has_key?(first_complete_email["text"], "preview")
    GmimexTest.Helpers.restore_from_backup
  end


  test "the previews of the selected emails, sliced" do
    base_dir = Path.expand("test/data")
    email = "aaa@test.com"
    folder = "."
    {sorted_emails, complete_emails} = Gmimex.read_folder(base_dir, email, folder, 2, 4)
    {:ok, new_file_listings} =  File.ls("test/data/test.com/aaa/tmp")
    assert new_file_listings == [".gitignore"]
    assert Enum.count(complete_emails) == 2
    first_complete_email  =  Enum.at(complete_emails, 0)
    first_sorted_email    =  Enum.at(sorted_emails, 2)
    second_complete_email =  Enum.at(complete_emails, 1)
    second_sorted_email   =  Enum.at(sorted_emails, 3)
    assert first_complete_email["subject"] == first_sorted_email["subject"]
    assert second_complete_email["subject"] == second_sorted_email["subject"]
    GmimexTest.Helpers.restore_from_backup
  end
end
