#ifdef __STDC__
#define ___P(x)		x
#else /* __STDC__ */
#define ___P(x)		()
#endif /* __STDC__ */

#ifdef TLS
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#endif /* TLS */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <netdb.h>
#include <unistd.h>
#include <syslog.h>
#include <sysexits.h>
#include <utime.h>

#include <snet.h>

#include "ll.h"
#include "envelope.h"
#include "queue.h"
#include "message.h"
#include "ml.h"
#include "smtp.h"

void	host_stab_stdout ___P(( void * ));
void	q_file_stab_stdout ___P(( void * ));
int	efile_time_compare ___P(( void *, void * ));
void	deliver_remote ___P(( struct host_q * ));
int	deliver_local ___P(( struct host_q * ));


    int
efile_time_compare( void *a, void *b )
{
    struct q_file		*qa;
    struct q_file		*qb;

    qa = (struct q_file*)a;
    qb = (struct q_file*)b;

    if ( qa->q_etime.tv_sec > qb->q_etime.tv_sec ) {
	return( 1 );
    } else if ( qa->q_etime.tv_sec < qb->q_etime.tv_sec ) {
	return( -1 );
    }

    if ( qa->q_etime.tv_nsec > qb->q_etime.tv_nsec ) {
	return( 1 );
    } else if ( qa->q_etime.tv_nsec < qb->q_etime.tv_nsec ) {
	return( -1 );
    }

    return( 0 );
}


    void
q_file_stab_stdout( void *data )
{
    struct q_file		*q;

    q = (struct q_file*)data;

    q_file_stdout( q );
}


    void
host_stab_stdout( void *data )
{
    struct host_q		*hq;

    hq = (struct host_q*)data;

    host_q_stdout( hq );

    ll_walk( hq->hq_qfiles, q_file_stab_stdout );

    printf( "\n" );
}


    /* 1. For each efile:
     *      -organize by host
     *      -organize under host in reverse chronological order
     *
     * 2. For each host:
     *      -try to send messages
     *      -if there is a failure, stat all the d files to see if a bounce
     *           needs to be generated.
     */

    int
