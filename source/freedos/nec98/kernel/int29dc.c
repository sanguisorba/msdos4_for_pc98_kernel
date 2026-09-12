/****************************************************************/
/*                                                              */
/*                          int29dc.c                           */
/*                                                              */
/*    NEC PC-98 specific console and miscellaneous functions    */
/*                                                              */
/*    Copyright 2001-2020 FreeDOS(98) porting project           */
/*                                                              */
/****************************************************************/

#include "portab.h"
#include "globals.h"
#include "prot_nec.h"
#include "nls.h"

#if defined(NEC98)
/*
 * int29h
 * Console output for NEC PC-98x1 (normal resolution)
 */
/* for int29 */

#define CHR2(c1,c2)		((c1) | ((c2) << 8))

#define CURSOR_X		(*(UBYTE FAR *)MK_FP(0x0060, 0x011c))
#define CURSOR_Y		(*(UBYTE FAR *)MK_FP(0x0060, 0x0110))
#define KANJI2_WAIT		(*(UBYTE FAR *)MK_FP(0x0060, 0x0115))
#define KANJI1_CODE		(*(UBYTE FAR *)MK_FP(0x0060, 0x0116))
#define PUT_ATTR		(*(UBYTE FAR *)MK_FP(0x0060, 0x011d))
#define CLEAR_CHAR		(*(UBYTE FAR *)MK_FP(0x0060, 0x0119))
#define CLEAR_ATTR		(*(UBYTE FAR *)MK_FP(0x0060, 0x0114))
#define FUNCTION_FLAG	(*(UBYTE FAR *)MK_FP(0x0060, 0x0111))
#define SCROLL_BOTTOM	(*(UBYTE FAR *)MK_FP(0x0060, 0x0112))
#define CURSOR_VIEW	(*(UBYTE FAR *)MK_FP(0x0060, 0x011b))
#define SAVE_CURSOR_X	(*(UBYTE FAR *)MK_FP(0x0060, 0x0127))
#define SAVE_CURSOR_Y	(*(UBYTE FAR *)MK_FP(0x0060, 0x0126))
#define SAVE_PUT_ATTR	(*(UBYTE FAR *)MK_FP(0x0060, 0x012b))

struct progkey {
  UBYTE *table;
  UWORD size;
};

UBYTE int29_esc				= FALSE;
UBYTE int29_esc_buf[128];
UBYTE int29_esc_cnt			= 0;
UBYTE int29_esc_equal		= FALSE;
UBYTE int29_esc_graph		= FALSE;
extern UBYTE FAR ASM programmable_keys;
extern struct progkey FAR ASM programmable_key[];


#if 1
  /* so effective in reducing size of code? (doubtful) */
# define programmable_key_table  nec98_programmable_key_table
#else
STATIC UBYTE FAR *programmable_key_table(unsigned index)
{
  return MK_FP(FP_SEG(&(programmable_key[0])), (UWORD)(programmable_key[index].table));
}
#endif

VOID ASMCFUNC int29_main(UBYTE c);

#if 0
#define iskanji(c)		(((c) >= 0x81 && (c) <= 0x9f) || ((c) >= 0xe0 && (c) <= 0xfc))
#define iskanji2(c)		((c) >= 0x40 && (c) <= 0xfc && (c) != 0x7f)
#else
STATIC int iskanji(unsigned char c)
{
  return (c>=0x81 && c<=0x9f) || (c>=0xe0 && c<=0xfc);
}
STATIC int iskanji2(unsigned char c)
{
  return !((c>=0x07&&c<=0x0d) || c==0x1a || c==0x1b || c==0x1e);
}
#endif


#if 1
# define sjis2jis(c) nec98_sjis2jis_far(c)
#else
STATIC UWORD sjis2jis(UWORD c)
{
  UBYTE h = c >> 8;
  UBYTE l = c;

  if(h <= 0x9f)
  {
    h <<= 1;
    if(l < 0x9f)
      h -= 0xe1;
    else
      h -= 0xe0;
  }
  else
  {
    h <<= 1;
    if(l < 0x9f)
      h -= 0x161;
    else
      h -= 0x160;
  }
  if(l <= 0x7f)
    l -= 0x1f;
  else if(l < 0x9f)
    l -= 0x20;
  else
    l -= 0x7e;

  return ((UWORD)h << 8) | l;
}
#endif

STATIC VOID put_func(UBYTE x, UBYTE y, UBYTE FAR *p)
{
  UBYTE cnt = 6;
  UBYTE clear_char = CLEAR_CHAR;
  UBYTE attr_r = CLEAR_ATTR ^ 4;  /* reverse */

  if(*p == 0xfe)
  {
    put_crt_wattr(x++, y, clear_char, attr_r);
    p++;
    cnt--;
  }

  while(cnt--)
  {
    UBYTE c = *p;

    if(c)
    {
      if(iskanji(c) && cnt < 6 - 1 && iskanji2(*(p + 1)))
      {
        UWORD k;
        p++;
        k = sjis2jis(((UWORD)c << 8) | *p++) - 0x2000;
        k = (k << 8) | (k >> 8);
        put_crt_wattr(x++, y, k, attr_r);
        put_crt_wattr(x++, y, k | 0x80, attr_r);
      }
      else
      {
        put_crt_wattr(x++, y, c, attr_r);
        p++;
      }
    }
    else
      put_crt_wattr(x++, y, clear_char, attr_r);
  }
}

