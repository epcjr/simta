#ifdef TLS
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#endif /* TLS */

#include <snet.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>

#include <sys/time.h>		/* struct timeval */
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <ldap.h>
#include <unistd.h>
#include <errno.h>
#include <sysexits.h>
#include <netdb.h>

#include "ll.h"
#include "envelope.h"
#include "address.h"
#include "ldap.h"

#define	SIMTA_LDAP_CONF		"./simta_ldap.conf"

int	ldap_message_stdout ___P(( LDAPMessage *m ));


static char			*attrs[] = { "*", NULL };
struct list			*ldap_searches = NULL;
struct list			*ldap_people = NULL;
struct list			*ldap_groups = NULL;
LDAP				*ld = NULL;


    int
ldap_config( char *fname )
{
    int			lineno = 0;
    int			fd;
    char		*line;
    SNET		*snet;
    char		*c;
    char		*d;
    size_t		len;
    struct list		**l;
    struct list		*l_new;
    struct list		**add;

    /* open fname */
    if (( fd = open( fname, O_RDONLY, 0 )) < 0 ) {
	if ( errno == ENOENT ) {
	    errno = 0;
	    /* XXX file not found, error? */
	    return( 0 );

	} else {
	    fprintf( stderr, "conf_read open %s: ", fname );
	    perror( NULL );
	    return( -1 );
	}
    }

    if (( snet = snet_attach( fd, 1024 * 1024 )) == NULL ) {
	perror( "conf_read snet_attach" );
	return( -1 );
    }

    for ( l = &ldap_searches; *l != NULL; l = &((*l)->l_next))
    	    ;

    while (( line = snet_getline( snet, NULL )) != NULL ) {
	lineno++;

	for ( c = line; ( *c != '\0' ) && ( *c != '#' ); c++ ) {
	    if (( *c != ' ' ) && ( *c != '\t' ) && ( *c != '\n' )) {
		if (( strncasecmp( c, "uri", 3 ) == 0 ) ||
			( strncasecmp( c, "url", 3 ) == 0 )) {

		    c += 3;

		    if ( isspace( *c ) == 0 ) {
			fprintf( stderr, "error 1: %s\n", line );
			break;
		    }

		    for ( c++; isspace( *c ) != 0; c++ );

		    if ( ldap_is_ldap_url( c ) != 0 ) {

			if (( *l = (struct list*)malloc( sizeof( struct list )))
				== NULL ) {
			    perror( "malloc" );
			    exit( 1 );
			}

			if (((*l)->l_string = strdup( c )) == NULL ) {
			    perror( "strdup" );
			    exit( 1 );
			}

			(*l)->l_next = NULL;

			l = &((*l)->l_next);

		    } else {
			fprintf( stderr, "error 2: %s\n", line );
		    }

		} else if (( strncasecmp( c, "oc", 2 ) == 0 ) ||
			( strncasecmp( c, "objectclass", 11 ) == 0 )) {

		    if ( strncasecmp( c, "oc", 2 ) == 0 ) {
			c += 2;
		    } else {
			c += 11;
		    }

		    if ( isspace( *c ) == 0 ) {
			fprintf( stderr, "error 3: %s\n", line );
			break;
		    }

		    for ( c++; isspace( *c ) != 0; c++ );

		    add = NULL;

		    if ( strncasecmp( c, "person", 6 ) == 0 ) {
			c += 6;
			add = &ldap_people;

		    } else if ( strncasecmp( c, "group", 5 ) == 0 ) {
			c += 5;
			add = &ldap_groups;
		    }

		    if ( add != NULL ) {
			if ( isspace( *c ) == 0 ) {
			    fprintf( stderr, "error 4: %s\n", line );
			    break;
			}

			for ( c++; isspace( *c ) != 0; c++ );

			if ( *c == '\0' ) {
			    fprintf( stderr, "error 5: %s\n", line );

			} else {
			    for ( d = c; *d != '\0'; d++ ) {
				if ( isspace( *d ) != 0 ) {
				    break;
				}
			    }

			    len = d - c + 1;

			    while ( *d != '\0' ) {
				if ( isspace( *d ) == 0 ) {
				    break;
				}

				d++;
			    }

			    if ( *d != '\0' ) {
				fprintf( stderr, "error 8: %s\n", line );

			    } else {
				if (( l_new = (struct list*)malloc(
					sizeof( struct list ))) == NULL ) {
				    perror( "malloc" );
				    exit( 1 );
				}
				memset( l_new, 0, sizeof( struct list ));

				if (( l_new->l_string = (char*)malloc( len ))
					== NULL ) {
				    perror( "malloc" );
				    exit( 1 );
				}
				memset( l_new->l_string, 0, len );

				strncpy( l_new->l_string, c, len );

				l_new->l_next = *add;
				*add = l_new;
			    }
			}

		    } else {
			fprintf( stderr, "error 6: %s\n", line );
		    }

		} else {
		    fprintf( stderr, "error 7: %s\n", line );
		}
		break;
	    }
	}
    }

    if ( snet_close( snet ) != 0 ) {
	perror( "nlist snet_close" );
	return( -1 );
    }

    /* XXX check to see that ldap is configured correctly */

    if ( ldap_people == NULL ) {
	fprintf( stderr, "%s: No ldap people\n", fname );
	return( 1 );
    }

    if ( ldap_searches == NULL ) {
	fprintf( stderr, "%s: No ldap searches\n", fname );
	return( 1 );
    }

    printf( "HERE!\n" );

    return( 0 );
}


    int
