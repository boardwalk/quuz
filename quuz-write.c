#include "quuz.h"
#include <assert.h>
#include <ctype.h>

static void inner_write(qz_state_t* st, qz_obj_t obj, int depth, FILE* fp, int* need_space);

static void inner_write_cell(qz_state_t* st, qz_cell_t* cell, int depth, FILE* fp, int* need_space)
{
  if(!cell) {
    if(*need_space) fputc(' ', fp);
    fputs("[nil]", fp);
    *need_space = 1;
    return;
  }

  if(qz_type(cell) == QZ_CT_PAIR)
  {
    qz_pair_t* pair = &cell->value.pair;

    if(*need_space) fputc(' ', fp);
    fputc('(', fp);
    *need_space = 0;

    if(depth)
    {
      depth--;
      for(;;)
      {
        if(qz_is_nil(pair->first)) /* this shouldn't happen? */
          break;

        inner_write(st, pair->first, depth, fp, need_space);

        if(qz_is_nil(pair->rest))
          break;

        if(!qz_is_pair(pair->rest))
        {
          if(*need_space) fputc(' ', fp);

          fputc('.', fp);
          *need_space = 1;

          inner_write(st, pair->rest, depth, fp, need_space);
          break;
        }

        pair = qz_to_pair(pair->rest);
      }
      depth++;
    }
    else
    {
      fputs("...",  fp);
    }

    fputc(')', fp);
    *need_space = 1;
  }
  else if(qz_type(cell) == QZ_CT_FUN)
  {
    /* TODO How are scheme-defined functions supposed to be written? */
    qz_pair_t* pair = &cell->value.pair;

    if(*need_space) fputc(' ', fp);
    fputs("[fun", fp);
    *need_space = 1;

    if(depth) {
      depth--;
      inner_write(st, pair->first, depth, fp, need_space);
      inner_write(st, pair->rest, depth, fp, need_space);
      depth++;
    }

    fputc(']', fp);
    *need_space = 1;
  }
  else if(qz_type(cell) == QZ_CT_STRING)
  {
    if(*need_space) fputc(' ', fp);

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
    *need_space = 1;
  }
  else if(qz_type(cell) == QZ_CT_VECTOR)
  {
    if(*need_space) fputc(' ', fp);

    fputs("#(", fp);
    *need_space = 0;

    if(depth) {
      depth--;
      for(size_t i = 0; i < cell->value.array.size; i++)
        inner_write(st, QZ_CELL_DATA(cell, qz_obj_t)[i], depth, fp, need_space);
      depth++;
    }
    else {
      fputs("...", fp);
    }

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

    if(depth) {
      depth--;

      int first_pair = 1;
      for(size_t i = 0; i < cell->value.array.capacity; i++)
      {
        qz_pair_t* pair = QZ_CELL_DATA(cell, qz_pair_t) + i;
        if(qz_is_nil(pair->first))
          continue;

        if(first_pair) {
          first_pair = 0;
        }
        else {
          fputc(',', fp);
          *need_space = 1;
        }

        inner_write(st, pair->first, depth, fp, need_space);

        if(*need_space) fputc(' ', fp);
        fputc('=', fp);
        *need_space = 1;

        inner_write(st, pair->rest, depth, fp, need_space);
      }

      depth++;
    }
    else {
      fputs("...", fp);
    }

    fputc('}', fp);
    *need_space = 1;
  }
  else if(qz_type(cell) == QZ_CT_REAL)
  {
    // TODO make this readable by qz_read()
    if(*need_space) fputc(' ', fp);

    fprintf(fp, "%f", cell->value.real);
    *need_space = 1;
  }
  else
  {
    assert(0); /* unknown cell type */
  }
}

static void inner_write(qz_state_t* st, qz_obj_t obj, int depth, FILE* fp, int* need_space)
{
  if(qz_is_fixnum(obj))
  {
    if(*need_space) fputc(' ', fp);
    fprintf(fp, "%ld", qz_to_fixnum(obj));
    *need_space = 1;
  }
  else if(qz_is_cell(obj))
  {
    inner_write_cell(st, qz_to_cell(obj), depth, fp, need_space);
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
    qz_obj_t name = QZ_NIL;
    if(st)
      name = qz_get_hash(st, st->sym_name, obj);

    if(qz_is_string(name)) {
      // TODO make this readable by qz_read()
      qz_cell_t* cell = qz_to_cell(name);
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
    if(isgraph(c))
      fprintf(fp, "#\\%c", c);
    else
      fprintf(fp, "#\\x%02x", c);

    *need_space = 1;
  }
  else
  {
    assert(0); /* unknown tagged pointer type */
  }
}

/* writes a qz_obj_t to a file stream in Scheme form
 * when depth < 0, the entire tree will be printed and readable by qz_read
 * when depth >= 0, the tree will only be printed to that depth */
void qz_write(qz_state_t* st, qz_obj_t o, int depth, FILE* fp)
{
  int need_space = 0;
  inner_write(st, o, depth, fp, &need_space);
}

