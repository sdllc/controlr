
#ifndef __COMMON_H
#define  __COMMON_H


typedef enum
{
	PARSE2_NULL,
	PARSE2_OK,
	PARSE2_INCOMPLETE,
	PARSE2_ERROR,
	PARSE2_EXTERNAL_ERROR,
	PARSE2_EOF
}
PARSE_STATUS_2;

extern void direct_callback( const char *channel, const char *data, int len );

#endif // #ifndef __COMMON_H
