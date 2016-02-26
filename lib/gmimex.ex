defmodule Gmimex do

  @get_json_defaults [raw: false, content: false ]
  @flags_default_opts [value: true]
  @move_message_default_opts [folder: "."]


  def get_json(path, opts \\ [])


  def get_json(path, opts) when is_binary(path) do
    {:ok, do_get_json(path, opts)}
  end


  def get_json(paths, opts) when is_list(paths) do
    {:ok, paths |> Enum.map(fn(x) ->
      {:ok, email_path} = find_email_path(x)
      do_get_json(email_path, opts) end)}
  end


  defp do_get_json(path, opts) do
    opts = Keyword.merge(@get_json_defaults, opts)
    unless File.exists?(path), do: raise "Email path: #{path} not found"
    json_bin = GmimexNif.get_json(path, opts[:content])
    if opts[:raw] do
      json_bin
    else
      {:ok, data} = Poison.Parser.parse(json_bin)
      flags = get_flags(path)
      if opts[:content] && data["attachments"] != [], do:
        flags = flags ++ [:attachments]
      data
        |> Map.put("filename", Path.basename(path))
        |> Map.put("path",     path)
        |> Map.put("flags",    flags)
    end
  end


  def get_json_list(email_list, opts \\ []) do
    email_list |> Enum.map(fn(x) -> {:ok, email} = Gmimex.get_json(x, opts); email end)
  end


  def get_part(path, part_id) do
    GmimexNif.get_part(path, part_id)
  end


  @doc """
  Read the emails within a folder.
  Maildir_path is the root directory of the mailbox (without ending in cur,new).
  If from_idx and to_idx is given, the entries in the list of emails is replaced
  with the full email (and thus the preview).
  """
  def read_folder(maildir_path, from_idx \\ 0, to_idx \\ 0, opts \\ []) do
    move_new_to_cur(maildir_path, opts)
    cur_path = Path.join(maildir_path, "cur")
    cur_email_names = files_ordered_by_time_desc(cur_path)
    emails = Enum.map(cur_email_names, fn(x) ->
      {:ok, email} = Gmimex.get_json(Path.join(cur_path, x), content: false);email end)
    sorted_emails = Enum.sort(emails, &(&1["sortId"] > &2["sortId"]))

    if from_idx > (len = Enum.count(sorted_emails)), do: from_idx = len
    if to_idx < 0, do: to_idx = 0

    selection_count = to_idx - from_idx
    if selection_count > 0 do
      selection = Enum.map(Enum.slice(sorted_emails, from_idx, to_idx-from_idx), &(&1["path"]))
      complete_emails = get_json_list(selection, content: false)
      nr_elements = Enum.count(sorted_emails)
      {part_1, part_2} = Enum.split(sorted_emails, from_idx)
      {part_2, part_3} = Enum.split(part_2, to_idx - from_idx)
      sorted_emails = part_1 |> Enum.concat(complete_emails) |> Enum.concat(part_3)
    end

    sorted_emails
  end


  # move all files from 'new' to 'cur'
  def move_new_to_cur(maildir_path, opts) do
    new_path = Path.join(maildir_path, "new")
    unless File.exists?(new_path), do: raise "Path: #{new_path} does not exist"
    {:ok, new_emails} = File.ls(Path.join(maildir_path, "new"))
    Enum.each(new_emails, fn(x) -> filepath = Path.join(new_path, x); move_to_cur(filepath, opts) end)
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
  def move_to_cur(path, opts \\ []) do
    opts = Keyword.merge(@move_message_default_opts, opts)
    filename = Path.basename(path) |> String.split(":2") |> List.first
    maildirname = Path.dirname(path) |> Path.dirname
    statdirname = Path.dirname(path) |> Path.basename
    if statdirname == "cur" do
      # nothing to do
      {:ok, path}
    else
      new_path = maildirname |> Path.join('cur') |> Path.join("#{filename}:2,")
      :ok = File.rename path, new_path
      {:ok, new_path}
    end
  end


  @doc """
  Moves an email from 'cur' to 'new' in the mailbox directory. Should never be
  used except in testing.
  """
  def move_to_new(path, opts \\ []) do
    opts = Keyword.merge(@move_message_default_opts, opts)
    filename = Path.basename(path) |> String.split(":2") |> List.first
    maildirname = Path.dirname(path) |> Path.dirname
    statdirname = Path.dirname(path) |> Path.basename
    if statdirname == "new" do
      # nothing to do
      {:ok, path}
    else
      new_path = maildirname |> Path.join('new') |> Path.join(filename)
      :ok = File.rename path, new_path
      {:ok, new_path}
    end
  end


  @doc """
  Moves an email to a new folder. base_path is the root directory of
  the mailbox on the email server, filename is the path to the email,
  and folder is the new folder where the email should be moved to.
  """
  def move_message_to_folder(base_path, message_path, opts \\ []) do
    opts = Keyword.merge(@move_message_default_opts, opts)
    filename = Path.basename(message_path)
    new_maildir = base_path |> Path.join(opts[:folder]) |> Path.join('new')
    File.mkdir_p! new_maildir
    new_path =  new_maildir |> Path.join(filename)
    :ok = File.rename message_path, new_path
    {:ok, new_path}
  end


  @doc """
  Un/Marks an email as passed, that is resent/forwarded/bounced.
  See http://cr.yp.to/proto/maildir.html for more information.
  """
  def passed!(path, opts \\ []), do: set_flag(path, "P", opts)



  @doc """
  Un/Marks an email as replied.
  """
  def replied!(path, opts \\ []), do: set_flag(path, "R", opts)


  @doc """
  Un/Marks an email as seen.
  """
  def seen!(path, opts \\ []), do: set_flag(path, "S", opts)


  @doc """
  Un/Marks an email as trashed.
  """
  def trashed!(path, opts \\ []), do: set_flag(path, "T", opts)


  @doc """
  Un/Marks an email as draft.
  """
  def draft!(path, opts \\ []), do: set_flag(path, "D", opts)


  @doc """
  Un/Marks an email as flagged.
  """
  def flagged!(path, opts \\ []), do: set_flag(path, "F", opts)


  @doc ~S"""
  Checks if a flag is present in the flags list. Examples:
  ## Example

      iex> Gmimex.has_flag? [:aa, :bb], :aa
      true

      iex> Gmimex.has_flag? [:aa, :bb], :cc
      false
  """
  def has_flag?(flaglist, flag) do
    Enum.any?(flaglist, &(&1 == flag))
  end


  @doc """
  Adds the flags given in the list to the filename.
  All existing flags are removed.
  """
  def flagged_filename(path, flags) do
    filename = Path.basename(path)
    dirname = Path.dirname(path)
    filename = Enum.reduce(flags, fn(x, acc) ->
      case x do
        :passed   -> acc = Gmimex.passed!  filename, false
        :replied  -> acc = Gmimex.replied! filename, false
        :seen     -> acc = Gmimex.seen!    filename, false
        :trashed  -> acc = Gmimex.trashed! filename, false
        :draft    -> acc = Gmimex.draft!   filename, false
        :flagged  -> acc = Gmimex.flagged! filename, false
      end
    end)
    Path.join(dirname, filename)
  end


  @doc """
  Returns the preview of the email, independent if it is in the
  text or html part. Input: message map extracted via get_json.
  """
  def preview(message) do
    if message["text"] && message["text"]["preview"] do
      message["text"]["preview"]
    else
      if message["html"] && message["html"]["preview"] do
        message["html"]["preview"]
      else
        ""
      end
    end
  end


  def update_filename_with_flag(path, flag, value \\ true) do
    [filename, flags] = Path.basename(path) |> String.split(":2,")
    flag_present = String.contains?(flags, String.upcase(flag)) || String.contains?(flags, String.downcase(flag))
    new_flags = flags
    new_path = path
    if value do
      if !flag_present, do:
        new_flags = "#{flags}#{String.upcase(flag)}" |> to_char_list |> Enum.sort |> to_string
    else
      if flag_present, do:
        new_flags = flags |> String.replace(String.upcase(flag),"") |> String.replace(String.downcase(flag), "")
    end
    new_path = Path.join(Path.dirname(path), "#{filename}:2,#{new_flags}")
  end


  defp set_flag(path, flag, opts \\ []) do
    opts = Keyword.merge(@flags_default_opts, opts)
    {:ok, path} = move_to_cur(path, opts) # assure the email is in cur
    new_path = update_filename_with_flag(path, flag, opts[:value])
    if path !== new_path do
      File.rename path, new_path
    end
    new_path
  end


  @doc """
  Finds an email in a directory.
  The issue is that the filename of an email can change, because the filename also contains
  flags set on the email.
  Returns exactly one email.
  ## Example
      iex> Gmimex.find_email_path("test/data/test.com/aaa", "1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e")
      {:ok, "test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS"}
      iex> Gmimex.find_email_path("test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e")
      {:ok, "test/data/test.com/aaa/cur/1443716368_0.10854.brumbrum,U=605,FMD5=7e33429f656f1e6e9d79b29c3f82c57e:2,FRS"}
  """
  def find_email_path(path) do
    maildir = path |> Path.dirname |> Path.dirname
    filename = Path.basename(path)
    find_email_path(maildir, filename)
  end


  def find_email_path(maildir, filename) do
    path = Path.join(maildir, filename)
    if File.exists?(path) do
      # nothing to do
      {:ok, path}
    else
      if String.contains?(filename, ":2"), do: filename = filename |> String.split(":2") |> List.first
      case Path.wildcard("#{maildir}/{cur,new}/#{filename}*") do
        []  -> {:err, "Email: #{filename} not found in maildir: #{maildir}"}
        [x] -> {:ok, x}
        _   -> {:err, "Multiples copies of email: #{filename} found in maildir: #{maildir}"}
      end
    end
  end


  defp get_flags(path) do
    flags = Path.basename(path) |> String.split(":2,") |> List.last
    Enum.reduce(to_char_list(flags), [], fn(x, acc) ->
      case x do
        80  -> acc ++ [:passed]
        112 -> acc ++ [:passed]
        82  -> acc ++ [:replied]
        114 -> acc ++ [:replied]
        83  -> acc ++ [:seen]
        115 -> acc ++ [:seen]
        84  -> acc ++ [:trashed]
        116 -> acc ++ [:trashed]
        68  -> acc ++ [:draft]
        100 -> acc ++ [:draft]
        70  -> acc ++ [:flagged]
        102 -> acc ++ [:flagged]
        _ -> acc
      end
    end)
  end

  @doc """
  Returnes the list of folders including the number of (new) emails
  """
  def folder_list_with_stats(mailbox_path) do
    folder_list(mailbox_path)
      |> Enum.map(fn(x) ->
        folder_path = Path.join(mailbox_path, x)
        stats = folder_stats(folder_path)
        Map.merge(%{path: x}, stats)
      end)

  end


  defp folder_stats(folder_path) do
    new_path = Path.join(folder_path, "new")
    new_cmd = "find #{new_path} -type f | wc -l"
    cur_path = Path.join(folder_path, "cur")
    cur_cmd = "find #{cur_path} -type f | wc -l"
    read_cmd = "find #{cur_path} -type f -name '*2,*S*' | wc -l"
    nr_new = :os.cmd(to_char_list(new_cmd))  |> to_string |> String.strip |> String.to_integer
    nr_cur = :os.cmd(to_char_list(cur_cmd))  |> to_string |> String.strip |> String.to_integer
    nr_read = :os.cmd(to_char_list(read_cmd))  |> to_string |> String.strip |> String.to_integer
    %{total_messages: (nr_cur + nr_new), new_messages: nr_new, read_messages: nr_read, unread_messages: (nr_cur + nr_new - nr_read)}
  end


  @doc """
  Returns the list of maildir folders.
  The folder list is not cleaned, that is all folders start
  with a "/".
  """
  def folder_list(mailbox_path) do
    # ensure that mailbox_path does not ends with a "/"
    if String.last(mailbox_path) == "/", do: mailbox_path = String.slice(mailbox_path, 0..-2)
    # We use System.cmd because it's much, much faster than Path.wildcard
    case System.cmd("find", [mailbox_path, "-type", "d", "-path", "*/new", "-o", "-path", "*/cur", "-o", "-path", "*/tmp", "-exec", "dirname", "{}", "\;"]) do
      {paths, 0} ->
        paths
          |> String.strip
          |> String.split("\n")
          |> Enum.map(&(String.replace(&1, mailbox_path, ""))) # create relative paths
          |> Enum.map(&(if &1 == "", do: "/", else: &1)) # create relative paths
      {_, 1} -> [] # if no folders, return empty array
    end
  end

end
