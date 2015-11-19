defmodule Gmimex do

  @get_json_defaults [raw: false, content: true, flags: true]


  def get_json(path, opts \\ [])


  def get_json(path, opts) when is_binary(path) do
    {:ok, do_get_json(path, opts)}
  end


  def get_json(paths, opts) when is_list(paths) do
    {:ok, paths |> Enum.map(&(do_get_json(&1, opts)))}
  end


  defp do_get_json(path, opts) do
    opts = Keyword.merge(@get_json_defaults, opts)
    {:ok, email_path} = fetch_file(path)
    json_bin = GmimexNif.get_json(email_path, opts[:content])
    if opts[:raw] do
      json_bin
    else
      {:ok, data} = Poison.Parser.parse(json_bin)
      if opts[:flags] do
        flags = get_flags(email_path) |> Map.put_new("attachments", data["attachments"] != [])
        data
          |> Map.put_new("path", email_path)
          |> Map.put_new("flags", flags)
      else
        data
      end
    end
  end


  def get_json_list(email_list, opts \\ []) do
    email_list |> Enum.map(fn(x) -> {:ok, email} = Gmimex.get_json(x, opts); email end)
  end


  def get_part(path, part_id) do
    GmimexNif.get_part(path, part_id)
  end


  def index_message(mailbox_path, message_path) do
    GmimexNif.index_message(mailbox_path, message_path)
  end


  def index_mailbox(mailbox_path) do
    GmimexNif.index_mailbox(mailbox_path)
  end


  def search_mailbox(mailbox_path, query, max_results \\ 10) do
    GmimexNif.search_mailbox(mailbox_path, query, max_results)
  end


  @doc """
  Read the emails within a folder.
  Base_dir is the root directory of the mailbox, email the email of the currently
  logged in webmail-user, and folder the folder which defaults to Inbox.
  If from_idx and to_idx is given, the entries in the list of emails is replaced
  with the full email (and thus the preview).
  """
  def read_folder(base_dir, email, folder \\ ".", from_idx \\ 0, to_idx \\ 0) do
    mailbox_path = mailbox_path(base_dir, email, folder)
    move_new_to_cur(mailbox_path)
    cur_path = Path.join(mailbox_path, "cur")
    cur_email_names = files_ordered_by_time_desc(cur_path)
    emails = Enum.map(cur_email_names, fn(x) ->
      {:ok, email} = Gmimex.get_json(Path.join(cur_path, x), flags: true, content: false);email end)
    sorted_emails = Enum.sort(emails, &(&1["sortId"] > &2["sortId"]))

    selection_count = to_idx - from_idx
    if selection_count > 0 do
      selection = Enum.map(Enum.slice(sorted_emails, from_idx, to_idx-from_idx), &(&1["path"]))
      complete_emails = get_json_list(selection, flags: true, content: true)
      nr_elements = Enum.count(sorted_emails)
      {part_1, part_2} = Enum.split(sorted_emails, from_idx)
      {part_2, part_3} = Enum.split(part_2, to_idx - from_idx)
      sorted_emails = part_1 |> Enum.concat(complete_emails) |> Enum.concat(part_3)
    end

    sorted_emails
  end


  # move all files from 'new' to 'cur'
  defp move_new_to_cur(mailbox_path) do
    {:ok, new_emails} = File.ls(Path.join(mailbox_path, "new"))
    Enum.each(new_emails, fn(x) -> filepath = Path.join(Path.join(mailbox_path, "new"), x); move_to_cur(filepath) end)
  end


  defp files_ordered_by_time_desc(dirpath) do
    {files, 0} = System.cmd "ls", ["-t", dirpath]
    # to avoid having returned [""]
    if String.length(files) == 0 do
      []
    else
      files |> String.strip |> String.split("\n")
    end
  end


  @doc """
  Moves an email from 'new' to 'cur' in the mailbox directory.
  """
  def move_to_cur(path) do
    filename = Path.basename(path) |> String.split(":2") |> List.first
    maildirname = Path.dirname(path) |> Path.dirname
    statdirname = Path.dirname(path) |> Path.basename
    if statdirname == "cur" do
      # nothing to do
      {:ok, path}
    else
      new_path = maildirname |> Path.join('cur') |> Path.join("#{filename}:2,")
      :ok = File.rename(path, new_path)
      {:ok, new_path}
    end
  end


  @doc """
  Moves an email from 'cur' to 'new' in the mailbox directory. Should never be
  used except in testing.
  """
  def move_to_new(path) do
    filename = Path.basename(path) |> String.split(":2") |> List.first
    maildirname = Path.dirname(path) |> Path.dirname
    statdirname = Path.dirname(path) |> Path.basename
    if statdirname == "new" do
      # nothing to do
      {:ok, path}
    else
      new_path = maildirname |> Path.join('new') |> Path.join(filename)
      :ok = File.rename(path, new_path)
      {:ok, new_path}
    end
  end


  @doc """
  Un/Marks an email as passed, that is resent/forwarded/bounced.
  See http://cr.yp.to/proto/maildir.html for more information.
  """
  def passed!(path, toggle \\ true), do: set_flag(path, "P", toggle)



  @doc """
  Un/Marks an email as replied.
  """
  def replied!(path, toggle \\ true), do: set_flag(path, "R", toggle)


  @doc """
  Un/Marks an email as seen.
  """
  def seen!(path, toggle \\ true), do: set_flag(path, "S", toggle)


  @doc """
  Un/Marks an email as trashed.
  """
  def trashed!(path, toggle \\ true), do: set_flag(path, "T", toggle)


  @doc """
  Un/Marks an email as draft.
  """
  def draft!(path, toggle \\ true), do: set_flag(path, "D", toggle)


  @doc """
  Un/Marks an email as flagged.
  """
  def flagged!(path, toggle \\ true), do: set_flag(path, "F", toggle)


  @doc """
  On a mailserver, the email is often stored as followes for
  the user aaa@bbb.com
  /bbb.com/aaa/{INBOX,Drafts,...}
  mailbox_path converts the email to such a path.
  ## Example
      iex> Gmimex.mailbox_path("/x/", "aaa@bbb.com")
      "/x/bbb.com/aaa"
      iex> Gmimex.mailbox_path("/x/", "aaa@bbb.com", "Draft")
      "/x/bbb.com/aaa/Draft"

  """
  def mailbox_path(base_path, email, folder \\ ".") do
    [user_name, domain] = String.split(email, "@")

    base_path
      |> Path.join(domain)
      |> Path.join(user_name)
      |> Path.join(folder)
      |> Path.expand
  end


  defp set_flag(path, flag, set_toggle) do
    {:ok, path} = move_to_cur(path) # assure the email is in cur
    [filename, flags] = Path.basename(path) |> String.split(":2,")
    flag_present = String.contains?(flags, String.upcase(flag)) || String.contains?(flags, String.downcase(flag))
    new_flags = flags
    new_path = path
    if set_toggle do
      if !flag_present, do:
        new_flags = "#{flags}#{String.upcase(flag)}" |> to_char_list |> Enum.sort |> to_string
    else
      if flag_present, do:
        new_flags = flags |> String.replace(String.upcase(flag),"") |> String.replace(String.downcase(flag), "")
    end
    if new_flags !== flags do
      new_path = Path.join(Path.dirname(path), "#{filename}:2,#{new_flags}")
      File.rename path, new_path
    end
    new_path
  end


  defp fetch_file(path) do
    if File.exists? path do
      {:ok, path}
    else
      dirname = Path.dirname path
      filename = Path.basename(path) |> String.split(":2") |> List.first
      maildirname = Path.dirname dirname
      case Path.wildcard("#{maildirname}/{cur,new}/#{filename}*") do
        []  -> {:err, "Email: #{filename} not found in maildir: #{maildirname}"}
        [x] -> {:ok, x}
        _   -> {:err, "Multiples copies of email: #{filename} not found in maildir: #{maildirname}"}
      end
    end
  end


  defp get_flags(path) do
    flags = Path.basename(path) |> String.split(":2,") |> List.last
    Enum.reduce(to_char_list(flags), %{}, fn(x, acc) ->
      case x do
        80  -> Map.put_new(acc, "passed", true)
        112 -> Map.put_new(acc, "passed", true)
        82  -> Map.put_new(acc, "replied", true)
        114 -> Map.put_new(acc, "replied", true)
        83  -> Map.put_new(acc, "seen", true)
        115 -> Map.put_new(acc, "seen", true)
        84  -> Map.put_new(acc, "trashed", true)
        116 -> Map.put_new(acc, "trashed", true)
        68  -> Map.put_new(acc, "draft", true)
        100 -> Map.put_new(acc, "draft", true)
        70  -> Map.put_new(acc, "flagged", true)
        102 -> Map.put_new(acc, "flagged", true)
        _ -> acc
      end
    end)
  end

end
