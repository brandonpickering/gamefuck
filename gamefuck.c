#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Utilities */

int parse_hex_addr(char *s, size_t *r) {
  if (strlen(s) > 2*sizeof(size_t)) {
    fprintf(stderr, "Invalid address: %s (too long)\n", s);
    return 0;
  }

  *r = 0;
  for (; *s != '\0'; s++) {
    int d;
    if (*s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
    else if (*s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
    else if (*s >= '0' && *s <= '9') d = *s - '0';
    else {
      fprintf(stderr, "Invalid hex digit: %c\n", *s);
      return 0;
    }
    *r = (*r << 4) + d;
  }
  return 1;
}

size_t hex_len(size_t n) {
  size_t len = 1;
  while ((n /= 16) != 0) len++;
  return len;
}

void fprint_hex(FILE *file, size_t len, size_t n) {
  static char s[2*sizeof(size_t) + 1];
  size_t nlen = hex_len(n);
  size_t i;
  if (nlen > len) len = nlen;

  memset(&s[0], '0', sizeof(s)-1);
  s[sizeof(s)-1] = '\0';

  for (i = 0; i < nlen; i++) {
    size_t d = n % 16;
    s[sizeof(s)-2 - i] = d < 10 ? d + '0' : d - 10 + 'a';
    n /= 16;
  }
  fprintf(file, "%s", s + (sizeof(s)-1 - len));
}

char *read_file(char *filename) {
  FILE *file;
  size_t len;
  char *data;

  file = fopen(filename, "rb");
  if (file == NULL) {
    fprintf(stderr, "Error: failed to open '%s'\n", filename);
    exit(1);
  }

  fseek(file, 0, SEEK_END);
  len = (size_t) ftell(file);
  fseek(file, 0, SEEK_SET);

  data = (char *) malloc(len + 1);
  if (data == NULL) {
    fprintf(stderr, "Error: not enough memory\n");
    fclose(file);
    exit(1);
  }
  fread(data, len, 1, file);
  data[len] = '\0';

  fclose(file);
  return data;
}


/* Memory */

typedef unsigned char byte;

typedef struct {
  size_t size;
  byte *buffer;
} memory;

void mem_init(memory *mem) {
  mem->size = 0;
  mem->buffer = NULL;
}

void mem_destroy(memory *mem) {
  free(mem->buffer);
  mem->size = 0;
  mem->buffer = NULL;
}

void mem_expand(memory *mem, size_t size) {
  size_t new_size;
  if (mem->size >= size) return;
  new_size = 3*mem->size / 2;
  if (new_size < size) new_size = size;

  mem->buffer = (byte *) realloc(mem->buffer, new_size);
  if (mem->buffer == NULL) {
    fprintf(stderr, "Error: not enough memory\n");
    exit(1);
  }
  memset(mem->buffer + mem->size, 0, new_size - mem->size);
  mem->size = new_size;
}

byte *mem_idx(memory *mem, size_t idx) {
  mem_expand(mem, idx+1);
  return mem->buffer + idx;
}

byte mem_get(memory *mem, size_t idx) {
  if (idx >= mem->size) return 0;
  return mem->buffer[idx];
}


/* Preprocessing */

char *get_input(char *source) {
  char *p;
  for (p = source; *p != '\0'; p++) {
    if (*p == '!') break;
  }
  if (*p == '\0') return NULL;
  *p = '\0';
  return p+1;
}

void strip_source(char *source) {
  char *w, *r;
  size_t comm;

  w = source;
  comm = 0;
  for (r = source; *r != '\0'; r++) {
    if (*r == '{') {
      comm++;
      continue;
    }
    if (*r == '}') {
      if (comm == 0) {
        fprintf(stderr, "Error: unmatched }\n");
        exit(1);
      }
      comm--;
      continue;
    }
    if (comm != 0) continue;

    if (isspace(*r)) continue;
    *w++ = *r;
  }
  if (comm != 0) {
    fprintf(stderr, "Error: unmatched {\n");
  }

  *w = '\0';
}

void check_brackets(char *source) {
  size_t bracks;
  char *p;

  bracks = 0;
  for (p = source; *p != '\0'; p++) {
    if (*p == '[') bracks++;
    if (*p == ']') {
      if (bracks == 0) {
        fprintf(stderr, "Error: unmatched ]\n");
        exit(1);
      }
      bracks--;
    }
  }
  if (bracks != 0) {
    fprintf(stderr, "Error: unmatched [\n");
    exit(1);
  }
}


/* Context */

typedef struct {
  char *source;
  char *ip;
  memory mem;
  size_t dp;
  char *input;
} context;

void cxt_init(context *cxt, char *source) {
  cxt->source = source;
  cxt->ip = cxt->source;

  cxt->input = get_input(cxt->source);
  strip_source(cxt->source);
  check_brackets(cxt->source);

  mem_init(&cxt->mem);
  cxt->dp = 0;
}

void cxt_destroy(context *cxt) {
  mem_destroy(&cxt->mem);
}

int cxt_step(context *cxt) {
  int c;

  switch (*cxt->ip) {
    case '+':
      (*mem_idx(&cxt->mem, cxt->dp))++;
      break;
    case '-':
      (*mem_idx(&cxt->mem, cxt->dp))--;
      break;
    case '<':
      if (cxt->dp == 0) {
        fprintf(stderr, "Error: invalid cell access (< past origin)\n");
        exit(1);
      }
      cxt->dp--;
      break;
    case '>':
      cxt->dp++;
      break;
    case '.':
      putchar(*mem_idx(&cxt->mem, cxt->dp));
      fflush(stdout);
      break;
    case ',':
      if (cxt->input == NULL)
        c = feof(stdin) ? EOF : getchar();
      else
        c = *cxt->input == '\0' ? EOF : *cxt->input++;
      *mem_idx(&cxt->mem, cxt->dp) = c == EOF ? 0 : (byte) c;
      break;
    case '[':
      if (*mem_idx(&cxt->mem, cxt->dp) == 0) {
        size_t bracks = 1;
        while (bracks != 0 && *++cxt->ip != '\0') {
          if (*cxt->ip == '[') bracks++;
          if (*cxt->ip == ']') bracks--;
        }
      }
      break;
    case ']':
      if (*mem_idx(&cxt->mem, cxt->dp) != 0) {
        size_t bracks = 1;
        while (bracks != 0 && --cxt->ip >= cxt->source) {
          if (*cxt->ip == '[') bracks--;
          if (*cxt->ip == ']') bracks++;
        }
      }
      break;
    default:
      return 0;
  }

  cxt->ip++;
  return 1;
}


/* Debugger */

void debug_prompt(context *cxt) {
  static const size_t iw = 1, ow = 10;
  int end = 0;
  size_t i0, i1;
  i0 = cxt->ip - cxt->source;
  i0 = i0/iw * iw;
  i0 = i0 >= ow ? i0 - ow : 0;
  i1 = i0 + iw + 2*ow;

  fprintf(stderr, "(");
  for (; i0 < i1; i0++) {
    if (end || cxt->source[i0] == '\0') {
      end = 1;
      fprintf(stderr, " ");
    } else if (cxt->source+i0 == cxt->ip) {
      if (cxt->source[i0] == '[' && *mem_idx(&cxt->mem, cxt->dp) == 0)
        fprintf(stderr, "\x1b[31;1m[\x1b[0m");
      else if (cxt->source[i0] == ']' && *mem_idx(&cxt->mem, cxt->dp) != 0)
        fprintf(stderr, "\x1b[31;1m]\x1b[0m");
      else
        fprintf(stderr, "\x1b[1m%c\x1b[0m", cxt->source[i0]);
    } else {
      fprintf(stderr, "%c", cxt->source[i0]);
    }
  }
  fprintf(stderr, ") ");
  fflush(stderr);
}

char *debug_input(context *cxt) {
  static char prev[128] = "";
  static char input[128];
  static char buf[128];
  char *cmd;

  while (1) {
    debug_prompt(cxt);
    fgets(&input[0], sizeof(input), stdin);

    strcpy(buf, input);
    cmd = strtok(&buf[0], " \t\r\n");
    if (cmd != NULL) {
      strcpy(prev, input);
      return cmd;
    }

    strcpy(buf, prev);
    cmd = strtok(&buf[0], " \t\r\n");
    if (cmd != NULL) return cmd;
  }
}

int debug_match(char *cmd, char *name) {
  size_t i;
  for (i = 0; cmd[i] != '\0'; i++) {
    if (cmd[i] != name[i])
      return 0;
  }
  return 1;
}

char *debug_arg(void) {
  return strtok(NULL, " \t\r\n");
}

int debug_arg_done(void) {
  char *arg = debug_arg();
  if (arg != NULL) {
    fprintf(stderr, "Unexpected arg: %s\n", arg);
    return 0;
  }
  return 1;
}

void debug_mem(context *cxt, char *focus) {
  static const size_t vw = 2, hw = 8;
  size_t foc = cxt->dp;
  size_t focl, foc0, foc1;
  size_t len;

  if (focus != NULL) {
    if (!parse_hex_addr(focus, &foc))
      return;
  }

  focl = foc/hw * hw;
  foc0 = focl >= vw*hw ? focl - vw*hw : 0;
  foc1 = foc0 + 2*vw*hw;
  len = hex_len(foc1);

  for (; foc0 <= foc1; foc0 += hw) {
    size_t f;
    fprint_hex(stderr, len, foc0);

    fprintf(stderr, " |");
    for (f = foc0; f < foc0 + hw; f++) {
      if (f == cxt->dp)
        fprintf(stderr, "\x1b[1m");
      if (focus != NULL && f == foc)
        fprintf(stderr, "\x1b[32m");
      fprintf(stderr, " %02x\x1b[0m", mem_get(&cxt->mem, f));
    }

    fprintf(stderr, " | ");
    for (f = foc0; f < foc0 + hw; f++) {
      char c = mem_get(&cxt->mem, f);
      if (!isprint(c)) c = '.';
      if (f == cxt->dp)
        fprintf(stderr, "\x1b[1m");
      if (focus != NULL && f == foc)
        fprintf(stderr, "\x1b[32m");
      fprintf(stderr, "%c\x1b[0m", c);
    }

    fprintf(stderr, "\n");
  }
}

void debug_mode(context *cxt) {
  while (*cxt->ip != '\0') {
    char *cmd = debug_input(cxt);

    if (debug_match(cmd, "step")) {
      if (debug_arg_done())
        cxt_step(cxt);

    } else if (debug_match(cmd, "quit")) {
      if (debug_arg_done())
        while (*cxt->ip != '\0')
          cxt->ip++;

    } else if (debug_match(cmd, "continue")) {
      if (debug_arg_done())
        return;

    } else if (debug_match(cmd, "memory")) {
      char *arg = debug_arg();
      if (arg == NULL || debug_arg_done())
        debug_mem(cxt, arg);

    } else {
      fprintf(stderr, "Unknown command: %s\n", cmd);
    }
  }
}

int debug_step(context *cxt) {
  switch (*cxt->ip) {
    case 'p':
      fprintf(stderr, "%02x\n", *mem_idx(&cxt->mem, cxt->dp));
      break;

    case '#':
      fprintf(stderr, "\n");
      cxt->ip++;
      debug_mode(cxt);
      return 1;

    default:
      return 0;
  }
  cxt->ip++;
  return 1;
}


/* Gamefuck extension */

#include <GLFW/glfw3.h>

static GLFWwindow *window;
static GLuint texture;
static byte vbuffer[256*256*3];
static byte vp_x, vp_y;
static byte key_states[256];
static byte key_fstates[256];

byte game_key_val(int key) {
  return (byte) (key < 256 ? key : key - 128);
}

void game_key_cb(GLFWwindow *w, int key, int scancode, int action, int mods) {
  (void) w;
  (void) scancode;
  (void) mods;
  if (action == GLFW_REPEAT) return;
  key_states[game_key_val(key)] = action == GLFW_PRESS;
}

void game_init(void) {
  if (!glfwInit()) {
    fprintf(stderr, "Error: failed to initialize GLFW\n");
    exit(1);
  }
  window = glfwCreateWindow(512, 512, "", NULL, NULL);
  if (window == NULL) {
    fprintf(stderr, "Error: failed to open window\n");
    exit(1);
  }
  glfwSetKeyCallback(window, &game_key_cb);

  glfwMakeContextCurrent(window);
  glEnable(GL_TEXTURE_2D);

  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
}

void game_destroy(void) {
  glBindTexture(GL_TEXTURE_2D, 0);
  glDeleteTextures(1, &texture);

  glfwDestroyWindow(window);
  glfwTerminate();
}

void game_display(void) {
  static double rate_time;
  static int num_frames;
  static char title[16];

  static double frame_time;
  int width, height;
  glClear(GL_COLOR_BUFFER_BIT);

  glfwGetFramebufferSize(window, &width, &height);
  glViewport(0, 0, width, height);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_RGB,
      GL_UNSIGNED_BYTE, vbuffer);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glBegin(GL_TRIANGLE_STRIP);
  glTexCoord2f(0, 0);
  glVertex2f(-1, -1);
  glTexCoord2f(1, 0);
  glVertex2f(1, -1);
  glTexCoord2f(0, 1);
  glVertex2f(-1, 1);
  glTexCoord2f(1, 1);
  glVertex2f(1, 1);
  glEnd();

  glfwPollEvents();
  memcpy(key_fstates, key_states, sizeof(key_states));

  while (glfwGetTime() - frame_time < 1/60.0) ;
  frame_time = glfwGetTime();
  glfwSwapBuffers(window);

  num_frames++;
  if (glfwGetTime() - rate_time >= 1) {
    sprintf(&title[0], "%d", num_frames);
    glfwSetWindowTitle(window, &title[0]);
    rate_time = glfwGetTime();
    num_frames = 0;
  }
}

void game_input(context *cxt) {
  byte k;
  for (k = 1; k != 0; k++) {
    if (key_fstates[k]) {
      key_fstates[k] = 0;
      *mem_idx(&cxt->mem, cxt->dp) = k;
      return;
    }
  }
  *mem_idx(&cxt->mem, cxt->dp) = 0;
}

int game_step(context *cxt) {
  char v;

  switch (*cxt->ip) {
    case 'P':
      fprintf(stderr, "%02x %02x\n", vp_x, vp_y);
      break;

    case ':':
      game_display();
      break;

    case ';':
      game_input(cxt);
      break;

    case 'r': vp_x += 1; break;
    case 'l': vp_x -= 1; break;
    case 'u': vp_y += 1; break;
    case 'd': vp_y -= 1; break;

    case '\'':
      v = *mem_idx(&cxt->mem, cxt->dp);
      vbuffer[3*(256*vp_y + vp_x) + 0] = v;
      vbuffer[3*(256*vp_y + vp_x) + 1] = v;
      vbuffer[3*(256*vp_y + vp_x) + 2] = v;
      break;

    default:
      return 0;
  }
  cxt->ip++;
  return 1;
}


/* Main */

void interpret(char *source) {
  context cxt;
  cxt_init(&cxt, source);
  game_init();

  while (*cxt.ip != '\0') {
    if (cxt_step(&cxt)) continue;
    if (debug_step(&cxt)) continue;
    if (game_step(&cxt)) continue;
    cxt.ip++;
  }

  game_destroy();
  cxt_destroy(&cxt);
}

int main(int argc, char **argv) {
  char *source;

  if (argc < 2) {
    fprintf(stderr, "Error: expected source file argument\n");
    return 1;
  }

  source = read_file(argv[1]);

  interpret(source);
  free(source);
  return 0;
}