STATIC VOID put_funcs(void)
{
  UBYTE i;
  UBYTE x;
  UBYTE y = get_crt_height();
  UBYTE ofs = (FUNCTION_FLAG == 2) ? 10 : 0;

  for(x = 0; x < 4; x++)
    clear_crt(x, y);
  put_crt_wattr(1, y, peekb(0x60, 0x8b), CLEAR_ATTR);
  put_crt_wattr(2, y, peekb(0x60, 0x8c), CLEAR_ATTR);
  for(i = 1; i <= 5; i++)
  {
    put_func(x, y, programmable_key_table(i + ofs));
    x += 6;
    clear_crt(x++, y);
  }
  for(; x < 4 + 6 * 5 + 4 + 4; x++)
    clear_crt(x, y);
  for(; i <= 10; i++)
  {
    put_func(x, y, programmable_key_table(i + ofs));
    x += 6;
    clear_crt(x++, y);
  }
  for(; x < 80; x++)
    clear_crt(x, y);
}

STATIC VOID clear_funcs(void)
{
  UBYTE x;
  UBYTE y = get_crt_height();
  UBYTE width = get_crt_width();

  for(x = 0; x < width; x++)
    clear_crt(x, y);
}

STATIC VOID show_function(void)
{
  if(FUNCTION_FLAG == 0)
  {
    if(CURSOR_Y >= SCROLL_BOTTOM) {
      crt_scroll_up();
      set_curpos(CURSOR_X, SCROLL_BOTTOM - 1);
    }
    SCROLL_BOTTOM--;
    *(UBYTE FAR *)MK_FP(0x60, 0x8c) = ' ';
    put_funcs();

    FUNCTION_FLAG = 1;
  }
}

STATIC VOID redraw_function(void)
{
  if(FUNCTION_FLAG != 0)
    put_funcs();
}

STATIC VOID hide_function(void)
{
  if(FUNCTION_FLAG != 0)
  {
    clear_funcs();
    SCROLL_BOTTOM++;

    FUNCTION_FLAG = 0;
  }
}

STATIC UBYTE *parse_esc_arg(UBYTE *p)
{
  if(!p || !*p)
    return NULL;

  while(*p)
  {
    if(*p == ';')
    {
      *p++ = '\0';
      return p;
    }
    p++;
  }

  return NULL;
}

STATIC int atoi(UBYTE *p)
{
  int ret = 0;

  if(!p)
    return -1;

  while(*p)
  {
    if(*p < '0' || *p > '9')
      return -1;
    ret *= 10;
    ret += *p - '0';
    p++;
  }

  return ret;
}

STATIC VOID set_curpos_clipped(UBYTE x, UBYTE y, UBYTE offset)
{
  UBYTE columns, rows;

  columns = get_crt_width();
  rows = get_crt_height();
  x = (x >= offset) ? (x - offset) : 0;
  y = (y >= offset) ? (y - offset) : 0;
  if (x >= columns) x = columns - 1;
  if (y >= rows) y = rows - 1;
  set_curpos(x, y);
}

STATIC VOID clear_screen_escj(UBYTE typ, UBYTE cpos_x, UBYTE cpos_y)
{
  UBYTE columns = get_crt_width();
  UBYTE rows = get_crt_height();
  UBYTE x, y;
  switch(typ)
  {
    /* ESC[0J Clear screen from cursor position to the bottom right corner */
    case 0:
      for(x = cpos_x; x < columns; ++x)
        clear_crt(x, cpos_y);
      for(y = cpos_y + 1; y < rows; ++y)
        for(x = 0; x < columns; ++x)
          clear_crt(x, y);
      break;
    
    /* ESC[1J Clear screen from upper left corner to cursor position */
    case 1:
      for(y = 0; y < cpos_y; ++y)
        for(x = 0; x < columns; ++x)
          clear_crt(x, y);
      for(x = 0; x <= cpos_x; ++x)
        clear_crt(x, cpos_y);
      break;
    
    /* ESC[2J Clear the screen, and move cursor to the HOME position */ 
    case 2:
      clear_crt_all();
      set_curpos(0, 0);
      break;
    
    default:
      return;
  }
  redraw_function();
}

STATIC VOID erase_line_esck(UBYTE typ, UBYTE cpos_x, UBYTE cpos_y)
{
  UBYTE columns = get_crt_width();
  UBYTE x;
  
  if (cpos_y >= get_crt_height()) return;
  switch(typ)
  {
    /* ESC[0K erase line from the cursor position to the end-of-line */
    case 0:
      for(x = cpos_x; x < columns; ++x)
        clear_crt(x, cpos_y);
      break;
    
    /* ESC[1K erase line from the beginng-of-line to the cursor position */
    case 1:
      for(x = 0; x <= cpos_x; ++x)
        clear_crt(x, cpos_y);
      break;
    
    /* ESC[2K clear the line where the cursor is placed */
    case 2:
      for(x = 0; x < columns; ++x)
        clear_crt(x, cpos_y);
      break;
  }
}

STATIC VOID move_cursor_up(UBYTE count)
{
  UBYTE y = CURSOR_Y;

  if (count == 0) count = 1;
  y = (y > count) ? (y - count) : 0;
  set_curpos(CURSOR_X, y);
}

STATIC VOID move_cursor_down(UBYTE count)
{
  UWORD rows = (UWORD)(UBYTE)(get_crt_height());
  UWORD y = (UWORD)(UBYTE)(CURSOR_Y);

  if (count == 0) count = 1;
  y += count;
  if (y >= rows) y = rows - 1;
  set_curpos(CURSOR_X, (UBYTE)y);
}

