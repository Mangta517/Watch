import sys
from pypdf import PdfReader

src = sys.argv[1]
dst = sys.argv[2]
r = PdfReader(src)
with open(dst, "w", encoding="utf-8") as f:
    f.write("PAGES=%d\n" % len(r.pages))
    for i, p in enumerate(r.pages):
        try:
            t = p.extract_text() or ""
        except Exception as e:
            t = "<<extract error: %r>>" % (e,)
        f.write("\n===== PAGE %d =====\n" % (i + 1))
        f.write(t)
print("pages:", len(r.pages))
