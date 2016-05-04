defmodule Mix.Tasks.Compile.Gmimex do
  @shortdoc "Compiles gmimex library"

  def run(_) do
    {result, _error_code} = System.cmd("make", ["clean", "all"], stderr_to_stdout: true)
    Mix.shell.info result
    Mix.Project.build_structure

    :ok
  end
end


defmodule Mix.Tasks.Clean.Make do
  @shortdoc "Cleans compiled NIF"

  def run(_) do
    {result, _error_code} = System.cmd("make", ['clean'], stderr_to_stdout: true)
    Mix.shell.info result

    :ok
  end
end


defmodule Gmimex.Mixfile do
  use Mix.Project

  @version File.read!("VERSION") |> String.strip

  def project do
    [app: :gmimex,
     version: @version,
     elixir: "~> 1.0",
     compilers: [:gmimex, :elixir, :app],
     name: "gmimex",
     package: package,
     build_embedded: Mix.env == :prod,
     start_permanent: Mix.env == :prod,
     aliases: aliases,
     deps: deps]
  end


  def application, do: []


  defp package do
    [
      maintainers:  ["Migadu GmbH"],
      licenses:     ["MIT"],
      links: %{
        "GitHub" => "https://github.com/dejanstrbac/gmimex",
        "Issues" => "https://github.com/dejanstrbac/gmimex/issues",
        "Docs"   => "http://hexdocs.pm/gmimex/"
      },
      files: [
        "src",
        "lib",
        "priv",
        "Makefile",
        "mix.exs",
        "README.md",
        "VERSION"
      ]
    ]
  end


  defp deps, do: [{:poison, "~> 1.5"}]

  defp aliases, do: [clean: ["clean", "clean.make"]]

end
