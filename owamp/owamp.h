/*
**      $Id$
*/
/************************************************************************
*									*
*			     Copyright (C)  2002			*
*				Internet2				*
*			     All Rights Reserved			*
*									*
************************************************************************/
/*
**	File:		owamp.h
**
**	Author:		Jeff W. Boote
**			Anatoly Karp
**
**	Date:		Wed Mar 20 11:10:33  2002
**
**	Description:	
**	This header file describes the owamp API. The owamp API is intended
**	to provide a portable layer for implementing the owamp protocol.
*/
#ifndef	OWAMP_H
#define	OWAMP_H

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>

#ifndef	False
#define	False	(0)
#endif
#ifndef	True
#define	True	(!False)
#endif

#include <owamp/arithm64.h>

#define	OWP_MODE_UNDEFINED		(0)
#define	OWP_MODE_OPEN			(01)
#define	OWP_MODE_AUTHENTICATED		(02)
#define	OWP_MODE_ENCRYPTED		(04)

/* Default mode offered by the server */
#define OWP_DEFAULT_OFFERED_MODE 	(OWP_MODE_OPEN|OWP_MODE_AUTHENTICATED|OWP_MODE_ENCRYPTED)

/*
 * The 5555 should eventually be replaced by a IANA blessed service name.
 */
#define OWP_CONTROL_SERVICE_NAME	"5555"

/*
 * Default value to use for the listen backlog. We pick something large
 * and let the OS truncate it if it isn't willing to do that much.
 */
#define OWP_LISTEN_BACKLOG	(64)

/*
 * These structures are opaque to the API user.
 * They are used to maintain state internal to the library.
 */
typedef struct OWPContextRec	*OWPContext;
typedef struct OWPControlRec	*OWPControl;
typedef struct OWPAddrRec	*OWPAddr;

/* Codes for returning error severity and type. */
typedef enum {
	OWPErrFATAL=-4,
	OWPErrWARNING=-3,
	OWPErrINFO=-2,
	OWPErrDEBUG=-1,
	OWPErrOK=0
} OWPErrSeverity;

typedef enum {
	OWPErrPOLICY,
	OWPErrINVALID,
	OWPErrUNSUPPORTED,
	OWPErrUNKNOWN
} OWPErrType;

/*
 * Valid values for "accept" - this will be added to for the purpose of
 * enumerating the reasons for rejecting a session, or early termination
 * of a test session.
 *
 * TODO:Get the additional "accept" values added to the spec.
 */
typedef enum{
	OWP_CNTRL_INVALID=-1,
	OWP_CNTRL_ACCEPT=0x0,
	OWP_CNTRL_REJECT=0x1,
	OWP_CNTRL_FAILURE=0x2,
	OWP_CNTRL_UNSUPPORTED=0x4
} OWPAcceptType;

typedef u_int32_t	OWPBoolean;
typedef u_int8_t	OWPSID[16];
typedef u_int8_t	OWPSequence[4];
typedef u_int8_t	OWPKey[16];
typedef u_int32_t	OWPSessionMode;


typedef struct OWPTimeStampRec{
	u_int32_t		sec;
	u_int32_t		frac_sec;
	u_int8_t		sync;
	u_int8_t		prec;
} OWPTimeStamp;

typedef enum {
	OWPTestUnspecified,	/* invalid value	*/
	OWPTestPoisson
} OWPTestType;

typedef struct{
	OWPTestType	test_type;
	OWPTimeStamp	start_time;
	u_int32_t	npackets;
	u_int32_t	typeP;
	u_int32_t	packet_size_padding;
	u_int32_t	InvLambda;
} OWPTestSpecPoisson;

typedef struct{
	OWPTestType	test_type;
	OWPTimeStamp	start_time;
	u_int32_t	npackets;
	u_int32_t	typeP;
	u_int32_t	packet_size_padding;
} OWPTestSpecAny;

typedef union _OWPTestSpec{
	OWPTestType		test_type;
	OWPTestSpecAny		any;
	OWPTestSpecPoisson	poisson;
	u_int32_t		padding[10]; /* bigger than any test... */
} OWPTestSpec;

/*
 * The following types are used to initialize the library.
 */
/*
 * This type is used to define the function that is called whenever an error
 * is encountered within the library.
 * This function should return 0 on success and non-0 on failure. If it returns
 * failure - the default library error function will be called.
 */
typedef int (*OWPErrFunc)(
	void		*err_data,
	OWPErrSeverity	severity,
	OWPErrType	etype,
	const char	*errmsg
);