main( int argc, char *argv[] )
{
    DIR				*dirp;
    struct dirent		*entry;
    struct q_file		*q;
    struct envelope		*env;
    struct host_q		*hq;
    struct stab_entry		*host_stab = NULL;
    struct stab_entry		*hs;
    struct stat			sb;
    int				result;
    char			fname[ MAXPATHLEN ];
    char			localhostname[ MAXHOSTNAMELEN ];

    openlog( argv[ 0 ], LOG_NDELAY, LOG_SIMTA );

    if ( gethostname( localhostname, MAXHOSTNAMELEN ) != 0 ) {
	syslog( LOG_ERR, "gethostname: %m" );
	exit( 1 );
    }

    if (( dirp = opendir( SLOW_DIR )) == NULL ) {
	syslog( LOG_ERR, "opendir %s: %m", SLOW_DIR );
	exit( 1 );
    }

    /* clear errno before trying to read */
    errno = 0;

    /* examine a directory */
    while (( entry = readdir( dirp )) != NULL ) {

	/* ignore '.' and '..' */
	if ( entry->d_name[ 0 ] == '.' ) {
	    if ( entry->d_name[ 1 ] == '\0' ) {
		continue;
	    } else if ( entry->d_name[ 1 ] == '.' ) {
		if ( entry->d_name[ 2 ] == '\0' ) {
		    continue;
		}
	    }
	}

	/* organize Efiles by host and modification time */
	if ( *entry->d_name == 'E' ) {
	    if (( env = env_create( entry->d_name + 1 )) == NULL ) {
		syslog( LOG_ERR, "env_create: %m" );
		exit( 1 );
	    }

	    sprintf( fname, "%s/%s", SLOW_DIR, entry->d_name );

	    if (( result = env_infile( env, fname )) < 0 ) {
		/* syserror */
		syslog( LOG_ERR, "env_infile %s: %m", fname );
		exit( 1 );

	    } else if ( result > 1 ) {
		/* syntax error */
		env_free( env );
		/* XXX env_infile should syslog errors */
		syslog( LOG_WARNING, "env_infile %s: syntax error", fname );
		continue;
	    }

	    if (( q = q_file_create( entry->d_name + 1 )) == NULL ) {
		syslog( LOG_ERR, "q_file_create: %m" );
		exit( 1 );
	    }

	    q->q_env = env;
	    q->q_expanded = q->q_env->e_expanded;

	    /* XXX DNS lookup if q->q_expanded == NULL? */

	    /* get efile modification time */
	    if ( stat( fname, &sb ) != 0 ) {
		syslog( LOG_ERR, "stat %s: %m", fname );
		exit( 1 );
	    }

#ifdef sun
	    q->q_etime.tv_sec = sb.st_mtime;
#else	/* sun */
	    q->q_etime = sb.st_mtimespec;
#endif	/* sun */

	    if (( hq = (struct host_q*)ll_lookup( host_stab, q->q_expanded ))
		    == NULL ) {
		if (( hq = host_q_create( q->q_expanded )) == NULL ) {
		    syslog( LOG_ERR, "host_q_create: %m" );
		    exit( 1 );
		}

		if ( ll_insert( &host_stab, hq->hq_name, hq, NULL ) != 0 ) {
		    syslog( LOG_ERR, "ll_insert: %m" );
		    exit( 1 );
		}	

		if ( strcasecmp( localhostname, hq->hq_name ) == 0 ) {
		    hq->hq_local = 1;
		}
	    }

	    if ( ll__insert( &(hq->hq_qfiles), q, efile_time_compare ) != 0 ) {
		syslog( LOG_ERR, "ll__insert: %m" );
		exit( 1 );
	    }
	}
    }

    /* did readdir finish, or encounter an error? */
    if ( errno != 0 ) {
	syslog( LOG_ERR, "readdir: %m" );
	exit( 1 );
    }

#ifdef DEBUG
    ll_walk( host_stab, host_stab_stdout );
#endif /* DEBUG */

    /*
     * 2. For each host:
     *      -try to send messages
     *      -if there is a failure, stat all the d files to see if a bounce
     *           needs to be generated.
     */

    for ( hs = host_stab; hs != NULL; hs = hs->st_next ) {
	hq = (struct host_q*)hs->st_data;

	if (( hq->hq_name == NULL ) || ( *hq->hq_name == '\0' )) {
	    /* XXX NULL host queue.  Add DNS code */

	} else if ( hq->hq_local != 0 ) {
	    deliver_local( hq );

	} else {
	    deliver_remote( hq );
	}
    }

#ifdef DEBUG
    ll_walk( host_stab, host_stab_stdout );
#endif /* DEBUG */

    return( 0 );
}


    int
