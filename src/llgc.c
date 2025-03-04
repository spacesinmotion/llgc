
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int Location;

inline Location l_create(unsigned short line, unsigned short column) {
  return ((Location)column << 16) | (Location)line;
}
unsigned short l_line(Location l) { return (unsigned short)(0xFFFF & l); }
unsigned short l_column(Location l) { return (unsigned short)(l >> 16); }

void test_Location() {
  printf("%s...", __FUNCTION__);

  assert(sizeof(Location) == 4);

  Location l = l_create(0, 0);
  assert(0 == l_line(l) && 0 == l_column(l));
  l = l_create(1, 1);
  assert(1 == l_line(l) && 1 == l_column(l));
  l = l_create(1236, 11231);
  assert(1236 == l_line(l) && 11231 == l_column(l));
  // printf("l %d %d\n", (int)l_line(l), (int)l_column(l));

  printf("%s\n", "ok");
}

typedef enum DataType {
  D_List = 0,
  D_Nil = 1,
  D_Symbol = 3,
  D_LongSymbol = 5,
  D_String = 7,
  D_LongString = 9,
  D_Bool = 11,
  D_Int = 13,
  D_Float = 15,
  D_CData = 17,
  D_CFunc = 19,
} DataType;

void test_DataType() {
  printf("%s...", __FUNCTION__);

  assert(D_List == 0);
  assert((D_Nil & 1) == 1);
  assert((D_Symbol & 1) == 1);
  assert((D_LongSymbol & 1) == 1);
  assert((D_String & 1) == 1);
  assert((D_LongString & 1) == 1);
  assert((D_Bool & 1) == 1);
  assert((D_Int & 1) == 1);
  assert((D_Float & 1) == 1);
  assert((D_CFunc & 1) == 1);

  printf("%s\n", "ok");
}

typedef struct Object Object;
typedef struct Context Context;
typedef Object *(*CFunc)(Context *c, Object *args);

typedef union Data {
  Object *ob;
  CFunc fn;
  void *cd;
  bool b;
  long long i;
  double f;
  char *lt;
  char t[8];
  DataType dt;
} Data;

typedef struct Object {
  Data car, cdr;
  Location location;
  int ref_count;
} Object;

typedef struct Context {
} Context;

static inline Object *ll_malloc(Context *c, DataType dt) {
  Object *o = (Object *)malloc(sizeof(Object));
  o->ref_count = 0;
  o->car.dt = dt;
  return o;
}
static inline DataType ll_type_internal(Object *o) { return !o ? D_Nil : ((o->car.dt & 1) ? o->car.dt : D_List); }
static inline DataType ll_type(Object *o) {
  DataType dt = ll_type_internal(o);
  if (dt == D_LongSymbol)
    return D_Symbol;
  if (dt == D_LongString)
    return D_String;
  return dt;
}

static inline void ll_free(Object *o) {
  if (!o)
    return;
  o->ref_count--;
  if (o->ref_count < 1) {
    if (ll_type_internal(o) == D_LongString || ll_type_internal(o) == D_LongSymbol)
      free(o->cdr.lt);
    if (ll_type_internal(o) == D_List) {
      ll_free(o->car.ob);
      ll_free(o->cdr.ob);
    }
    free(o);
  }
}

Object *ll_assign(Object *l, Object *r) {
  ll_free(l);
  if (r)
    r->ref_count++;
  return r;
}

