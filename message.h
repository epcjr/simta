/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

/*****     message.h     *****/


struct line {
    struct line		*line_next;
    struct line		*line_prev;
    char		*line_data;
};

struct message_data {
    struct line		*md_first;
    struct line		*md_last;
};

struct message {
    struct envelope	*m_env;
    struct message_data	*m_data;
};

struct header {
    char		*h_key;
    struct line		*h_line;
    char		*h_data;
};

#ifdef __STDC__
#define ___P(x)         x
#else /* __STDC__ */
#define ___P(x)         ()
#endif /* __STDC__ */

struct line	*data_add_line ___P(( struct message_data *, char * ));
struct line	*data_prepend_line ___P(( struct message_data *, char * ));
struct message_data	*data_infile ___P(( char *, char * ));
void		data_stdout ___P(( struct message_data * ));

struct message	*message_infiles ___P(( char *, char * ));
void		message_stdout ___P(( struct message * ));
int		message_outfiles ___P(( struct message *, char * ));

/* XXX Bad functions */
struct message	*message_create ___P(( char * ));