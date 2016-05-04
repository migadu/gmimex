defmodule GmimexServer do
  use GenServer

  def start_link(opts \\ []) do
    GenServer.start_link(__MODULE__, nil, opts)
  end

  def get_preview_json(server, path) do
    GenServer.call(server, {:get_preview_json, path})
  end

  def get_json(server, path, keep_raw \\ false) do
    GenServer.call(server, {:get_json, path, keep_raw})
  end

  def get_part(server, path, part_id) do
    GenServer.call(server, {:get_part, path, part_id})
  end


  def init(_) do
    {:ok, %{port: start_port, next_id: 1, awaiting: %{}}}
  end


  def handle_call(cmd, _from, state) do
    {id, reply, state} = send_request(state, cmd)
    {:reply, reply, state}
  end

  def handle_call(request, from, state) do
    # Call the default implementation from GenServer
    super(request, from, state)
  end


  def handle_info({port, {:exit_status, status}}, %{port: port}) do
    :erlang.error({:port_exit, status})
  end

  def handle_info(_, state), do: {:noreply, state}


  defp start_port do
    Port.open({:spawn, "priv/port"}, [:binary, {:packet, 4}])
  end

  defp send_request(state, cmd) do
    id = state.next_id
    port = state.port
    send(port, {self, {:command, encode(cmd)}})
    receive do
      {^port, {:data, data}} ->
        {id, decode(data), %{state | next_id: id + 1}}
    end
  end

  def encode({:get_part, path, part_id}) do
    "{ \"exec\": \"get_part\", \"path\": \"#{path}\", \"partId\": #{part_id} }" |> to_char_list
  end

  def encode({:get_preview_json, path}), do:
    "{ \"exec\": \"get_preview_json\", \"path\": \"#{path}\" }" |> to_char_list

  def encode({:get_json, path, keep_raw}), do:
    "{ \"exec\": \"get_json\", \"path\": \"#{path}\", \"raw\": #{keep_raw} }" |> to_char_list

  def decode( <<101, 114, 114, _message :: binary>>), do:
    :error

  def decode(json_response), do:
    {:ok, json_response }


end
