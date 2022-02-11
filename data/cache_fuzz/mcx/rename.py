import os

index = 0
for f in os.listdir("."):
    if not f.endswith(".mcx"): continue
    if f.startswith("fuzz_"):
        index = max(index, int(f[5:-4]))

print(index)

for f in os.listdir("."):
    if f.endswith(".py"): continue
    if not f.startswith("fuzz_"):
        index += 1
        name = f"fuzz_{index:04}.mcx"
        os.rename(f, name)