ldap_value( LDAPMessage *e, char *attr, struct list *master )
{
    int				x;
    char			**values;
    struct list			*l;

    if (( values = ldap_get_values( ld, e, attr )) == NULL ) {
	/* XXX ldaperror? */
	return( -1 );
    }

    for ( x = 0; values[ x ] != NULL; x++ ) {
	for ( l = master ; l != NULL; l = l->l_next ) {
	    if ( strcasecmp( values[ x ], l->l_string ) == 0 ) {
		ldap_value_free( values );
		return( 1 );
	    }
	}
    }

    ldap_value_free( values );

    return( 0 );
}


    int
ldap_expand( char *addr, struct recipient *rcpt, struct stab_entry **expansion,
	struct stab_entry **seen, int *ae_error )
{
    int			x;
    int			whiteout;
    int			result;
    int			count = 0;
    size_t		buf_len = 0;
    size_t		len;
    size_t		place;
    char		*at;
    char		*c;
    char		*d;
    char		*insert;
    char		*domain;
    char		*buf = NULL;
    char		**values;
    LDAPMessage		*res;
    LDAPMessage		*message;
    LDAPMessage		*entry;
    LDAPURLDesc		*lud;
    struct list		*l;
    struct timeval	timeout = {60,0};

    if (( at = strchr( addr, '@' )) == NULL ) {
	/* XXX daemon error handling/reporting */
	printf( "ldap_expand( %s ): bad address\n", addr );
	return( 0 );
    }

    domain = at + 1;

#ifdef DEBUG
    printf( "ldap_expand( %s )\n", addr );
#endif /* DEBUG */

    if ( ld == NULL ) {
	/* XXX static hostname for now */
	if (( ld = ldap_init( "da.dir.itd.umich.edu", 4343 )) == NULL ) {
	    /* XXX daemon error handling/reporting */
	    perror( "ldap_init" );
	    return( -1 );
	}
    }

#ifdef DEBUG
    printf( "ldap_init da.dair.itd.umich.edu success\n" );
#endif /* DEBUG */