Object *ll_bool(Context *c, bool v) {
  Object *o = ll_malloc(c, D_Bool);
  o->cdr.b = v;
  return o;
}
bool ll_to_bool(Object *o) {
  assert(ll_type(o) == D_Bool);
  return o->cdr.b;
}
Object *ll_int(Context *c, long long v) {
  Object *o = ll_malloc(c, D_Int);
  o->cdr.i = v;
  return o;
}
long long ll_to_int(Object *o) {
  assert(ll_type(o) == D_Int);
  return o->cdr.i;
}
Object *ll_float(Context *c, double v) {
  Object *o = ll_malloc(c, D_Float);
  o->cdr.f = v;
  return o;
}
double ll_to_float(Object *o) {
  assert(ll_type(o) == D_Float);
  return o->cdr.f;
}
void ll_set_text_(Object *o, const char *b, size_t l) {
  if (l > 7) {
    o->cdr.lt = (char *)malloc(l + 1);
    memcpy(o->cdr.lt, b, l);
    o->cdr.lt[l] = '\0';
  } else {
    o->cdr.ob = NULL;
    memcpy(o->cdr.t, b, l);
    o->cdr.t[l] = '\0';
  }
}
Object *ll_symbol_view(Context *c, const char *b, const char *e) {
  size_t l = e - b;
  Object *o = ll_malloc(c, l > 7 ? D_LongSymbol : D_Symbol);
  ll_set_text_(o, b, l);
  return o;
}
Object *ll_symbol(Context *c, const char *v) { return ll_symbol_view(c, v, v + strlen(v)); }
const char *ll_to_symbol(Object *o) {
  if (ll_type_internal(o) == D_LongSymbol)
    return o->cdr.lt;
  assert(ll_type(o) == D_Symbol);
  return o->cdr.t;
}
Object *ll_string_view(Context *c, const char *b, const char *e) {
  size_t l = e - b;
  Object *o = ll_malloc(c, l > 7 ? D_LongString : D_String);
  ll_set_text_(o, b, l);
  return o;
}
Object *ll_string(Context *c, const char *v) { return ll_string_view(c, v, v + strlen(v)); }
const char *ll_to_string(Object *o) {
  if (ll_type_internal(o) == D_LongString)
    return o->cdr.lt;
  assert(ll_type(o) == D_String);
  return o->cdr.t;
}
Object *ll_cdata(Context *c, void *v) {
  Object *o = ll_malloc(c, D_CData);
  o->cdr.cd = v;
  return o;
}
void *ll_to_cdata(Object *o) {
  assert(ll_type(o) == D_CData);
  return o->cdr.cd;
}
Object *ll_cfunc(Context *c, CFunc v) {
  Object *o = ll_malloc(c, D_CFunc);
  o->cdr.fn = v;
  return o;
}
CFunc ll_to_cfunc(Object *o) {
  assert(ll_type(o) == D_CFunc);
  return o->cdr.fn;
}

Object *ll_cons(Context *c, Object *a, Object *b) {
  Object *o = ll_malloc(c, D_List);
  o->car.ob = ll_assign(NULL, a);
  o->cdr.ob = ll_assign(NULL, b);
  assert(ll_type_internal(o) == D_List);
  return o;
}

Object *ll_list(Context *c, int n, Object **objs) {
  Object *o = NULL;
  for (int i = n - 1; i >= 0; --i)
    o = ll_cons(c, objs[i], o);
  return o;
}

Object *ll_car(Object *o) {
  assert(ll_type(o) == D_List);
  return o->car.ob;
}

Object *ll_cdr(Object *o) {
  assert(ll_type(o) == D_List);
  return o->cdr.ob;
}

Object *ll_next(Object **arg) {
  Object *o = ll_assign(NULL, *arg);
  assert(ll_type_internal(o) == D_List);
  *arg = ll_assign(*arg, ll_cdr(o));
  Object *x = ll_car(o);
  o = ll_assign(o, NULL);
  return x;
}

Object *dummy_cfunc(Context *c, Object *o) {
  (void)c;
  (void)o;
  return NULL;
}

void test_object_atoms() {
  printf("%s...", __FUNCTION__);

  assert(sizeof(Data) == 8);
  assert(sizeof(Object) == 24);

  Context c;

  Object *o = ll_assign(NULL, ll_bool(&c, true));
  assert(o->ref_count == 1);
  assert(ll_type(o) == D_Bool);
  assert(ll_to_bool(o));

  o = ll_assign(o, ll_bool(&c, false));
  assert(o->ref_count == 1);
  assert(ll_type(o) == D_Bool);
  assert(!ll_to_bool(o));

  o = ll_assign(o, ll_int(&c, 42));
  assert(o->ref_count == 1);
  assert(ll_type(o) == D_Int);
  assert(ll_to_int(o) == 42);

  o = ll_assign(o, ll_float(&c, 4.2));
  assert(o->ref_count == 1);
  assert(ll_type(o) == D_Float);
  assert(ll_to_float(o) == 4.2);

  o = ll_assign(o, ll_symbol(&c, "sym"));
  assert(o->ref_count == 1);
  assert(ll_type_internal(o) == D_Symbol);
  assert(ll_type(o) == D_Symbol);
  assert(strcmp(ll_to_symbol(o), "sym") == 0);

  o = ll_assign(o, ll_symbol(&c, "a_quite_long_sym"));
  assert(o->ref_count == 1);
  assert(ll_type_internal(o) == D_LongSymbol);
  assert(ll_type(o) == D_Symbol);
  assert(strcmp(ll_to_symbol(o), "a_quite_long_sym") == 0);

  const char *sv = "sv and other text";
  o = ll_assign(o, ll_symbol_view(&c, sv, sv + 2));
  assert(o->ref_count == 1);
  assert(ll_type_internal(o) == D_Symbol);
  assert(ll_type(o) == D_Symbol);
  assert(strcmp(ll_to_symbol(o), "sv") == 0);

  o = ll_assign(o, ll_string(&c, "a str"));
  assert(o->ref_count == 1);
  assert(ll_type_internal(o) == D_String);
  assert(ll_type(o) == D_String);
  assert(strcmp(ll_to_string(o), "a str") == 0);

  o = ll_assign(o, ll_string(&c, "a longer string"));
  assert(o->ref_count == 1);
  assert(ll_type_internal(o) == D_LongString);
  assert(ll_type(o) == D_String);
  assert(strcmp(ll_to_string(o), "a longer string") == 0);

  int x = 54211;
  o = ll_assign(o, ll_cdata(&c, &x));
  assert(o->ref_count == 1);
  assert(ll_type(o) == D_CData);
  assert(ll_to_cdata(o) && ll_to_cdata(o) == &x);

  o = ll_assign(o, ll_cfunc(&c, dummy_cfunc));
  assert(o->ref_count == 1);
  assert(ll_type(o) == D_CFunc);
  assert(ll_to_cfunc(o) && ll_to_cfunc(o) == dummy_cfunc);

  o = ll_assign(o, NULL);
  assert(!o);

  printf("%s\n", "ok");
}