STATIC VOID move_cursor_left(UBYTE count)
{
  UBYTE x = CURSOR_X;

  if (count == 0) count = 1;
  x = (x > count) ? (x - count) : 0;
  set_curpos(x, CURSOR_Y);
}

STATIC VOID move_cursor_right(UBYTE count)
{
  UWORD columns = (UWORD)(UBYTE)(get_crt_width());
  UWORD x = (UWORD)(UBYTE)(CURSOR_X);

  if (count == 0) count = 1;
  x += count;
  if (x >= columns) x = columns - 1;
  set_curpos((UBYTE)x, CURSOR_Y);
}

STATIC VOID roll_screen_up(VOID) /* move cursor down or scroll up (EscD) */
{
  UBYTE rows = get_crt_height();
  UBYTE y = CURSOR_Y;

  y++;
  if (y >= rows)
  {
    crt_scroll_up();
    y = rows - 1;
  }
  set_curpos(CURSOR_X, y);
}

STATIC VOID roll_screen_down(VOID) /* move cursor up or scroll down (EscM) */
{
  UBYTE y = CURSOR_Y;

  if (y == 0)
  {
    crt_rolldown(1);
  }
  else
  {
    --y;
    set_curpos(CURSOR_X, y);
  }
}

STATIC VOID set_crt_lines(UBYTE is_25line)
{
  UBYTE new_rows = is_25line ? 24 : 19;
  
  set_curpos(0, 0);
  crt_set_mode(is_25line ? 0 : 1);
  *(UBYTE FAR *)MK_FP(0x60, 0x113) = !!is_25line;
  SCROLL_BOTTOM = new_rows;
  clear_crt_all();
  if (FUNCTION_FLAG) {
    SCROLL_BOTTOM = --new_rows;
    redraw_function();
  }
  update_cursor_view();
}

STATIC VOID set_graph_state(UBYTE mode_number)
{
  if (mode_number == 0) {
    pokew(0x60, 0x8a, 0x2001);  /* kanji mode */
    redraw_function();
  }
  else if (mode_number == 3) {
    pokew(0x60, 0x8a, 0x6700);  /* graph mode */
    redraw_function();
  }
}

