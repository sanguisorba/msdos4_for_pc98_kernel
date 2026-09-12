#if !defined(PROTO_NEC_HEADER) && defined(NEC98)
# define PROTO_NEC_HEADER

/* console.asm + conseg60.asm */
# define USE_PUTCRT_SEG60 1

# define ASMCON  ASMCFUNC
# define ASMCON_FAR  FAR ASMCFUNC
# define ASMCONPASCAL_FAR  FAR ASMPASCAL

/* UBYTE  ASMCON crt_set_mode(UBYTE mode); */
VOID  ASMCON set_curpos(UBYTE x, UBYTE y);
/* VOID  ASMCON crt_scroll_up(VOID); */
UBYTE  ASMCON get_crt_width(VOID);
UBYTE  ASMCON get_crt_height(VOID);
VOID  ASMCON put_crt(UBYTE x, UBYTE y, UWORD c);
VOID  ASMCON put_crt_wattr(UBYTE x, UBYTE y, UWORD c, UBYTE a);
VOID  ASMCON clear_crt(UBYTE x, UBYTE y);
/* VOID  ASMCON clear_crt_all(VOID); */
VOID  ASMCON update_cursor_view(VOID);
/* VOID  ASMCON crt_rollup(UBYTE lines); */
/* VOID  ASMCON crt_rolldown(UBYTE lines); */

UBYTE FAR *  ASMPASCAL nec98_programmable_key_table(unsigned index);
VOID  ASMPASCAL nec98_set_cnvkey_table(UBYTE index);
VOID  ASMPASCAL nec98_get_programmable_key(void far *keydata, unsigned keyindex);
VOID  ASMPASCAL nec98_set_programmable_key(const void far *keydata, unsigned keyindex);

VOID  ASMCON_FAR push_cursor_pos_to_conin(VOID);
VOID  ASMCON_FAR nec98_console_esc6n_far(VOID);

UBYTE  ASMCONPASCAL_FAR nec98_crt_set_mode_far(UBYTE mode);
UBYTE  ASMCONPASCAL_FAR nec98_crt_rollup_far(UBYTE linecnt);
UBYTE  ASMCONPASCAL_FAR nec98_crt_rolldown_far(UBYTE linecnt);
VOID  ASMCON_FAR nec98_crt_scroll_up_far(VOID);

VOID  ASMCON_FAR  nec98_clear_crt_all_far(VOID);

VOID  ASMCONPASCAL_FAR nec98_put_crt_far(UBYTE x, UBYTE y, UWORD ccode);
VOID  ASMCONPASCAL_FAR nec98_put_crt_wattr_far(UBYTE x, UBYTE y, UWORD ccode, UWORD attr);
VOID  ASMCONPASCAL_FAR nec98_clear_crt_far(UBYTE x, UBYTE y);
VOID  ASMCONPASCAL_FAR nec98_set_curpos_far(UBYTE x, UBYTE y);
UWORD  ASMCONPASCAL_FAR nec98_show_hide_cursor_far(UBYTE showhide);
VOID   ASMCONPASCAL_FAR nec98_update_cursor_view_far(VOID);

UWORD  ASMCONPASCAL_FAR nec98_sjis2jis_far(UWORD sjis);


/* kernel.asm + supseg60.asm */
# define ASMSUP  ASMCFUNC
# define ASMSUP_FAR  FAR ASMCFUNC
# define ASMSUPPASCAL_FAR  FAR ASMPASCAL

VOID ASMSUPPASCAL_FAR nec98_sup_get_scsi_devices_far(VOID FAR *p);
UWORD ASMSUP_FAR nec98_sup_get_machine_type_far(VOID);
VOID ASMSUPPASCAL_FAR nec98_sup_get_daua_list_far(VOID FAR *p);


#if defined __WATCOMC__
#pragma aux clear_crt modify exact [ax]
#pragma aux get_crt_width modify exact [ax]
#pragma aux get_crt_height modify exact [ax]
#pragma aux put_crt modify exact [ax]
#pragma aux put_crt_wattr modify exact [ax]
#pragma aux set_curpos modify exact [ax dx]
#pragma aux update_cursor_view modify exact [ax dx]

#pragma aux (pascal) nec98_crt_set_mode_far modify exact [ax]
#pragma aux (pascal) nec98_programmable_key_table modify exact [ax dx]
#pragma aux (pascal) nec98_set_cnvkey_table modify exact [ax dx]
#pragma aux (pascal) nec98_get_programmable_key modify exact [ax cx dx]
#pragma aux (pascal) nec98_set_programmable_key modify exact [ax cx dx]

#pragma aux nec98_set_console_esc6n_far modify exact [ax dx]
#pragma aux (pascal) nec98_crt_rollup_far modify exact [ax dx]
#pragma aux (pascal) nec98_crt_rolldown_far modify exact [ax dx]
#pragma aux nec98_crt_scroll_up_far modify exact [ax dx]
#pragma aux nec98_clear_crt_all_far modify exact [ax dx]

#pragma aux (pascal) nec98_put_crt_far modify exact [ax]
#pragma aux (pascal) nec98_put_crt_wattr_far modify exact [ax]
#pragma aux (pascal) nec98_clear_crt_far modify exact [ax]
#pragma aux (pascal) nec98_set_curpos_far modify exact [ax]
#pragma aux (pascal) nec98_show_hide_cursor_far modify exact [ax]
#pragma aux (pascal) nec98_update_cursor_view_far modify exact [ax]

#pragma aux (pascal) nec98_sjis2jis_far modify exact [ax]

#pragma aux (pascal) nec98_sup_get_scsi_devices_far modify exact [ax]
#pragma aux nec98_sup_get_machine_type_far modify exact [ax]
#pragma aux (pascal) nec98_sup_get_daua_list_far modify exact [ax]
#endif


#define set_cnvkey_table nec98_set_cnvkey_table

#define crt_set_mode  nec98_crt_set_mode_far
#define crt_rollup  nec98_crt_rollup_far
#define crt_rolldown  nec98_crt_rolldown_far
#define crt_scroll_up  nec98_crt_scroll_up_far
#define clear_crt_all  nec98_clear_crt_all_far

#ifdef USE_PUTCRT_SEG60
# define put_crt  nec98_put_crt_far
# define put_crt_wattr  nec98_put_crt_wattr_far
# define clear_crt  nec98_clear_crt_far
# define set_curpos nec98_set_curpos_far
# define update_cursor_view  nec98_update_cursor_view_far
#endif


#endif

