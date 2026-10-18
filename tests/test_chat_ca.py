"""Ensure full DER anchors preserve the exact SDK trust set, not expanded trust."""
import hashlib
import importlib.util
from pathlib import Path
import unittest
from cryptography import x509
from cryptography.hazmat.primitives.serialization import Encoding
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('make_chat_ca',ROOT/'tools/make_chat_ca.py')
tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
class RootsTest(unittest.TestCase):
    def test_frozen_sdk_source(self):
        canonical = tool.SOURCE.read_text(encoding='utf-8').encode('utf-8')
        self.assertEqual(hashlib.sha256(canonical).hexdigest(),
                         '50a6277ec69113f00c5fd45f09e8b97a4b3e32daa35d3a95ab30137a55386cef')
    def test_full_certificates_match_subjects_and_are_ca_roots(self):
        entries=tool.roots();self.assertEqual(len(entries),150)
        for subject,der in entries:
            cert=x509.load_der_x509_certificate(der)
            self.assertEqual(cert.subject.public_bytes(),subject)
            self.assertTrue(cert.extensions.get_extension_for_class(x509.BasicConstraints).value.ca)
            self.assertEqual(cert.public_bytes(Encoding.DER),der)
    def test_generated_files_current(self): tool.generate(check=True)
if __name__=='__main__': unittest.main()