deliver_local( struct host_q *hq )
{
    struct q_file		*q;
    struct stab_entry		*qs;
    struct recipient		*r;
    int				sent;
    int				mailed;
    int				old_dfile = 0;
    int				fd;
    char			*at;
    char			fname[ MAXPATHLEN ];
    static int			(*local_mailer)(int, char *, char *) = NULL;

    if ( local_mailer == NULL ) {
	if (( local_mailer = get_local_mailer()) == NULL ) {
	    syslog( LOG_ERR, "deliver local: no local mailer!" );
	    exit( 1 );
	}
    }

    for ( qs = hq->hq_qfiles; qs != NULL; qs = qs->st_next ) {
	q = (struct q_file*)qs->st_data;

	/* get message_data */
	errno = 0;
	sprintf( fname, "%s/D%s", SLOW_DIR, q->q_id );

	if (( fd = open( fname, O_RDONLY, 0 )) < 0 ) {
	    if ( errno == ENOENT ) {
		errno = 0;
		syslog( LOG_WARNING, "Missing Dfile: %s", fname );
		q->q_action = Q_REMOVE;
		continue;

	    } else {
		syslog( LOG_ERR, "open %s: %m", fname );
		exit( 1 );
	    }
	}

	sent = 0;
	q->q_env->e_failed = 0;
	q->q_env->e_tempfail = 0;
	q->q_env->e_success = 0;

	/* XXX check for old Dfile at some point */

	for ( r = q->q_env->e_rcpt; r != NULL; r = r->r_next ) {
	    if ( sent != 0 ) {
		if ( lseek( fd, (off_t)0, SEEK_SET ) != 0 ) {
		    syslog( LOG_ERR, "lseek: %m" );
		    exit( 1 );
		}
	    }

	    for ( at = r->r_rcpt; ; at++ ) {
		if ( *at == '@' ) {
		    *at = '\0';
		    break;

		} else if ( *at == '\0' ) {
		    at = NULL;
		    break;
		}
	    }

	    if (( mailed = (*local_mailer)( fd, q->q_env->e_mail,
		    r->r_rcpt )) < 0 ) {
		/* syserror */
		exit( 1 );

	    } else if ( mailed == 0 ) {
		/* success */
		r->r_delivered = R_DELIVERED;
		q->q_env->e_success++;

	    } else if (( mailed == EX_TEMPFAIL ) && ( old_dfile == 0 )) {
		/* retry later */
		r->r_delivered = R_TEMPFAIL;
		q->q_env->e_tempfail++;

	    } else {
		/* hard failure -or- EX_TEMPFAIL and old_dfile */
		r->r_delivered = R_FAILED;
		/* XXX generate bounce */
		q->q_env->e_failed++;
	    }

	    sent++;

	    if ( at != NULL ) {
		*at = '@';
	    }
	}

	if ( close( fd ) != 0 ) {
	    syslog( LOG_ERR, "close: %m" );
	    exit( 1 );
	}

	if ( q->q_env->e_tempfail == 0  ) {
	    /* no retries, only successes and bounces */
	    /* delete Efile then Dfile */
	    sprintf( fname, "%s/E%s", SLOW_DIR, q->q_id );

	    if ( unlink( fname ) != 0 ) {
		syslog( LOG_ERR, "unlink %s: %m", fname );
		exit( 1 );
	    }

	    sprintf( fname, "%s/D%s", SLOW_DIR, q->q_id );

	    if ( unlink( fname ) != 0 ) {
		syslog( LOG_ERR, "unlink %s: %m", fname );
		exit( 1 );
	    }

	    q->q_action = Q_REMOVE;

	} else if (( q->q_env->e_success == 0 ) &&
		( q->q_env->e_failed == 0 )) {
	    /* all addresses are to be retried, no re-writing.  touch Efile */
	    sprintf( fname, "%s/E%s", SLOW_DIR, q->q_id );

	    if ( utime( fname, NULL ) != 0 ) {
		syslog( LOG_ERR, "utime %s: %m", fname );
		exit( 1 );
	    }

	    q->q_action = Q_REORDER;

	} else {
	    /* some retries, and some sent.  re-write envelope */
	    q->q_action = Q_REORDER;
	    env_cleanup( q->q_env );
	    /* XXX env_outfile should syslog errors */
	    if ( env_outfile( q->q_env, SLOW_DIR ) != 0 ) {
		syslog( LOG_ERR, "utime %s: %m", fname );
		exit( 1 );
	    }
	}
    }

    queue_cleanup( hq );

    return( 0 );
}


    void
