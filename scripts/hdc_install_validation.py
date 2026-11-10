"""Strict install-result guard: HDC may return0 for an explicit device install error."""
import re

def validate_install_output(text,returncode=0):
    if returncode!=0 or re.search(r'\berror\s*:|\bfailed\b|\bfailure\b|\[fail\]',text,re.I):
        raise ValueError('Device installation did not succeed: '+text.strip())
    if re.search(r'\bmsg:\s*install bundle successfully\.',text,re.I) is None:
        raise ValueError('Device installation has no explicit success acknowledgement: '+text.strip())