STATIC VOID parse_esc(UBYTE c)
{
  if(int29_esc_cnt < sizeof(int29_esc_buf))
  {
    int29_esc_buf[int29_esc_cnt] = c;
    int29_esc_cnt++;
  }

  if (int29_esc_equal)
  {
    if (int29_esc_cnt == 3)
    {
      UBYTE x, y;
      y = int29_esc_buf[1];
      x = int29_esc_buf[2];
      set_curpos_clipped(x, y, 0x20);
      int29_esc_cnt = 0;
      int29_esc_equal = FALSE;
      int29_esc = FALSE;
    }
    return;
  }
  else if (int29_esc_cnt == 1 && c == '=')
  {
    int29_esc_equal = TRUE;
    return;
  }
  if (int29_esc_graph)
  {
    if (int29_esc_cnt == 2)
    {
      set_graph_state(int29_esc_buf[1] - '0');
      int29_esc_cnt = 0;
      int29_esc_graph = FALSE;
      int29_esc = FALSE;
    }
    return;
  }
  else if (int29_esc_cnt == 1 && c == ')')
  {
    int29_esc_graph = TRUE;
    return;
  }

  if(c >= 'a' &&  c <= 'z'
    || c >= 'A' &&  c <= 'Z'
    || c == '*')
  {
    switch(int29_esc_buf[0])
    {
      case 'D':
        if(int29_esc_cnt == 1)  /* ESCD カーソル下1移動 */
        {
          roll_screen_up();
        }
        break;

      case 'E':
        if(int29_esc_cnt == 1)  /* ESCE カーソル下1左端移動 */
        {
          int29_main(0x0d);	/* CR */
          int29_main(0x0a);	/* LF */
        }
        break;

      case 'M':
        if(int29_esc_cnt == 1)  /* ESCM カーソル上1移動 */
        {
          roll_screen_down();
        }
        break;

      case '*':
        if(int29_esc_cnt == 1)  /* ESC* 画面消去&カーソルをホームへ */
        {
          clear_screen_escj(2, 0, 0);
        }
        break;

#if 0
      case ')':
        if(int29_esc_cnt == 2)
        {
          switch(int29_esc_buf[1])
          {
            case '0':  /* ESC)0 漢字モード設定 */
              break;
            case '3':  /* ESC)3 グラフモード設定 */
              break;
          }
        }
        break;
#endif

      case '[':
        switch(c)
        {
#if 1
          case 's':
            if(int29_esc_cnt == 2)
            {
              SAVE_CURSOR_Y = CURSOR_Y;
              SAVE_CURSOR_X = CURSOR_X;
              SAVE_PUT_ATTR = PUT_ATTR;
            }
            break;
          case 'u':
            if(int29_esc_cnt == 2)
            {
              CURSOR_Y = SAVE_CURSOR_Y;
              CURSOR_X = SAVE_CURSOR_X;
              PUT_ATTR = SAVE_PUT_ATTR;
              set_curpos(CURSOR_X, CURSOR_Y);
            }
            break;
#endif

          case 'h':
            if(int29_esc_cnt == 4)
            {
              switch(*(UWORD *)&int29_esc_buf[1])
              {
                case CHR2('>', '1'):  /* ESC[>1h ファンクション非表示 */
                  hide_function();
                  break;
                case CHR2('>', '3'):  /* ESC[>3h 20行モード設定 */
                  set_crt_lines(FALSE);
                  break;
                case CHR2('>', '5'):  /* ESC[>5h カーソル非表示 */
                  CURSOR_VIEW = 0;
                  update_cursor_view();
                  break;
                case CHR2('?', '5'):  /* ESC[?5h 拡張アトリビュートモード設定(ハイレゾ) */
                  break;
              }
            }
            break;

          case 'l':
            if(int29_esc_cnt == 4)
            {
              switch(*(UWORD *)&int29_esc_buf[1])
              {
                case CHR2('>', '1'):  /* ESC[>1l ファンクション表示 */
                  show_function();
                  break;
                case CHR2('>', '3'):  /* ESC[>3l 25行モード設定 */
                  set_crt_lines(TRUE);
                  break;
                case CHR2('>', '5'):  /* ESC[>5l カーソル表示 */
                  CURSOR_VIEW = 1;
                  update_cursor_view();
                  break;
                case CHR2('?', '5'):  /* ESC[?5l 標準アトリビュートモード設定 */
                  break;
              }
            }
            break;

          case 'm':
            if(int29_esc_cnt >= 2)  /* ESC[<ps>;...;<ps>m 表示属性<ps>設定 */
            {
              CONST STATIC UBYTE palgbr[8] = { 0x00, 0x40, 0x20, 0x60, 0x80, 0xc0, 0xa0, 0xe0 }; /* ESC[17m..23m */
              CONST STATIC UBYTE palbgr[8] = { 0x00, 0x40, 0x80, 0xc0, 0x20, 0x60, 0xa0, 0xe0 }; /* ESC[30m..37m */
              UBYTE clr_attr = CLEAR_ATTR;
              UBYTE new_attr = clr_attr & 0xf0;
              UBYTE *arg;
              UBYTE *next_arg;

              if ((new_attr & 0x10) == 0) new_attr |= 1;
              int29_esc_buf[int29_esc_cnt - 1] = '\0';
#if defined(__WATCOMC__) && (__WATCOMC__ < 1300)   /* to avoid compiler's bug(?) */
              if(int29_esc_cnt < sizeof(int29_esc_buf) - 1)
                int29_esc_buf[int29_esc_cnt] = '\0';
#endif
              for(arg = &int29_esc_buf[1]; arg != NULL; arg = next_arg)
              {
                int attr;

                next_arg = parse_esc_arg(arg);
                attr = atoi(arg);
                if(attr < 0)
                  break;
                if(attr >= 0 && attr <= 16)
                {
                  switch(attr)
                  {
                    case 1:  /* ハイライト (Bold) */
                      new_attr |= 0xe0;
                      break;
                    case 2:  /* バーティカルライン */
                      new_attr |= 0x10;
                      break;
                    case 4:  /* アンダーライン */
                      new_attr |= 0x08;
                      break;
                    case 5:  /* ブリンク */
                      new_attr |= 0x02;
                      break;
                    case 7:  /* リバース */
                      new_attr |= 0x04;
                      break;
                    case 8:  /* シークレット */
                    case 16:
                      new_attr &= 0xfe;
                      break;
                    default:
                      new_attr = clr_attr;
                      break;
                  }
                }
                else if(attr >= 17 && attr <= 23)
                {
                  new_attr = (new_attr & 0x1f) | palgbr[attr & 7];
                }
                else if(attr >= 30 && attr <= 37)
                {
                  new_attr = (new_attr & 0x1f) | palbgr[attr - 30];
                }
                else if(attr >= 40 && attr <= 47)
                {
                  new_attr = (new_attr & 0x1f) | palbgr[attr - 40] | 4;
                }
                else
                {
                  /* fallback */
                  new_attr = clr_attr;
                }
              }
              PUT_ATTR = new_attr;
            }
            break;

          case 'A':
            if(int29_esc_cnt >= 2)  /* ESC[<pn>A カーソル上<pn>移動 */
            {
              int pn = 1;

              if (int29_esc_cnt >= 3)
              {
                int29_esc_buf[int29_esc_cnt - 1] = '\0';
                pn = atoi(&int29_esc_buf[1]);
                if(pn == 0)
                  pn = 1;
              }
              if(pn > 0)
              {
                UBYTE y = CURSOR_Y;
                if(y > 0)
                {
                  if(y > pn)
                    y -= pn;
                  else
                    y = 0;
                  set_curpos(CURSOR_X, y);
                }
              }
            }
            break;

          case 'B':
            if(int29_esc_cnt >= 2)  /* ESC[<pn>B カーソル下<pn>移動 */
            {
              int pn = 1;

              if(int29_esc_cnt >= 3)
              {
                int29_esc_buf[int29_esc_cnt - 1] = '\0';
                pn = atoi(&int29_esc_buf[1]);
                if(pn == 0)
                  pn = 1;
              }
              if(pn > 0)
              {
                UBYTE y		= CURSOR_Y;
                UBYTE max_y	= get_crt_height() - 1;
                if(y < max_y)
                {
                  if(max_y - y > pn)
                    y += pn;
                  else
                    y = max_y;
                  set_curpos(CURSOR_X, y);
                }
              }
            }
            break;

          case 'C':
            if(int29_esc_cnt >= 2)  /* ESC[<pn>C カーソル右<pn>移動 */
            {
              int pn = 1;

              if(int29_esc_cnt >= 3)
              {
                int29_esc_buf[int29_esc_cnt - 1] = '\0';
                pn = atoi(&int29_esc_buf[1]);
                if(pn == 0)
                  pn = 1;
              }
              if(pn > 0)
              {
                UBYTE x		= CURSOR_X;
                UBYTE max_x	= get_crt_width() - 1;
                if(x < max_x)
                {
                  if(max_x - x > pn)
                    x += pn;
                  else
                    x = max_x;
                  set_curpos(x, CURSOR_Y);
                }
              }
            }
            break;

          case 'D':
            if(int29_esc_cnt >= 2)  /* ESC[<pn>D カーソル左<pn>移動 */
            {
              int pn = 1;

              if(int29_esc_cnt >= 3)
              {
                int29_esc_buf[int29_esc_cnt - 1] = '\0';
                pn = atoi(&int29_esc_buf[1]);
                if(pn == 0)
                  pn = 1;
              }
              if(pn > 0)
              {
                UBYTE x = CURSOR_X;
                if(x > 0)
                {
                  if(x > pn)
                    x -= pn;
                  else
                    x = 0;
                  set_curpos(x, CURSOR_Y);
                }
              }
            }
            break;

          case 'f':
          case 'H':
            if(int29_esc_cnt >= 2)  /* ESC[<>;<>H カーソル<><>移動 */
            {
              UBYTE *arg;
              UBYTE *next_arg;
              int x;
              int y;

              int29_esc_buf[int29_esc_cnt - 1] = '\0';
              arg = &int29_esc_buf[1];
              if (int29_esc_cnt == 2)
                set_curpos(0, 0);
              next_arg = parse_esc_arg(arg);
              if(!next_arg)
                break;
              y = atoi(arg);
              arg = next_arg;
              parse_esc_arg(arg);
              x = atoi(arg);
              set_curpos_clipped(x, y, 1);
            }
            break;

          case 'J':
            if(int29_esc_cnt == 2)
            {
              int29_esc_buf[1] = '0';
              int29_esc_cnt = 3;
            }
            if(int29_esc_cnt == 3)
            {
              clear_screen_escj(int29_esc_buf[1] - '0', CURSOR_X, CURSOR_Y);
            }
            break;

#if 1
          case 'n':
            if(int29_esc_cnt == 3)
            {
              switch(int29_esc_buf[1])
              {
                case '6':	/* ESC[6n */
                  nec98_console_esc6n_far();
                  break;
              }
            }
# if 0
            else if(int29_esc_cnt == 4)
            {
              switch(*(UWORD *)&int29_esc_buf[1])
              {
                case CHR2('>', '3'):  /* ESC[>3n 31lines mode (Hi-res) */
                  break;
              }
            }
# endif
            break;
#endif

          case 'K':
            if(int29_esc_cnt == 2)
            {
              int29_esc_buf[1] = '0';
              int29_esc_cnt = 3;
            }
            if(int29_esc_cnt == 3)
            {
              erase_line_esck(int29_esc_buf[1] - '0', CURSOR_X, CURSOR_Y);
            }
            break;

          case 'L':
            if(int29_esc_cnt == 2)
            {
              int29_esc_buf[1] = '0';
              int29_esc_cnt = 3;
            }
            if(int29_esc_cnt >= 3)  /* ESC[<pn>L カーソル上<pn>行クリア */
            {
              int pn;

              int29_esc_buf[int29_esc_cnt - 1] = '\0';
              pn = atoi(&int29_esc_buf[1]);
              if(pn > 0 || (pn == 0 && int29_esc_buf[1] == '0'))
              {
                crt_rolldown(pn);
                set_curpos(0, CURSOR_Y);
              }
            }
            break;

          case 'M':
            if(int29_esc_cnt == 2)
            {
              int29_esc_buf[1] = '0';
              int29_esc_cnt = 3;
            }
            if(int29_esc_cnt >= 3)  /* ESC[<pn>M カーソル下<pn>行クリア */
            {
              int pn;

              int29_esc_buf[int29_esc_cnt - 1] = '\0';
              pn = atoi(&int29_esc_buf[1]);

              if(pn > 0 || (pn == 0 && int29_esc_buf[1] == '0'))
              {
                crt_rollup(pn);
                set_curpos(0, CURSOR_Y);
              }
            }
            break;
        }
        break;
    }

    int29_esc_cnt = 0;
    int29_esc = FALSE;
  }
}

