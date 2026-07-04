#!/bin/sh
set -e

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
CC="$ROOT_DIR/9cc"

assert_compile_fail() {
  code="$(cat)"

  set +e
  echo "$code" | "$CC" -o /tmp/test-error.s - 2>/tmp/test-error.log
  status=$?
  set -e

  if [ "$status" -eq 0 ]; then
    echo "FAILED: expected compile error, but compilation succeeded:"
    echo "$code"
    exit 1
  fi

  echo "OK: compile error"
  echo "----- code -----"
  echo "$code"
  echo "----- error -----"
  cat /tmp/test-error.log
  echo
}

assert_compile_fail "_Alignof char" <<'EOF'
_Alignof char;
EOF

assert_compile_fail "parameter name omitted" <<'EOF'
int no_name2(int) {
  return 0;
}
EOF