    for ( l = ldap_searches; l != NULL; l = l->l_next ) {
	/* make sure buf is big enough search url */
	if (( len = strlen( l->l_string) + 1 ) > buf_len ) {
	    if (( buf = (char*)realloc( buf, len )) == NULL ) {
		/* XXX daemon error handling/reporting */
		return( -1 );
	    }

	    buf_len = len;
	}

	d = buf;
	c = l->l_string;

	while ( *c != '\0' ) {

	    if ( *c != '%' ) {
		/* raw character, copy to data buffer */
		*d = *c;

		/* advance cursors */
		d++;
		c++;

	    } else if ( *( c + 1 ) == '%' ) {
		/* %% -> copy single % to data buffer */
		*d = *c;

		/* advance cursors */
		c += 2;
		d++;

	    } else {
		if (( *( c + 1 ) == 's' ) ||  ( *( c + 1 ) == 'h' )) {
		    /* we currently support %s -> username, %h -> hostname */
		    if ( *( c + 1 ) == 's' ) {
			*at = '\0';
			insert = addr;
			whiteout = 1;

		    } else {
			insert = domain;
			whiteout = 0;
		    }

		    /* if needed, resize buf to handle upcoming insert */
		    if (( len += strlen( insert )) > buf_len ) {
			place = d - buf;

			if (( buf = (char*)realloc( buf, len )) == NULL ) {
			    perror( "malloc" );
			    return( -1 );
			}

			d = buf + place;
			buf_len = len;
		    }

		    /* insert word */
		    while (( *insert != '\0' ) && ( insert != at )) {
			if ((( *insert == '.' ) || ( *insert == '_' ))
				&& ( whiteout != 0 )) {
			    *d = ' ';
			} else {
			    *d = *insert;
			}

			insert++;
			d++;
		    }

		    /* advance read cursor */
		    c += 2;

		} else {
		    /* XXX daemon error handling/reporting */
		    /* XXX unknown/unsupported sequence, copy & warn for now */
		    fprintf( stderr, "unknown sequence: %c\n", *( c + 1 ));
		    *d = *c;
		    c++;
		}
	    }
	}

	*d = '\0';

	if ( ldap_url_parse( buf, &lud ) != 0 ) {
	    /* XXX daemon error handling/reporting */
	    fprintf( stderr, "ldap_url_parse %s:", buf );
	    perror( NULL );
	    return( -1 );
	}

#ifdef DEBUG
    printf( "search base %s, filter %s\n", lud->lud_dn, lud->lud_filter );
#endif /* DEBUG */

	if ( ldap_search_st( ld, lud->lud_dn, lud->lud_scope,
		lud->lud_filter, attrs, 0, &timeout, &res ) != LDAP_SUCCESS ) {
	    /* XXX daemon error handling/reporting */
	    ldap_perror( ld, "ldap_search_st" );
	    return( -1 );
	}

	if (( count = ldap_count_entries( ld, res )) < 0 ) {
	    /* XXX daemon error handling/reporting */
	    ldap_perror( ld, "ldap_count_entries" );
	    goto error;
	}

	if ( count > 0 ) {
	    break;
	}
    }

#ifdef DEBUG
    printf( "%d matches found\n", count );
#endif /* DEBUG */

    if ( count == 0 ) {
	/* XXX daemon error handling/reporting */
	return( 0 );
    }

    if (( entry = ldap_first_entry( ld, res )) == NULL ) {
	/* XXX daemon error handling/reporting */
	ldap_perror( ld, "ldap_first_entry" );
	goto error;
    }

#ifdef DEBUG
    printf( "%d entrie(s)\n", count );
#endif /* DEBUG */

    if (( message = ldap_first_message( ld, res )) == NULL ) {
	/* XXX daemon error handling/reporting */
	ldap_perror( ld, "ldap_first_message" );
	goto error;
    }

    result = 0;