deliver_remote( struct host_q *hq )
{
    struct q_file		*q;
    struct stab_entry		*qs;
    struct stat			sb;
    int				result;
    int				fd;
    int				sent = 0;
    char			fname[ MAXPATHLEN ];
    SNET			*snet = NULL;
    SNET			*message;
    void                        (*logger)(char *) = NULL;

#ifdef DEBUG
    logger = stdout_logger;
#endif /* DEBUG */

    /* XXX send only to terminator (or alias rsug), for now */
    if (( strcasecmp( hq->hq_name, "terminator.rsug.itd.umich.edu" ) != 0 ) &&
	    ( strcasecmp( hq->hq_name, "rsug.itd.umich.edu" ) != 0 )) {
	return;
    }

    for ( qs = hq->hq_qfiles; qs != NULL; qs = qs->st_next ) {
	q = (struct q_file*)qs->st_data;

	/* get message_data */
	errno = 0;
	sprintf( fname, "%s/D%s", SLOW_DIR, q->q_id );

	if (( fd = open( fname, O_RDONLY, 0 )) < 0 ) {
	    if ( errno == ENOENT ) {
		errno = 0;
		syslog( LOG_WARNING, "Missing Dfile: %s", fname );
		q->q_action = Q_REMOVE;
		continue;

	    } else {
		syslog( LOG_ERR, "open %s: %m", fname );
		exit( 1 );
	    }
	}

	if ( fstat( fd, &sb ) != 0 ) {
	    syslog( LOG_ERR, "snet_attach: %m" );
	    exit( 1 );
	}

	/* XXX old dfile check */

	if (( message = snet_attach( fd, 1024 * 1024 )) == NULL ) {
	    syslog( LOG_ERR, "snet_attach: %m" );
	    exit( 1 );
	}

	if ( sent != 0 ) {
	    if (( result = smtp_rset( snet, logger )) == SMTP_ERR_SYSCALL ) {
		exit( 1 );

	    } else if ( result == SMTP_ERR_SYNTAX ) {
		break;
	    }
	}

	/* open connection, completely ready to send at least one message */
	if ( snet == NULL ) {
	    if (( snet = smtp_connect( hq->hq_name, 25 )) == NULL ) {
		exit( 1 );
	    }

	    if (( result = smtp_helo( snet, logger )) == SMTP_ERR_SYSCALL ) {
		exit( 1 );

	    } else if ( result == SMTP_ERR_SYNTAX ) {
		if ( snet_close( message ) != 0 ) {
		    syslog( LOG_ERR, "close: %m" );
		    exit( 1 );
		}

		return;

	    } else if ( result == SMTP_ERR_MAIL_LOOP ) {
		/* mail loop */
		if ( snet_close( message ) != 0 ) {
		    syslog( LOG_ERR, "close: %m" );
		    exit( 1 );
		}

		syslog( LOG_ALERT, "Hostname %s is not a remote host",
			hq->hq_name );
		return;
	    }
	}

	if (( result = smtp_send( snet, q->q_env, message, logger ))
		== SMTP_ERR_SYSCALL ) {
	    exit( 1 );

	} else if ( result == SMTP_ERR_SYNTAX ) {
	    /* XXX error case? */
	}

	sent++;

	if ( q->q_env->e_failed > 0 ) {
	    /* XXX generate bounce */

	    if ( lseek( fd, (off_t)0, SEEK_SET ) != 0 ) {
		syslog( LOG_ERR, "lseek: %m" );
		exit( 1 );
	    }
	}

	if ( snet_close( message ) != 0 ) {
	    syslog( LOG_ERR, "close: %m" );
	    exit( 1 );
	}

	if ( q->q_env->e_tempfail == 0  ) {
	    /* no retries, only successes and bounces */
	    /* delete Efile then Dfile */
	    sprintf( fname, "%s/E%s", SLOW_DIR, q->q_id );

	    if ( unlink( fname ) != 0 ) {
		syslog( LOG_ERR, "unlink %s: %m", fname );
		exit( 1 );
	    }

	    sprintf( fname, "%s/D%s", SLOW_DIR, q->q_id );

	    if ( unlink( fname ) != 0 ) {
		syslog( LOG_ERR, "unlink %s: %m", fname );
		exit( 1 );
	    }

	    q->q_action = Q_REMOVE;

	} else if (( q->q_env->e_success == 0 ) &&
		( q->q_env->e_failed == 0 )) {
	    /* all addresses are to be retried, no re-writing.  touch Efile */
	    sprintf( fname, "%s/E%s", SLOW_DIR, q->q_id );

	    if ( utime( fname, NULL ) != 0 ) {
		syslog( LOG_ERR, "utime %s: %m", fname );
		exit( 1 );
	    }

	    q->q_action = Q_REORDER;

	} else {
	    /* some retries, and some sent.  re-write envelope */
	    env_cleanup( q->q_env );
	    if ( env_outfile( q->q_env, SLOW_DIR ) != 0 ) {
		exit( 1 );
	    }

	    q->q_action = Q_REORDER;
	}
    }

    if ( snet != NULL ) {
	if (( result = smtp_quit( snet, logger )) < 0 ) {
	    exit( 1 );
	}
    }

    queue_cleanup( hq );

    return;
}
