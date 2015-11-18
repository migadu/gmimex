ExUnit.start()

defmodule GmimexTest.Helpers do
  @doc"""
  As we are moving emails back and forth, we need a way to restore
  the original setup.
  """
  def restore_from_backup(data_path \\ Path.expand("test/data/"), original_path \\ Path.expand("test/data-original/")) do
    File.rm_rf! data_path
    File.cp_r! original_path, data_path
  end
end