/*
 * This type is used to define the function that retrieves the shared
 * secret from whatever key-store is in use.
 * It should return True if it is able to fill in the key_ret variable that
 * is passed in from the caller. False if not. If the function returns false,
 * the caller will check the err_ret value. If OK, then the kid simply didn't
 * exist - otherwise it indicates an error in the key store mechanism.
 */	
typedef OWPBoolean	(*OWPGetAESKeyFunc)(
	void		*app_data,
	const char	*kid,
	u_int8_t	*key_ret,
	OWPErrSeverity	*err_ret
);

/*
 * This function will be called from OWPControlOpen and OWPServerAccept
 * to determine if the control connection should be accepted.
 * It is called after connecting, and after determining the kid.
 * On failure, value of *err_ret can be inspected: if > OWPErrWARNING,
 * this means rejection based on policy, otherwise there was an error
 * in the function itself.
 */
typedef OWPBoolean (*OWPCheckControlPolicyFunc)(
	void		*app_data,
	OWPSessionMode	mode_req,
	const char	*kid,
	struct sockaddr	*local_sa_addr,
	struct sockaddr	*remote_sa_addr,
	OWPErrSeverity	*err_ret
);

/*
 * This function will be called by OWPRequestTestSession if
 * one of the endpoints of the test is on the localhost before
 * it calls the EndpointInit*Func's. If err_ret returns
 * OWPErrFATAL, OWPRequestTestSession will not continue, and return
 * OWPErrFATAL as well.
 *
 * endpoint->sid will not be valid yet.
 * Only the IP address values will be set in the sockaddr structures -
 * i.e. port numbers will not be valid.
 */
typedef OWPBoolean (*OWPCheckTestPolicyFunc)(
	void		*app_data,
	OWPSessionMode	mode,
	const char	*kid,
	OWPBoolean	local_sender,
	struct sockaddr	*local_sa_addr,
	struct sockaddr	*remote_sa_addr,
	OWPTestSpec	*test_spec,
	OWPErrSeverity	*err_ret
);

/*
 * The endpoint_handle data returned from this function is used by the
 * application to keep track of a particular endpoint of an OWAMP test.
 * It is opaque from the point of view of the library.
 * (It is simply a more specific app_data.)
 *
 * This function needs to allocate port and return it in the localsaddr
 * structure.
 * If "recv" - (send == False) then also allocate and return sid.
 */
typedef OWPErrSeverity (*OWPEndpointInitFunc)(
	void		*app_data,
	void		**end_data_ret,
	OWPBoolean	send,
	OWPAddr		localaddr,
	OWPTestSpec	*test_spec,
	OWPSID		sid_ret		/* only used if !send */
);

/*
 * Given remote_addr/port (can "connect" to remote addr now)
 * return OK
 * set the sid (if this is a recv - MUST be the same as came from Initfunc)
 */
typedef OWPErrSeverity (*OWPEndpointInitHookFunc)(
	void		*app_data,
	void		*end_data,
	OWPAddr		remoteaddr,
	OWPSID		sid
);

/*
 * Given start session
 */
typedef OWPErrSeverity (*OWPEndpointStartFunc)(
	void		*app_data,
	void		*end_data
);

/*
 * Given stop session
 */
typedef OWPErrSeverity (*OWPEndpointStopFunc)(
	void		*app_data,
	void		*end_data,
	OWPAcceptType	aval
);

/*
 * Given retrieve session
 */
typedef void (*OWPRetrieveSessionDataFunc)(
	void		*app_data,
	OWPSID		sid,
	OWPErrSeverity	*err_ret
);

/*
 * This type is used to define the function that is called to retrieve the
 * current timestamp.
 */
typedef OWPTimeStamp (*OWPGetTimeStampFunc)(
	void		*app_data,
	OWPErrSeverity	*err_ret
);


/* 
 * This structure encodes parameters needed to initialize the library.
 */ 
typedef struct {
	struct timeval			tm_out;
	void				*err_data;
	OWPErrFunc			err_func;
	OWPGetAESKeyFunc		get_aes_key_func;
	OWPCheckControlPolicyFunc	check_control_func;
	OWPCheckTestPolicyFunc		check_test_func;
	OWPEndpointInitFunc		endpoint_init_func;
	OWPEndpointInitHookFunc		endpoint_init_hook_func;
	OWPEndpointStartFunc		endpoint_start_func;
	OWPEndpointStopFunc		endpoint_stop_func;
	OWPGetTimeStampFunc		get_timestamp_func;
} OWPInitializeConfigRec, *OWPInitializeConfig;

/*
 * API Functions
 *
 */
extern OWPContext
OWPContextInitialize(
	OWPInitializeConfig	config
);