    if ( ldap_groups != NULL ) {
	if (( result = ldap_value( entry, "objectClass", ldap_groups )) < 0 ) {
	    /* XXX daemon error handling/reporting */
	    ldap_perror( ld, "ldap_get_values 1" );
	    goto error;

	} else if ( result > 0 ) {

#ifdef DEBUG
	    printf( "%s IS A GROUP!\n", addr );
#endif /* DEBUG */

	}
    }

    if ( result == 0 ) {
	if (( result = ldap_value( entry, "objectClass", ldap_people )) < 0 ) {
	    /* XXX daemon error handling/reporting */
	    ldap_perror( ld, "ldap_get_values 2" );
	    goto error;

	} else if ( result > 0 ) {

#ifdef DEBUG
	    printf( "%s IS A PERSON!\n", addr );

	    /*
	    if ( ldap_message_stdout( entry ) != 0 ) {
		return( -1 );
	    }
	    */
#endif /* DEBUG */

	    /* get individual's email address(es) */
	    if (( values = ldap_get_values( ld, entry,
		    "mailForwardingAddress" )) == NULL ) {
		/* XXX daemon error handling/reporting */
		ldap_perror( ld, "ldap_get_values 2" );
		goto error;
	    }

	    for ( x = 0; values[ x ] != NULL; x++ ) {
		if ( ll_lookup( *seen, addr ) == NULL ) {
		    /* XXX daemon error handling/reporting */
		    if ( add_address( expansion, values[ x ], rcpt ) != 0 ) {
			perror( "add_address" );
			return( -1 );
		    }

#ifdef DEBUG
		    printf( "mailforwardingaddress added: %s\n", values[ x ]);
#endif /* DEBUG */

		    count++;
		}
	    }

	    ldap_value_free( values );

	} else {
	    /* not a group, or a person */

#ifdef DEBUG
	    printf( "%s IS NOT A PERSON OR A GROUP!\n", addr );
#endif /* DEBUG */

	    /* XXX daemon error handling/reporting */
	    return( -1 );
	}
    }

    /* XXX need to do more than just return */
    ldap_msgfree( res );
    return( count );

error:
    /* XXX daemon error handling/reporting */
    ldap_msgfree( res );
    return( -1 );
}



    int
ldap_message_stdout( LDAPMessage *m )
{
    LDAPMessage		*entry;
    LDAPMessage		*message;
    char		*dn;
    char		*attribute;
    BerElement		*ber;
    char		**values;
    int			i;

    if (( entry = ldap_first_entry( ld, m )) == NULL ) {
	ldap_perror( ld, "ldap_first_entry" );
	return( -1 );
    }

    if (( message = ldap_first_message( ld, m )) == NULL ) {
	ldap_perror( ld, "ldap_first_message" );
	return( -1 );
    }

    if (( dn = ldap_get_dn( ld, message )) == NULL ) {
	ldap_perror( ld, "ldap_get_dn" );
	return( -1 );
    }

    printf( "dn: %s\n", dn );

    /* Print attriubtes and values */
    if (( attribute = ldap_first_attribute( ld, message, &ber )) == NULL ) {
	ldap_perror( ld, "ldap_first_attribute" );
	return( -1 );
    }

    printf( "%s:\n", attribute );

    if (( values = ldap_get_values( ld, entry, attribute )) == NULL ) {
	ldap_perror( ld, "ldap_get_values" );
	return( -1 );
    }

    for ( i = 0; values[ i ] != NULL; i++ ) {
	printf( "	%s\n", values[ i ] );
    }

    ldap_value_free( values );

    while (( attribute = ldap_next_attribute( ld, message, ber )) != NULL ) {
	printf( "%s:\n", attribute );

	if (( values = ldap_get_values( ld, entry, attribute )) == NULL ) {
	    ldap_perror( ld, "ldap_get_values" );
	    return( -1 );
	}

	for ( i = 0; values[ i ] != NULL; i++ ) {
	    printf( "	%s\n", values[ i ] );
	}

	ldap_value_free( values );
    }

    ber_free( ber, 0 );

    return( 0 );
}
