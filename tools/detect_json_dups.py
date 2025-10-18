import json
import sys
from collections import defaultdict

path = r"e:\Projects\Clion\RogueLikeGame\CMakePresets.json"

duplicates_found = False

def hook(pairs):
    global duplicates_found
    counts = defaultdict(int)
    for k, v in pairs:
        counts[k] += 1
    for k, c in counts.items():
        if c > 1:
            duplicates_found = True
            print(f"Duplicate key '{k}' found {c} times in one object.")
    # return dict (last wins)
    return {k: v for k, v in pairs}

try:
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()
    json.loads(text, object_pairs_hook=hook)
    if not duplicates_found:
        print("Parsed OK — no duplicate keys inside the same object.")
    else:
        print("Found duplicate keys. See messages above.")
except Exception as e:
    print("JSON parse error:", e)
    sys.exit(1)