extern void
OWPContextFree(
	OWPContext	ctx
);

/*
 * Error reporting routines - in the end these will just call the
 * function that is registered for the context as the OWPErrFunc
 */
extern void
OWPError(
	OWPContext	ctx,
	OWPErrSeverity	severity,
	OWPErrType	etype,
	const char	*fmt,
	...
);

#define OWPLine	__FILE__,__LINE__

extern void
OWPErrorLine(
	OWPContext	ctx,
	const char	*file,	/* fill with __FILE__ macro */
	int		line,	/* fill with __LINE__ macro */
	OWPErrSeverity	severity,
	OWPErrType	etype,
	const char	*fmt,
	...
);

/*
 * The OWPAddrBy* functions are used to allow the OWP API to more
 * adequately manage the memory associated with the many different ways
 * of specifying an address - and to provide a uniform way to specify an
 * address to the main API functions.
 * These functions return NULL on failure. (They call the error handler
 * to specify the reason.)
 */
extern OWPAddr
OWPAddrByNode(
	OWPContext	ctx,
	const char	*node	/* dns or valid char representation of addr */
);

extern OWPAddr
OWPAddrByAddrInfo(
	OWPContext		ctx,
	const struct addrinfo	*ai	/* valid addrinfo linked list	*/
);

extern OWPAddr
OWPAddrBySockFD(
	OWPContext	ctx,
	int		fd	/* fd must be an already connected socket */
);

/*
 * return FD for given OWPAddr or -1 if it doesn't refer to a socket yet.
 */
extern int
OWPAddrFD(
	OWPAddr	addr
	);

/*
 * return socket address length (for use in calling accept etc...)
 * or 0 if it doesn't refer to a socket yet.
 */
extern socklen_t
OWPAddrSockLen(
	OWPAddr	addr
	);

extern OWPErrSeverity
OWPAddrFree(
	OWPAddr	addr
);

/*
 * These functions return -1 on error. They read/write n bytes to the
 * file descriptor. (They loop internally calling read/write until the
 * entire buffer is read/wrote.) Readn may return less then n bytes if
 * it encounters EOF.
 */
extern ssize_t
OWPReadn(
	int	fd,
	void	*buff,
	size_t	n
	 );

extern ssize_t
OWPWriten(
	int		fd,
	const void	*buff,
	size_t		n
	  );

/*
 * OWPControlOpen allocates an OWPclient structure, opens a connection to
 * the OWP server and goes through the initialization phase of the
 * connection. This includes AES/CBC negotiation. It returns after receiving
 * the ServerOK message.
 *
 * This is typically only used by an OWP client application (or a server
 * when acting as a client of another OWP server).
 *
 * err_ret values:
 * 	OWPErrOK	completely successful - highest level mode ok'd
 * 	OWPErrINFO	session connected with less than highest level mode
 * 	OWPErrWARNING	session connected but future problems possible
 * 	OWPErrFATAL	function will return NULL - connection is closed.
 * 		(Errors will have been reported through the OWPErrFunc
 * 		in all cases.)
 * function return values:
 * 	If successful - even marginally - a valid OWPclient handle
 * 	is returned. If unsuccessful, NULL is returned.
 *
 * local_addr can only be set using OWPAddrByNode or OWPAddrByAddrInfo
 * server_addr can use any of the OWPAddrBy* functions.
 *
 * Once an OWPAddr record is passed into this function - it is
 * automatically free'd and should not be referenced again in any way.
 *
 * Client
 */
extern OWPControl
OWPControlOpen(
	OWPContext	ctx,
	OWPAddr		local_addr,	/* src addr or NULL		*/
	OWPAddr		server_addr,	/* server addr or NULL		*/
	u_int32_t	mode_mask,	/* OR of OWPSessionMode vals	*/
	const char	*kid,		/* null if unwanted		*/
	void		*app_data,	/* set app_data	for connection	*/
	OWPErrSeverity	*err_ret
);

/*
 * Client and Server
 */
extern OWPErrSeverity
OWPControlClose(
	OWPControl	cntrl
);

/*
 * Request a test session - if err_ret is OWPErrOK - then the function
 * returns a valid SID for the session.
 *
 * Once an OWPAddr record has been passed into this function, it
 * is automatically free'd. It should not be referenced again in any way.
 *
 * Client
 */
extern OWPBoolean
OWPRequestTestSession(
	OWPControl	control_handle,
	OWPAddr		sender,
	OWPBoolean	server_conf_sender,
	OWPAddr		receiver,
	OWPBoolean	server_conf_receiver,
	OWPTestSpec	*test_spec,
	OWPSID		sid_ret,
	OWPErrSeverity	*err_ret
);