VOID ASMCFUNC int29_main(UBYTE c)
{
  UBYTE width;
  UBYTE height;
  BYTE x;
  BYTE y;

  width = get_crt_width();
  height = get_crt_height();

  if(KANJI2_WAIT && iskanji2(c))
  {
    UWORD k, cc;
    int is_half;

    x = CURSOR_X;
    y = CURSOR_Y;
    cc = ((UWORD)KANJI1_CODE << 8) | c;
    k = sjis2jis(cc) - 0x2000;
    is_half = (cc >= 0x8500 && cc <= 0x869e) || c == 0x1f; /* for compatiblity with NEC MS-DOS' standard console */
    k = (k << 8) | (k >> 8);

    if (x+1 == width && !is_half) { /* wraparound for full width character */
      put_crt(x, y, CLEAR_CHAR);
      x = 0;
      y++;
      if(y >= height)
      {
        y = height - 1;
        crt_scroll_up();
      }
    }
    put_crt(x, y, k);
    x++;
    if(x >= width)
    {
      x = 0;
      y++;
      if(y >= height)
      {
        y = height - 1;
        crt_scroll_up();
      }
    }

    if (!is_half)
    {
      put_crt(x, y, k | 0x80);
      x++;
      if(x >= width)
      {
        x = 0;
        y++;
        if(y >= height)
        {
          y = height - 1;
          crt_scroll_up();
        }
      }
    }

    set_curpos(x, y);
    KANJI2_WAIT = 0;
    KANJI1_CODE = 0;
    return;
  }

  switch(c)
  {
#if 1
    case 0x1b:
      int29_esc = TRUE;
      break;
#endif

    case 0x07:  /* Beep */
      int29_esc = FALSE;
      /* todo */
      break;

    case 0x08:  /* BS (just move left) */
      int29_esc = FALSE;
      x = CURSOR_X;
      y = CURSOR_Y;
      x--;
      if(x < 0)
      {
        x = width - 1;
        y--;
        if(y < 0)
          break;
      }
      set_curpos(x, y);
      break;

    case 0x09:  /* TAB (right cursor by 8 charwidth) */
      int29_esc = FALSE;
      x = CURSOR_X;
      y = CURSOR_Y;
      x = (x & ~7U) + 8;
      if(x >= width)
      {
        x = 0;
        y++;
        if(y >= height)
        {
          y = height - 1;
          crt_scroll_up();
        }
      }
      set_curpos(x, y);
      break;

    case 0x0a:	/* LF */
      int29_esc = FALSE;
      y = CURSOR_Y;
      y++;
      if(y >= height)
      {
        y = height - 1;
        crt_scroll_up();
      }
      set_curpos(CURSOR_X, y);
#if defined(PRINT_STEP)
      /* wait a key on LF */
      {
        _asm {
          push ax
          mov ah, 00h
          int 18h
          pop ax
        }
      }
#endif
      break;

    case 0x0b:  /* up cursor */
      int29_esc = FALSE;
      y = CURSOR_Y;
      if(y > 0)
        set_curpos(CURSOR_X, y - 1);
      break;
    
    case 0x0c:  /* right cursor */
      int29_esc = FALSE;
      x = CURSOR_X;
      y = CURSOR_Y;
      x++;
      if(x >= width)
      {
        x = 0;
        y++;
        if(y >= height)
        {
          y = height - 1;
          crt_scroll_up();
        }
      }
      set_curpos(x, y);
      break;

    case 0x0d:	/* CR */
      int29_esc = FALSE;
      set_curpos(0, CURSOR_Y);
      break;

    case 0x1a:  /* Clear Screen */
      int29_esc = FALSE;
      clear_screen_escj(2, 0, 0);
      break;
    
    case 0x1e:  /* HOME */
      int29_esc = FALSE;
      set_curpos(0, 0);
      break;

    default:
      if(int29_esc)
      {
        parse_esc(c);
        break;
      }

      if(peekb(0x60, 0x8a) && iskanji(c))
      {
        KANJI1_CODE = c;
        KANJI2_WAIT = 1;
        break;
      }

      x = CURSOR_X;
      y = CURSOR_Y;
      put_crt(x, y, c);
      x++;
      if(x >= width)
      {
        x = 0;
        y++;
        if(y >= height)
        {
          y = height - 1;
          crt_scroll_up();
        }
      }
      set_curpos(x, y);
      break;
  }
}


