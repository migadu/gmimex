# Gmimex

**Parses and extracts emails from a maildir**

## Installation

If [available in Hex](https://hex.pm/docs/publish), the package can be installed as:

  1. Add gmimex to your list of dependencies in `mix.exs`:

        def deps do
          [{:gmimex, "~> 0.0.1"}]
        end

  2. Ensure gmimex is started before your application:

        def application do
          [applications: [:gmimex]]
        end

## Tests

    mix test
