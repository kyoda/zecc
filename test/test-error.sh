#!/bin/sh
set -e

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)

COMPILER="${1:-zecc}"

if [ "$COMPILER" = "zecc" ]; then
  COMPILE_CMD="$ROOT_DIR/zecc -o /tmp/test-error.s -"
elif [ "$COMPILER" = "gcc" ]; then
  COMPILE_CMD="${CC:-gcc} -std=gnu99 -Wall -Wno-switch -xc -c -o /tmp/test-error.o -"
else
  echo "unknown compiler: $COMPILER"
  exit 1
fi

assert_compile_fail() {
  code="$(cat)"

  set +e
  echo "$code" | sh -c "$COMPILE_CMD" 2>/tmp/test-error.log
  status=$?
  set -e

  if [ "$status" -eq 0 ]; then
    echo "FAILED: expected compile error, but compilation succeeded"
    echo "----- compiler -----"
    echo "$COMPILER"
    echo "----- code -----"
    echo "$code"
    exit 1
  fi

  echo "OK: compile error [$COMPILER]"
  echo "----- code -----"
  echo "$code"
  echo "----- error -----"
  cat /tmp/test-error.log
  echo
}

# Invalid C code is embedded in the shell script instead of a .c file
# to avoid IDE/editor error highlighting.

assert_compile_fail <<'EOF'
int main() {
  _Alignof char;
}
EOF

assert_compile_fail <<'EOF'
int add(int a[2]) { return a[0] + a[1]; }
int main() {
  return add({1, 2});
}
EOF

assert_compile_fail <<'EOF'
int main () {
  return ({});
}
EOF

assert_compile_fail <<'EOF'
int main() {
  int a[3]; int b[3]; b = a;
  return;
}
EOF

assert_compile_fail <<'EOF'
int main() {
  void a;
  return;
}
EOF

assert_compile_fail <<'EOF'
int main() {
  return ({return 2;})
}
EOF

assert_compile_fail <<'EOF'
int main() {
  int i = 0; int i = 4; i; 
}
EOF

assert_compile_fail <<'EOF'
int main() {
  int int a; sizeof(a); 
}
EOF

assert_compile_fail <<'EOF'
int no_name(int a) {
  return a;
}
int no_name(int a) {
  return a;
}
EOF

assert_compile_fail <<'EOF'
int g1 = 1;
int g2 = g1 + 1;
EOF

assert_compile_fail <<'EOF'
int g1 = 1;
int g13 = g1 ? 2 : 3;
EOF

assert_compile_fail <<'EOF'
int g13 = 1 ? g1 : 3;
EOF

assert_compile_fail <<'EOF'
struct { struct { char a; char b; } x; } gst5 = { {1, 2} };
char *p = gst5.x + 1;
EOF

assert_compile_fail <<'EOF'
int main() {
  char a[2]; a = {'a', 'b'}; sizeof(a);
}
EOF

assert_compile_fail <<'EOF'
int main() {
  return sizeof(int [][3]);
}
EOF

assert_compile_fail <<'EOF'
int main() {
  typedef int myint; long long myint a;
}
EOF