defmodule GmimexNif do
  @on_load { :init, 0 }


  def init do
    path = :filename.join(:code.priv_dir(:gmimex), 'gmimex')
    :ok  = :erlang.load_nif(path, 1)
  end


  def get_json(_path, _opts),                      do: exit(:nif_not_loaded)
  def get_part(_path, _part_id),                   do: exit(:nif_not_loaded)
  def index_message(_index_path, _message_path),   do: exit(:nif_not_loaded)
  def index_mailbox(_path),                        do: exit(:nif_not_loaded)
  def search_mailbox(_path, _query, _max_results), do: exit(:nif_not_loaded)

end