void test_object_list_creation() {
  printf("%s...", __FUNCTION__);

  Context c;

  Object *b = ll_assign(NULL, ll_bool(&c, true));
  Object *i1 = ll_assign(NULL, ll_int(&c, 42));
  Object *i2 = ll_assign(NULL, ll_int(&c, 21));

  Object *o = ll_assign(NULL, ll_cons(&c, b, i1));
  assert(o->ref_count == 1);
  assert(ll_type(o) == D_List);
  assert(b->ref_count == 2);
  assert(i1->ref_count == 2);

  o = ll_assign(o, NULL);
  assert(!o);
  assert(b->ref_count == 1);
  assert(i1->ref_count == 1);

  o = ll_assign(o, ll_cons(&c, b, ll_cons(&c, i1, ll_cons(&c, i2, NULL))));
  assert(o->ref_count == 1);
  assert(ll_type(o) == D_List);
  assert(b->ref_count == 2);
  assert(i1->ref_count == 2);
  assert(i2->ref_count == 2);

  assert(ll_cdr(o) && ll_cdr(o)->ref_count == 1);
  assert(ll_cdr(ll_cdr(o)) && ll_cdr(ll_cdr(o))->ref_count == 1);

  o = ll_assign(o, NULL);
  assert(!o);
  assert(b->ref_count == 1);
  assert(i1->ref_count == 1);

  o = ll_assign(o, ll_list(&c, 3, (Object *[]){b, i1, i2}));
  assert(o->ref_count == 1);
  assert(ll_cdr(o) && ll_cdr(o)->ref_count == 1);
  assert(ll_cdr(ll_cdr(o)) && ll_cdr(ll_cdr(o))->ref_count == 1);
  assert(ll_type(o) == D_List);
  assert(b->ref_count == 2);
  assert(i1->ref_count == 2);
  assert(i2->ref_count == 2);

  b = ll_assign(b, NULL);
  i1 = ll_assign(i1, NULL);
  i2 = ll_assign(i2, NULL);

  printf("%s\n", "ok");
}

void test_object_list_interaction() {
  printf("%s...", __FUNCTION__);

  Context c;

  Object *b = ll_assign(NULL, ll_bool(&c, true));
  Object *i1 = ll_assign(NULL, ll_int(&c, 42));
  Object *i2 = ll_assign(NULL, ll_int(&c, 21));

  Object *o = ll_assign(NULL, ll_list(&c, 3, (Object *[]){b, i1, i2}));
  assert(o->ref_count == 1);

  assert(b == ll_car(o));
  assert(ll_cdr(o) && ll_type(ll_cdr(o)) == D_List);
  assert(ll_car(ll_cdr(o)) && i1 == ll_car(ll_cdr(o)));
  assert(ll_cdr(ll_cdr(o)) && ll_type(ll_cdr(ll_cdr(o))) == D_List);
  assert(ll_car(ll_cdr(ll_cdr(o))) && ll_car(ll_cdr(ll_cdr(o))) == i2);
  assert(ll_cdr(ll_cdr(ll_cdr(o))) == NULL);
  assert(ll_type(ll_cdr(ll_cdr(ll_cdr(o)))) == D_Nil);

  assert(o->ref_count == 1);
  Object *oo = ll_assign(NULL, o);
  assert(o == oo && oo->ref_count == 2);

  assert(b->ref_count == 2);
  Object *a = ll_assign(NULL, ll_next(&o));
  assert(oo->ref_count == 1);
  assert(a == b);
  assert(i1 == ll_car(o));
  assert(b->ref_count == 3);
  a = ll_assign(a, ll_next(&o));
  assert(b->ref_count == 2);
  assert(a == i1);
  assert(i2 == ll_car(o));
  assert(i1->ref_count == 3);
  a = ll_assign(a, ll_next(&o));
  assert(i1->ref_count == 2);
  assert(a == i2);
  assert(NULL == o);

  oo = ll_assign(oo, NULL);
  assert(b->ref_count == 1);
  assert(i1->ref_count == 1);
  assert(i2->ref_count == 2);
  a = ll_assign(a, NULL);
  assert(i2->ref_count == 1);

  b = ll_assign(b, NULL);
  i1 = ll_assign(i1, NULL);
  i2 = ll_assign(i2, NULL);

  printf("%s\n", "ok");
}

