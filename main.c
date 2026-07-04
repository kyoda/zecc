#include "zecc.h"

static char *infile;
static char *out_opt;

static void usage(int exit_code) {
  fprintf(stderr, "Usage: zecc [-h|--help] [-o outfile] infile\n");
  exit(exit_code);
}

static void parse_args(int argc, char **argv) {
  for(int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      usage(0);
    }

    if (strcmp(argv[i], "-o") == 0) {
      if (i == argc - 1) {
        fprintf(stderr, "outfile is not specified\n");
        usage(1);
      }
      out_opt = argv[++i];
      continue;
    }

    if (argv[i][0] == '-' && argv[i][1] != '\0') {
      fprintf(stderr, "invalid option: %s\n", argv[i]);
      usage(1);
    }

    infile = argv[i];
  }
}

static char *read_file() {
  FILE *fp;
  if (strcmp(infile, "-") == 0) {
    fp = stdin;
  } else {
    fp = fopen(infile, "r");
    if (!fp) {
      error("cannot open %s: %s", infile, strerror(errno));
    }
  }

  char *buf;
  size_t buflen;
  FILE *out = open_memstream(&buf, &buflen);

  for (;;) {
    char line[4096];
    int n = fread(line, 1, sizeof(line), fp);
    if (n == 0) {
      break;
    }
    fwrite(line, 1, n, out);
  }

  if (fp != stdin) {
    fclose(fp);
  }

  fflush(out);

  // add newline to end of file
  if (buflen == 0 || buf[buflen - 1] != '\n') {
    fputc('\n', out);
  }
  fputc('\0', out);

  fclose(out);

  return buf;
}

static FILE *open_file(char *out_opt) {
  if (!out_opt || strcmp(out_opt, "-") == 0) {
    return stdout;
  }

  FILE *fp = fopen(out_opt, "w");
  if (!fp) {
    error("cannot open %s: %s", out_opt, strerror(errno));
  }

  return fp;
}

int main(int argc, char **argv) {

  parse_args(argc, argv);
  
  Token *token = tokenize(read_file(), infile);
  Obj *prog = parse(token);

  FILE *outfile = open_file(out_opt);
  fprintf(outfile, ".file 1 \"%s\"\n", infile);
  codegen(prog, outfile);

  return 0;
}

