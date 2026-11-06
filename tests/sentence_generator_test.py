"""Offline generator must preserve editorial translations and reject stale input."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "generator", Path(__file__).resolve().parents[1] / "scripts/build_sentence_translations.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class SentenceGeneratorTest(unittest.TestCase):
    def test_preserves_reviewed_text_and_rejects_changed_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "include").mkdir()
            data = root / "examples_ko.json"
            examples = {"pencil": "I write my notes with a pencil."}
            reviewed = {"pencil": {"en": examples["pencil"], "ko": "연필로 적어요."}}
            (root / "examples.json").write_text(json.dumps(examples), encoding="utf-8")
            data.write_text(json.dumps(reviewed), encoding="utf-8")
            with patch.object(generator, "ROOT", root), patch.object(generator, "DATA", data), \
                    patch.object(sys, "argv", ["generator"]), \
                    patch.object(generator, "translate", side_effect=AssertionError("unexpected network")):
                generator.main()
                self.assertEqual(json.loads(data.read_text(encoding="utf-8")), reviewed)
                header = (root / "include/sentence_translations.hpp").read_bytes()
                generator.main()
                self.assertEqual((root / "include/sentence_translations.hpp").read_bytes(), header)
                examples["pencil"] = "A changed English sentence."
                (root / "examples.json").write_text(json.dumps(examples), encoding="utf-8")
                with self.assertRaises(SystemExit):
                    generator.main()
                self.assertEqual(json.loads(data.read_text(encoding="utf-8")), reviewed)


if __name__ == "__main__":
    unittest.main()
