# Contributing

This repository is a specification. A change here changes what other people's
clients and servers implement, so please keep it to the point.

## Proposing a change

1. Open an issue describing the packet or behaviour, which end changes, and
   why. A wire change breaks existing implementations; say which ones you
   have updated.
2. Send a pull request with the Markdown change. Keep the table style: one
   table per packet, `Field Name | Field Type | Notes`, sizes in the same
   row as the packet id.
3. If the change affects code, update `examples/c/` (and the Godot copy) and
   run `make test` in `examples/c/`. The golden vectors in the tests are the
   same bytes the documents show.

## Style

* English, present tense, short sentences.
* Every packet says its direction (`Client -> Server`, `Server -> Client` or
  both), its packet id and its total size.
* Do not remove a field or rename one silently: say what replaced it.
* `Uint8`, `Uint16`, `Uint32`, `Int8`, `Float`, `Vec3`, `Color` (B, G, R
  unless stated), `String` (UTF-8 unless stated), `Varint` and `Svarint`, as
  defined in [Protocol 1.0](protocol10.md#data-types) and
  [Protocol 0.75](protocol075.md#data-types).

## Licence

By contributing you agree that the text is licensed under CC BY 4.0 and the
example code under the MIT licence. See [LICENSE](LICENSE).