Object *ll_read(Context *c, const char *t, const char **end) {
  Object *o = NULL;

  while (*t && isspace(*t))
    ++t;

  const char *s = t;
  if (*t == '"') {
    ++t;
    while (*t != '"' || *(t - 1) == '\\')
      ++t;
    ++t;
    o = ll_string_view(c, s + 1, t - 1);
  } else {
    while (*t && !isspace(*t))
      ++t;

    if (t - s == 4 && strncmp(s, "true", 4) == 0)
      o = ll_bool(c, true);
    else if (t - s == 5 && strncmp(s, "false", 5) == 0)
      o = ll_bool(c, false);
    else if (t > s) {
      char *end = NULL;
      long long i = strtol(s, &end, 10);
      if (end == t)
        o = ll_int(c, i);
      else {
        double d = strtod(s, &end);
        if (end == t)
          o = ll_float(c, d);
        else
          o = ll_symbol_view(c, s, t);
      }
    }
  }

  if (end)
    *end = t;

  return o;
}

void test_parsing_atoms() {
  printf("%s...", __FUNCTION__);

  Context c;
  Object *o = ll_assign(NULL, ll_read(&c, "", NULL));
  assert(!o);

  o = ll_assign(NULL, ll_read(&c, "sym", NULL));
  assert(o && ll_type(o) == D_Symbol && strcmp(ll_to_symbol(o), "sym") == 0);
  o = ll_assign(NULL, ll_read(&c, " \n xxx  ", NULL));
  assert(o && ll_type(o) == D_Symbol && strcmp(ll_to_symbol(o), "xxx") == 0);
  o = ll_assign(NULL, ll_read(&c, "a_really_long_sym98", NULL));
  assert(o && ll_type(o) == D_Symbol && strcmp(ll_to_symbol(o), "a_really_long_sym98") == 0);

  o = ll_assign(NULL, ll_read(&c, "\"a str\"", NULL));
  assert(o && ll_type(o) == D_String && strcmp(ll_to_string(o), "a str") == 0);
  o = ll_assign(NULL, ll_read(&c, "\"a long string with escaped \\\" str\"", NULL));
  assert(o && ll_type(o) == D_String && strcmp(ll_to_string(o), "a long string with escaped \\\" str") == 0);

  o = ll_assign(NULL, ll_read(&c, "true", NULL));
  assert(o && ll_type(o) == D_Bool && ll_to_bool(o));
  o = ll_assign(NULL, ll_read(&c, " false ", NULL));
  assert(o && ll_type(o) == D_Bool && !ll_to_bool(o));

  o = ll_assign(NULL, ll_read(&c, "\t 523 ", NULL));
  assert(o && ll_type(o) == D_Int && ll_to_int(o) == 523);
  o = ll_assign(NULL, ll_read(&c, "\r -8635 ", NULL));
  assert(o && ll_type(o) == D_Int && ll_to_int(o) == -8635);

  o = ll_assign(NULL, ll_read(&c, "\r 4.25 ", NULL));
  assert(o && ll_type(o) == D_Float && ll_to_float(o) == 4.25);
  o = ll_assign(NULL, ll_read(&c, "\r -6.75e2 ", NULL));
  assert(o && ll_type(o) == D_Float && ll_to_float(o) == -6.75e2);

  printf("%s\n", "ok");
}

int main(int argc, char *argv[]) {
  printf("(hi %s)\n", "llgc");

  test_Location();
  test_DataType();
  test_object_atoms();
  test_object_list_creation();
  test_object_list_interaction();
  test_parsing_atoms();

  printf("%s\n", "ok");
  return 0;
}