/*
 * int DCh
 * MS-DOS (IO.SYS) extension BIOS
 * (only for NEC PC-98 series and EPSON compatibles)
 */

#define USE_INTDC_SUPSEG60 1

extern UWORD ASM FAR cnvkey_src[];
extern UBYTE ASM FAR cnvkey_dest[][16];

VOID ASMCFUNC intdc_main(iregs FAR *r)
{
  switch(r->CL)
  {
    case 0x09:
      switch(r->AX)
      {
        case 0x0000:		/* Get type of SCSI devices */
        {
#if USE_INTDC_SUPSEG60
          nec98_sup_get_scsi_devices_far(MK_FP(r->DS, r->DX));
#else
          UBYTE equips = *(UBYTE FAR *)MK_FP(0, 0x482);
          UBYTE FAR *p = MK_FP(r->DS, r->DX);
          int i;
          /* todo: prepare 0060:1DBB~1DC2 */
          for(i=0; i<7; ++i)
          {
            if (equips & (1 << i))
            {
              p[i] = 0;
            }
            else
            {
              UBYTE dt = *(UBYTE FAR *)MK_FP(0, 0x460 + i*4);
              p[i] = dt ? dt : 0xff;
            }
          }
          p[7] = 0xff;
#endif
          return;
        }
      }
      break;

    case 0x0c:
      switch(r->AH)
      {
        case 0x00:		/* プログラマブルキー設定内容取得 */
#if 0
          put_string("\nread programmable key=");
          put_unsigned(r->AL, 16, 2);
          put_string("\n");
#endif
          nec98_get_programmable_key(MK_FP(r->DS, r->DX), r->AL);
          return;

        case 0x01:  /* data key buffer functions */
          switch(r->AL)
          {
            case 0x00:  /* int DCh,cl=0Ch,ax=0100h: get contents of data key buffer */
            {
              UBYTE FAR *p = MK_FP(r->DS, r->DX);
              fmemset(p, 0, 512 + 2);
              return;
            }
          }
          break;
      }
      break;

    case 0x0d:
      switch(r->AH)
      {
        case 0x00:  /* プログラマブルキー設定 */
#if 0
          put_string("\nwrite programmable key=");
          put_unsigned(r->AL, 16, 2);
          put_string("\n");
#endif
          nec98_set_programmable_key(MK_FP(r->DS, r->DX), r->AL);
          set_cnvkey_table(r->AL);
          redraw_function();
          return;

        case 0x01:		/* data key buffer functions */
          switch(r->AL)
          {
            case 0x00:  /* int DCh,cl=0Dh,ax=0100h: set contents of data key buffer */
            {
              return; /* just a dummy, for now */
            }
          }
          break;
      }
      break;

    case 0x0e:  /* RS-232C */
      /* DL  bit7~4: port number, bit3~0: command */
      if ((r->DL & 0x0f) == 7) /* optional serial borard (PC-9861K) support */
      {
        r->AX = 0xffff;		/* PC-9861K not exist (not supported yet) */
        return;
      }
      if (r->DL <= 0x0f)		/* (r->DL >> 4) == 0 : on-board RS-232C */
      {
        switch (r->DL & 0x0f)
        {
          case 0:  /* query length of receive buffer */
            r->AX = 0;
            return;
          case 1:  /* initialize the RS-232C port */
            *(UBYTE FAR *)MK_FP(0x60, 0x68) = r->BH;
            *(UBYTE FAR *)MK_FP(0x60, 0x69) = r->BL;
            return;
          case 6:  /* query current configuration of the port */
            r->BH = peekb(0x60, 0x68);
            r->BL = peekb(0x60, 0x69);
            /* seems AX not modified */
            return;
        }
        break;
      }
      break;

    case 0x0f:
      switch(r->AX & 0xfff0)
      {
        case 0x0000:  /* CTL+FUNCのソフトキー化／解除 */
          return;
        case 0x8000:
          switch(r->AL)
          {
            case 0x00:  /* check whether CTL+Func can get from apps */
              r->AX = peekb(0x60, 0x10c) & 1;
              return;
            case 0x02:  /* check whether CTL+XFER/NFER can get from apps */
              r->AX = (peekb(0x60, 0x10c)>>1) & 1;
              return;
          }
          break;
      }
      break;

    case 0x10:
      switch(r->AH)
      {
        case 0x00:  /* 直接コンソール出力 */
          int29_main(r->DL);
          return;
        case 0x01:  /* 直接コンソール出力(文字列) */
          {
            char ch;
            char FAR *ptr = MK_FP(r->DS, r->DX);
            while((ch = *ptr++) != '$')
              int29_main(ch);
            return;
          }
        case 0x02:  /* set attribute */
          PUT_ATTR = r->DL;
          return;
        case 0x03:  /* 直接コンソール出力(カーソル位置) */
          {
            set_curpos_clipped((r->DL + 0x20U)&0xff, (r->DH + 0x20U)&0xff, 0x20);
            return;
          }
        case 0x04:  /* move cursor down or scroll up (EscD) */
          roll_screen_up();
          return;
        case 0x05:  /* move cursor up or scroll down (EscM) */
          roll_screen_down();
          return;
        case 0x06:  /* move cursor up (Esc[nA) */
          move_cursor_up(r->DL);
          return;
        case 0x07:  /* move cursor down (Esc[nB) */
          move_cursor_down(r->DL);
          return;
        case 0x08:  /* move cursor right (Esc[nC) */
          move_cursor_right(r->DL);
          return;
        case 0x09:  /* move cursor left (Esc[nD) */
          move_cursor_left(r->DL);
          return;
        case 0x0a:  /* clear screen */
          clear_screen_escj(r->DL, CURSOR_X, CURSOR_Y);
          return;
        case 0x0b:  /* clear line (Esc[nK) */
          erase_line_esck(r->DL, CURSOR_X, CURSOR_Y);
          return;
        case 0x0c:  /* scroll down text area */
          crt_rolldown(r->DL);
          set_curpos(0, CURSOR_Y);
          return;
        case 0x0d:  /* scroll up text area */
          crt_rollup(r->DL);
          set_curpos(0, CURSOR_Y);
          return;
        case 0x0e:  /* set console mode (kanji/graph) */
          set_graph_state(r->DL);
          return;
      }
      break;

    case 0x11:
      switch(r->AX & 0xfff0)
      {
        case 0x0020:	/* CTL+P/Nの動作指定 */
          return;
      }
      break;

    case 0x12:  /* Get MS-DOS product version and Machine Type */
    {
    /*
    (20200207~0210: tested on NEC MS-DOS 6.2)
     0000:0500 word |0481|0487|0458|result 
      b13 12 11   0 | b6 |byte| b7 |  DX   | description
     ---------------+----+----+----+-------+----------------------
        0  0  0   0 |  0 |    |  0 | 0000h | PC-9801 the original
        1  0  0   0 |  0 |    |  0 | 0001h | PC-9801E/F/M
        1  1  0   1 |  0 |    |  0 | 0002h | PC-9801U
        1  0  0   1 |  0 |    |  0 | 0003h | PC-98x1 normal (old key mode?)
        1  0  0   1 |  1 |    |  0 | 0004h | PC-98x1 normal (new key mode?) [*1]
        1  1  0   0 |    |    |  0 |(0004h)| PC-98LT, PC-98HA, DB-P1
              0     |  1 |    |  0 |(0004h)| PC-98x1 normal (new key mode?) (NEC MS-DOS 5.0+)
              1     |    |    |  0 |(0101h)| (PC-98 hi-res)
        0  0  1   0 |  0 |    |  0 | 0100h | PC-98XA
        0  0  1   0 |  1 |    |  0 | 0101h | PC-98XA? [*2]
        1  0  1   0 |    |    |  0 | 0101h | PC-98 hi-res (except PC-H98, PC-98XA)
              1     |    |    |  0 |(0101h)| (PC-98 hi-res)
              0     |    |!04 |  1 | 1004h | PC-H98 normal (other than i486)
              0     |    | 04 |  1 | 1005h | PC-H98 normal (i486) [*3]
              1     |    |!04 |  1 | 1101h | PC-H98 hi-res (other than i486)
              1     |    | 04 |  1 | 1102h | PC-H98 hi-res (i486) [*4]
    
    (20200208: tested on NEC MS-DOS 3.3D, EPSON MS-DOS 5.0/6.2)
      *1 DX=0003h on EPSON MS-DOS 5.0/6.2
         DX=0003h on NEC MS-DOS 3.3D with [0000:0458] bit6=0
         DX=0004h on NEC MS-DOS 3.3D with [0000:0458] bit6=1 (PC-98GS)
      *2 DX=0100h on NEC MS-DOS 3.3D, EPSON MS-DOS 5.0/6.2
      *3 DX=1004h on EPSON MS-DOS 5.0/6.2
      *4 DX=1101h on EPSON MS-DOS 5.0/6.2
    
    */
#if USE_INTDC_SUPSEG60
      r->DX = nec98_sup_get_machine_type_far();
#else
      UWORD b500 = peekw(0, 0x0500) & 0x3801;
      UBYTE b481 = peekb(0, 0x0481);
      UWORD m = 4;

      if (peekb(0, 0x0458) & 0x80) {  /* PC-H98 */
        m = ((b500 & 0x800) ? 0x1101 : 0x1004) + (peekb(0, 0x0487) == 4);
      }
      else if (b500 & 0x0800) { /* PC-98 hi-res */
        if (b500 == 0x0800 && (b481 & 0x40) == 0)
          m = 0x0100; /* PC-98XA */
        else
          m = 0x0101; /* PC-98 hi-res (except PC-H98, PC-98XA) */
      }
      else {    /* PC-98x1/PC-98GS normal */
        if ((b481 & 0x40) == 0) switch (b500) {
          case 0x0000: m = 0; break;    /* PC-9801 the original */
          case 0x2000: m = 1; break;    /* PC-9801E/F/M */
          case 0x3001: m = 2; break;    /* PC-9801U */
          case 0x2001: m = 3; break;    /* PC-98x1 normal */
        }
        /* fallback: m=4 (PC-98x1/PC-98GS normal) */
      }
      r->DX = m;
#endif
      r->AX = peekw(0x60, 0x20);
      return;
    }

    case 0x13:  /* ドライブ名-DA/UAリスト取得 */
#if USE_INTDC_SUPSEG60
      nec98_sup_get_daua_list_far(MK_FP(r->DS, r->DX));
#else
      {
        UBYTE FAR *ptr = MK_FP(r->DS, r->DX);
        UBYTE FAR *daua = MK_FP(0x60, 0x006c);
        UBYTE FAR *daua2 = MK_FP(0x60, 0x2c86);
        int i;
        for(i = 0; i < 0x60; i++)
          ptr[i] = 0;
        for(i = 0; i < 16; i++)
          ptr[i] = daua[i];
        for(i = 0; i < 27; i++)
        {
          ptr[0x1a + i*2] = daua2[i*2];
          ptr[0x1b + i*2] = daua2[i*2 + 1];
        }
      }
#endif
      return;

    case 0x81:
      switch(r->AX)
      {
        case 0x0000:  /* プロテクトメモリ容量取得 */
          r->AL = *(UBYTE FAR *)MK_FP(0x0060, 0x0031);
          return;

        default:
        {
          /*
            reserve extended memory
            
            AX 0x0001 (other than 0)
            BX extended memory size to allocate (by 128k)
            return:
            AX==0 success
              BX  lowest address (by 64K)
              DX  highest address (by 64K)
            AX==1 not engough memory
              BX  largest avail block (by 128K)
          */
          UWORD mem128k = *(UBYTE FAR *)MK_FP(0, 0x0401);
          UWORD size128k = r->BX;
          if (size128k <= mem128k)
          {
            *(UBYTE FAR *)MK_FP(0, 0x0401) -= (UBYTE)size128k;
            r->BX = (UWORD)*(UBYTE FAR *)MK_FP(0, 0x0401) * 2 + 0x10;
            r->DX = r->BX + size128k * 2;
            r->AX = 0;  /* success */
          }
          else
          {
            r->AX = 1;  /* fail */
            r->BX = mem128k;
          }
          return;
        }
      }
      break;

    case 0x82:
      switch(r->AX)
      {
        case 0x0000:  /* get extended memory size and region */
          r->AX = *(UBYTE FAR *)MK_FP(0, 0x0401);
          r->BX = 0x10;                  /* lowest (by 64K) */
          r->DX = r->BX + (r->AX * 2);  /* highest (by 64K) */
          return;

        default:
          r->AX = 0xffff;
          return;
      }
      break;

    default:
      if (r->CL >= 0xe0 && r->CL <= 0xfd) /* NEC KANJI INPUT FEP API */
      {
        return;
      }
  }

#if 1
  put_string("\nunimplemented internal dos function INTDC cx=");
  put_unsigned(r->CX, 16, 4);
  put_string("h ax=");
  put_unsigned(r->AX, 16, 4);
  put_string("h bx=");
  put_unsigned(r->BX, 16, 4);
  put_string("h dx=");
  put_unsigned(r->DX, 16, 4);
  put_string("h\n");
  put_string("System halted");
  for(;;);
#endif
}

#endif

