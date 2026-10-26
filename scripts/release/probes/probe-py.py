"""Release probe for anvil-py: argv[1] = extracted package dir (contains anvil/)."""
import sys
import os
root = sys.argv[1]
sys.path.insert(0, os.path.join(root, "src") if os.path.isdir(os.path.join(root, "src")) else root)
from anvil import AnvilDocument

print(f"version={AnvilDocument.get_version()}")
doc = AnvilDocument.load_buffer('#!aml\ninclude "missing.anvl";\nx := 1;\n')
if doc is None:
    print("include=<null document>")
else:
    print(f"include={doc.error.message if doc.has_errors and doc.error else '<parsed without error>'}")
doc = AnvilDocument.load_buffer('#!aml\nname := "David";\n')
v = doc.find_value("name") if doc else None
print(f"name={v.as_string() if v is not None else '<none>'}")
