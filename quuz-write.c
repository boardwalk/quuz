#include "quuz.h"
#include <assert.h>
#include <ctype.h>

#ifdef DEBUG_COLLECTOR
static const char* type_name(qz_cell_type_t ct)
{
  switch(ct) {
  case QZ_CT_PAIR:
    return "pair";
  case QZ_CT_FUN:
    return "fun";
  case QZ_CT_PROMISE:
    return "promise";
  case QZ_CT_ERROR:
    return "error";
  case QZ_CT_STRING:
    return "string";
  case QZ_CT_VECTOR:
    return "vector";
  case QZ_CT_BYTEVECTOR:
    return "bytevector";
  case QZ_CT_HASH:
    return "hash";
  case QZ_CT_RECORD:
    return "record";
  case QZ_CT_PORT:
    return "port";
  case QZ_CT_REAL:
    return "real";
  }
  return "unknown";
}

static const char* color_name(qz_cell_color_t cc)
{
  switch(cc) {
  case QZ_CC_BLACK:
    return "black";
  case QZ_CC_GRAY:
    return "gray";
  case QZ_CC_WHITE:
    return "white";
  case QZ_CC_PURPLE:
    return "purple";
  }
  return "unknown";
}

void describe(qz_state_t* st, qz_cell_t* cell)
{
  //qz_write(st, qz_from_cell(cell), 0, stderr);
  fprintf(stderr, "<%p r=%lu t=%s c=%s b=%lu>",
      (void*)cell, qz_refcount(cell),
      type_name(qz_type(cell)), color_name(qz_color(cell)), qz_buffered(cell));
}
#endif

/* quuz-collector.c */
typedef void (*child_func)(qz_state_t* st, qz_cell_t* cell);
void call_if_valid_cell(qz_state_t* st, qz_obj_t obj, child_func func);
void all_children(qz_state_t* st, qz_cell_t* cell, child_func func);

static void clear_dirty(qz_state_t* st, qz_cell_t* cell)
{
  if(qz_dirty(cell)) {
    qz_set_dirty(cell, 0);
    all_children(st, cell, clear_dirty);
  }
}

static void inner_write(qz_state_t* st, qz_obj_t obj, FILE* fp, int human, int* need_space);

static void write_pair(qz_state_t* st, qz_cell_t* cell, FILE* fp, int human, int* need_space, const char* name)
{
  /* TODO How are scheme-defined functions supposed to be written? */
  qz_pair_t* pair = &cell->value.pair;

  if(*need_space) fputc(' ', fp);
  fputc('[', fp);
  fputs(name, fp);
  *need_space = 1;

  inner_write(st, pair->first, fp, human, need_space);
  inner_write(st, pair->rest, fp, human, need_space);

  fputc(']', fp);
  *need_space = 1;
}