/*
 * Start all test sessions - if successful, returns OWPErrOK.
 *
 * Client and Server
 */
extern OWPErrSeverity
OWPStartTestSessions(
	OWPControl	control_handle
);

/*
 * Wait for test sessions to complete. This function will return the
 * following integer values:
 * 	<0	ErrorCondition (can cast to OWPErrSeverity)
 * 	0	StopSessions received (OWPErrOK)
 * 	1	wake_time reached
 * 	2	CollectSession received from other side, and this side has
 * 		a receiver endpoint.
 *	3	system event (signal)
 *
 * Client and Server
 */
extern int
OWPWaitTestSessionStop(
	OWPControl	control_handle,
	OWPTimeStamp	wake_time,		/* abs time */
	OWPErrSeverity	*err_ret
);

/*
 * Return the file descriptor being used for the control connection. An
 * application can use this to call select or otherwise poll to determine
 * if anything is ready to be read but they should not read or write to
 * the descriptor.
 * This can be used in conjunction with the OWPWaitTestSessionStop
 * function so that the application can recieve user input, and only call
 * the OWPWaitTestSessionStop function when there is something to read
 * from the connection. (A timestamp in the past would be used in this case
 * so that OWPWaitTestSessionStop does not block.)
 *
 * If the control_handle is no longer connected - the function will return
 * a negative value.
 *
 * Client and Server.
 */
extern int
OWPGetControlFD(
	OWPControl	control_handle
);

/*
 * Send the StopSession message, and wait for the response.
 *
 * Client and Server.
 */
extern void
OWPSendStopSessions(
	OWPControl	control_handle,
	OWPErrSeverity	*err_ret
);

extern
OWPAddr
OWPServerSockCreate(
	OWPContext	ctx,
	OWPAddr		addr,
	OWPErrSeverity	*err_ret
	);


/*!
 * Function:	OWPControlAccept
 *
 * Description:	
 * 		This function is used to initialiize the communication
 * 		to the peer.
 *           
 * In Args:	
 * 		connfd,connsaddr, and connsaddrlen are all returned
 * 		from "accept".
 *
 * Returns:	Valid OWPControl handle on success, NULL if
 *              the request has been rejected, or error has occurred.
 *              Return value does not distinguish between illegal
 *              requests, those rejected on policy reasons, or
 *              errors encountered by the server during execution.
 * 
 * Side Effect:
 */
extern OWPControl
OWPControlAccept(
	OWPContext	ctx,		/* library context		*/
	int		connfd,		/* conencted socket		*/
	struct sockaddr	*connsaddr,	/* connected socket addr	*/
	socklen_t	connsaddrlen,	/* connected socket addr len	*/
	u_int32_t	mode_offered,	/* advertised server mode	*/
	void		*app_data,	/* set app_data for conn	*/
	OWPErrSeverity	*err_ret	/* err - return			*/
		 );


extern OWPErrSeverity
OWPProcessTestRequest(
	OWPControl	cntrl
		);

extern OWPErrSeverity
OWPProcessStartSessions(
	OWPControl	cntrl
	);

extern OWPErrSeverity
OWPProcessStopSessions(
	OWPControl	cntrl
	);

extern OWPErrSeverity
OWPProcessRetrieveSession(
	OWPControl	cntrl
	);

/*
 * TODO: Add timeout so ProcessRequests can break out if no request
 * comes in some configurable fixed time. (Necessary to have the server
 * process exit when test sessions are done, if the client doesn't send
 * the StopSessions.)
 */
extern OWPErrSeverity
OWPProcessRequests(
	OWPControl	cntrl
		);

/*
** Fetch context field of OWPControl structure.
*/
extern OWPContext
OWPGetContext(
	OWPControl	cntrl
	);

extern OWPSessionMode
OWPGetMode(
	OWPControl	cntrl
	);

typedef u_int64_t owp_packsize_t;
/*
** Given the protocol family, OWAMP mode and packet padding,
** compute the size of resulting full IP test packet.
*/
owp_packsize_t owp_ip_packet_size(int af, int mode, u_int32_t padding);


/*
** Request records with numbers from <begin> to <end>
** of a given session <SID>. Process server response (Control-Ack).
** On success, read the first 16 octets of data transmitted
** by the server, save the number of records promised into
** *numrec, and return OWPErrOK. Else return OWPErrFATAL.
*/
OWPErrSeverity
OWPFetchSession(OWPControl cntrl,
		u_int32_t  begin,
		u_int32_t  end,
		OWPSID	   sid,
		u_int32_t  *num_rec);
#endif	/* OWAMP_H */
