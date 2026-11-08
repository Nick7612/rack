#!/usr/bin/env python3
"""Validate the translation catalogs against the strings in the source.

Fails on malformed JSON, keys that no longer exist in the source, and
placeholder mismatches such as "%1 balls left" translated without %1. Missing
keys are reported as warnings because brand names, keyboard keys, and strings
that are identical in a language are intentionally left out.
"""

import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
CATALOGS = ["da", "de", "es", "fr", "pt-BR"]


def source_strings(path: pathlib.Path) -> set[str]:
    text = path.read_text(encoding="utf-8")
    found: set[str] = set()
    for function in ("qsTr", "tr"):
        pattern = r"\b" + function + r'\(\"((?:[^\"\\]|\\.)*)\"\)'
        for raw in re.findall(pattern, text):
            try:
                found.add(json.loads('"' + raw + '"'))
            except json.JSONDecodeError:
                print(f"{path}: cannot decode {raw!r}", file=sys.stderr)
                sys.exit(1)
    return found


def main() -> int:
    keys: set[str] = set()
    for path in sorted(SRC.glob("*.qml")):
        keys |= source_strings(path)
    for path in sorted(SRC.glob("*.cpp")):
        if path.name != "main.cpp":
            keys |= source_strings(path)

    failed = False
    for code in CATALOGS:
        path = SRC / "i18n" / f"{code}.json"
        try:
            catalog = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            print(f"{path}: {error}", file=sys.stderr)
            failed = True
            continue

        unknown = sorted(set(catalog) - keys)
        missing = sorted(keys - set(catalog))
        for key in unknown:
            print(f"{path}: unknown source string: {key!r}", file=sys.stderr)
            failed = True
        for key, value in catalog.items():
            if not isinstance(value, str):
                print(f"{path}: {key!r} is not a string", file=sys.stderr)
                failed = True
                continue
            expected = sorted(re.findall(r"%\d", key))
            actual = sorted(re.findall(r"%\d", value))
            if expected != actual:
                print(
                    f"{path}: placeholder mismatch: {key!r} -> {value!r}",
                    file=sys.stderr,
                )
                failed = True

        print(f"{code}: {len(catalog)} entries, {len(unknown)} unknown, {len(missing)} untranslated")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