static void inner_write_cell(qz_state_t* st, qz_cell_t* cell, FILE* fp, int human, int* need_space)
{
  if(!cell) {
    if(*need_space) fputc(' ', fp);
    fputs("()", fp);
    *need_space = 1;
    return;
  }

  if(qz_dirty(cell)) {
    if(*need_space) fputc(' ', fp);
    fputs("...", fp);
    *need_space = 1;
    return;
  }

  qz_set_dirty(cell, 1);

//#ifdef DEBUG_COLLECTOR
//  describe(cell);
//#endif

  if(qz_type(cell) == QZ_CT_PAIR)
  {
    qz_pair_t* pair = &cell->value.pair;

    if(*need_space) fputc(' ', fp);
    fputc('(', fp);
    *need_space = 0;

    for(;;)
    {
      inner_write(st, pair->first, fp, human, need_space);

      if(qz_is_null(pair->rest))
        break;

      if(!qz_is_pair(pair->rest))
      {
        if(*need_space) fputc(' ', fp);

        fputc('.', fp);
        *need_space = 1;

        inner_write(st, pair->rest, fp, human, need_space);
        break;
      }

      pair = qz_to_pair(pair->rest);
    }

    fputc(')', fp);
    *need_space = 1;
  }
  else if(qz_type(cell) == QZ_CT_FUN)
  {
    write_pair(st, cell, fp, human, need_space, "fun");
  }
  else if(qz_type(cell) == QZ_CT_PROMISE)
  {
    write_pair(st, cell, fp, human, need_space, "promise");
  }
  else if(qz_type(cell) == QZ_CT_ERROR)
  {
    write_pair(st, cell, fp, human, need_space, "error");
  }
  else if(qz_type(cell) == QZ_CT_STRING)
  {
    if(*need_space) fputc(' ', fp);

    if(human) {
      fwrite(QZ_CELL_DATA(cell, char), sizeof(char), cell->value.array.size, fp);
    }
    else {
      fputc('"', fp);
      for(size_t i = 0; i < cell->value.array.size; i++) {
        char c = QZ_CELL_DATA(cell, char)[i];
        if(c == '"')
          fputs("\\\"", fp);
        else if(c == '\\')
          fputs("\\\\", fp);
        else if(isprint(c))
          fputc(c, fp);
        else
          fprintf(fp, "\\x%02x;", c);
      }
      fputc('"', fp);
    }

    *need_space = 1;
  }
  else if(qz_type(cell) == QZ_CT_VECTOR)
  {
    if(*need_space) fputc(' ', fp);

    fputs("#(", fp);
    *need_space = 0;

    for(size_t i = 0; i < cell->value.array.size; i++)
      inner_write(st, QZ_CELL_DATA(cell, qz_obj_t)[i], fp, human, need_space);

    fputc(')', fp);
    *need_space = 1;
  }
  else if(qz_type(cell) == QZ_CT_BYTEVECTOR)
  {
    if(*need_space) fputc(' ', fp);

    fputs("#u8(", fp);
    *need_space = 0;

    for(size_t i = 0; i < cell->value.array.size; i++)
    {
      if(*need_space) fputc(' ', fp);

      fprintf(fp, "#x%02x", QZ_CELL_DATA(cell, uint8_t)[i]);
      *need_space = 1;
    }

    fputc(')', fp);
    *need_space = 1;
  }
  else if(qz_type(cell) == QZ_CT_HASH)
  {
    if(*need_space) fputc(' ', fp);

    fputc('{', fp);
    *need_space = 0;

    int first_pair = 1;
    for(size_t i = 0; i < cell->value.array.capacity; i++)
    {
      qz_pair_t* pair = QZ_CELL_DATA(cell, qz_pair_t) + i;
      if(qz_is_none(pair->first))
        continue;

      if(first_pair) {
        first_pair = 0;
      }
      else {
        fputc(',', fp);
        *need_space = 1;
      }

      inner_write(st, pair->first, fp, human, need_space);

      if(*need_space) fputc(' ', fp);
      fputc('=', fp);
      *need_space = 1;

      inner_write(st, pair->rest, fp, human, need_space);
    }

    fputc('}', fp);
    *need_space = 1;
  }
  else if(qz_type(cell) == QZ_CT_PORT)
  {
    if(*need_space) fputc(' ' , fp);
    fprintf(fp, "[port %d]", fileno(cell->value.fp));
    *need_space = 1;
  }
  else if(qz_type(cell) == QZ_CT_REAL)
  {
    /* TODO make this readable by qz_read() */
    if(*need_space) fputc(' ', fp);

    fprintf(fp, "%f", cell->value.real);
    *need_space = 1;
  }
  else
  {
    assert(0); /* unknown cell type */
  }
}

static void inner_write(qz_state_t* st, qz_obj_t obj, FILE* fp, int human, int* need_space)
{
  if(qz_is_fixnum(obj))
  {
    if(*need_space) fputc(' ', fp);
    fprintf(fp, "%ld", qz_to_fixnum(obj));
    *need_space = 1;
  }
  else if(qz_is_cell(obj))
  {
    inner_write_cell(st, qz_to_cell(obj), fp, human, need_space);
  }
  else if(qz_is_cfun(obj))
  {
    if(*need_space) fputc(' ', fp);
    fprintf(fp, "[cfun %p]", (void*)(size_t)qz_to_cfun(obj));
    *need_space = 1;
  }
  else if(qz_is_sym(obj))
  {
    if(*need_space) fputc(' ', fp);

    // translate identifier to string
    qz_obj_t* name = NULL;
    if(st)
      name = qz_hash_get(st, st->sym_name, obj);

    if(name) {
      /* TODO make this readable by qz_read() */
      qz_cell_t* cell = qz_to_cell(*name);
      fprintf(fp, "%.*s", (int)cell->value.array.size, QZ_CELL_DATA(cell, char));
    }
    else {
      fprintf(fp, "[sym %lx]", qz_to_sym(obj));
    }

    *need_space = 1;
  }
  else if(qz_is_bool(obj))
  {
    if(*need_space) fputc(' ', fp);

    int b = qz_to_bool(obj);
    if(b)
      fputs("#t", fp);
    else
      fputs("#f", fp);

    *need_space = 1;
  }
  else if(qz_is_char(obj))
  {
    if(*need_space) fputc(' ', fp);

    char c = qz_to_char(obj);
    if(human)
      fputc(c, fp);
    else if(isgraph(c))
      fprintf(fp, "#\\%c", c);
    else
      fprintf(fp, "#\\x%02x", c);

    *need_space = 1;
  }
  else if(qz_is_none(obj))
  {
    if(*need_space) fputc(' ', fp);
    fputs("[unknown]", fp);
    *need_space = 1;
  }
  else
  {
    assert(0); /* unknown tagged pointer type */
  }
}

void qz_write(qz_state_t* st, qz_obj_t obj, FILE* fp)
{
  int need_space = 0;
  inner_write(st, obj, fp, 0, &need_space);
  call_if_valid_cell(st, obj, clear_dirty);
}

void qz_display(qz_state_t* st, qz_obj_t obj, FILE* fp)
{
  int need_space = 0;
  inner_write(st, obj, fp, 1, &need_space);
  call_if_valid_cell(st, obj, clear_dirty);
}
