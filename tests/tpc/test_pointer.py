"""Native semantic checks for the actual x86 pointer guest."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


@unittest.skipUnless(shutil.which("gcc"), "gcc is required")
class PointerGuestTests(unittest.TestCase):
    def test_checksums_and_symbols(self):
        source = Path(__file__).parent / "pointer.c"
        with tempfile.TemporaryDirectory(prefix="p1-guest-") as folder:
            binary = Path(folder) / "pointer"
            subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-Werror",
                            "-fno-pie", "-no-pie", str(source), "-o", str(binary)], check=True)
            for mode in ("positive", "negative", "zero", "varying", "independent", "split", "forwarded"):
                with self.subTest(mode=mode):
                    result = subprocess.run([str(binary), mode, "257", "3"],
                                            check=True, capture_output=True, text=True)
                    fields = dict(item.split("=", 1) for item in result.stdout.split() if "=" in item)
                    self.assertEqual(fields["checksum"], fields["expected"])
            symbols = subprocess.check_output(["nm", str(binary)], text=True)
            self.assertIn(" p1_producer", symbols)
            self.assertIn(" p1_dependent", symbols)
            self.assertNotEqual(subprocess.run([str(binary), "invalid"], capture_output=True).returncode, 0)


if __name__ == "__main__":
    unittest.main()
