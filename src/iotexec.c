/*==============================================================================
MIT License

Copyright (c) 2023 Trevor Monk

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
==============================================================================*/

/*!
 * @defgroup iotexec iotexec
 * @brief Service to execute cloud-to-device commands
 * @{
 */

/*============================================================================*/
/*!
@file iotexec.c

    IOT cloud-to-device command execution service

    The iotexec service receives cloud-to-device commands via the
    iotclient library, executes them, and sends the responses back
    using a device-to-cloud message via the iotclient library.

*/
/*============================================================================*/

/*==============================================================================
        Includes
==============================================================================*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <stdbool.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <iotclient/iotclient.h>

/*==============================================================================
        Private definitions
==============================================================================*/

/*! Maximum command length */
#define MAX_MESSAGE_LENGTH  4096

/*! Maximum pending commands */
#define MAX_PENDING_MESSAGES 10

/*! iotexec state */
typedef struct iotexecState
{
    /*! variable server handle */
    IOTCLIENT_HANDLE hIoTClient;

    /*! verbose flag */
    bool verbose;

} IOTExecState;

/*==============================================================================
        Private file scoped variables
==============================================================================*/

/*! iotexec application State object */
IOTExecState state;

/*==============================================================================
        Private function declarations
==============================================================================*/

int main(int argc, char **argv);
static int ProcessOptions( int argC, char *argV[], IOTExecState *pState );
static void usage( char *cmdname );
static int ProcessMessages(IOTExecState *pState);
static int ProcessMessage(IOTExecState *pState);
static int ProcessCommand( IOTExecState *pState,
                           const char *cmd,
                           const char *msgId );
static void SetupTerminationHandler( void );
static void TerminationHandler( int signum, siginfo_t *info, void *ptr );

/*==============================================================================
        Private function definitions
==============================================================================*/

/*============================================================================*/
/*  main                                                                      */
/*!
    Main entry point for the iotsend application

    @param[in]
        argc
            number of arguments on the command line
            (including the command itself)

    @param[in]
        argv
            array of pointers to the command line arguments

    @return none

==============================================================================*/
int main(int argc, char **argv)
{
    int result = EINVAL;

    /* process the command line options */
    ProcessOptions( argc, argv, &state );

    /* set up an abnormal termination handler */
    SetupTerminationHandler();

    state.hIoTClient = IOTCLIENT_Create();
    if ( state.hIoTClient != NULL )
    {
        IOTCLIENT_SetVerbose( state.hIoTClient, state.verbose );

        /* create a cloud-to-device message receiver */
        result = IOTCLIENT_CreateReceiver( state.hIoTClient,
                                           "exec",
                                           MAX_PENDING_MESSAGES,
                                           MAX_MESSAGE_LENGTH );
        if( result == EOK )
        {
            ProcessMessages( &state );
        }

        IOTCLIENT_Close( state.hIoTClient );
    }

    return result;
}

/*============================================================================*/
/*  ProcessMessages                                                           */
/*!
    Process cloud-to-device command messages

    The ProcessMessages function waits for and processes received
    cloud-to-device commands.

    @param[in]
        pState
            pointer to the IOTExecState

==============================================================================*/
static int ProcessMessages(IOTExecState *pState)
{
    int result = EINVAL;
    char *pHeader;
    char *pBody;
    size_t headerLength;
    size_t nodyLength;
    FILE *fp;

    if( pState != NULL )
    {
        while( true )
        {
            result = ProcessMessage(pState);
        }
    }

    return result;
}

/*============================================================================*/
/*  ProcessMessage                                                            */
/*!
    Process a cloud-to-device command message

    The ProcessMessage function waits for and processes a received
    cloud-to-device message.

    @param[in]
        pState
            pointer to the IOTExecState

    @retval EOK message was processed successfully
    @retval EINVAL invalid arguments
    @retval EMSGSIZE message is too large and cannot be processed
    @retval error as returned from ProcessCommand

==============================================================================*/
static int ProcessMessage(IOTExecState *pState)
{
    int result = EINVAL;
    char *pHeader;
    char *pBody;
    size_t headerLength = 0;
    size_t bodyLength = 0;
    char rxMsgId[64];
    const char *msgId = NULL;
    int rc;

    if ( pState != NULL )
    {
        /* wait for a cloud-to-device message */
        result = IOTCLIENT_Receive( pState->hIoTClient,
                                    &pHeader,
                                    &pBody,
                                    &headerLength,
                                    &bodyLength );
        if( result == EOK )
        {
            printf("ProcessMessage!!\n");
            printf("headerLength = %ld\n", headerLength );
            printf("header (%ld): %.*s\n",
                    headerLength,
                    (int)headerLength,
                    pHeader );

            printf("body (%ld): %.*s\n",
                    bodyLength,
                    (int)bodyLength,
                    pBody );

            if ( pHeader != NULL )
            {
                /* try to get the 'messageID' property */
                rc = IOTCLIENT_GetProperty( pHeader,
                                            "messageId",
                                            rxMsgId,
                                            sizeof( rxMsgId ) );
                if( rc == EOK )
                {
                    printf("messageId = %s\n", rxMsgId );
                    msgId = rxMsgId;
                }
            }

            if ( ( pBody != NULL ) &&
                 ( headerLength + bodyLength < MAX_MESSAGE_LENGTH ) )
            {
                /* NUL terminate the message body */
                pBody[bodyLength] = '\0';

                /* process received message as a command */
                result = ProcessCommand( pState, pBody, msgId );
            }
            else
            {
                result = EMSGSIZE;
            }
        }

        if( pState->verbose && ( result != EOK ) )
        {
            fprintf(stderr, "ProcessMessage: %s\n", strerror(result));
        }
    }

    return result;
}

/*============================================================================*/
/*  ProcessCommand                                                            */
/*!
    Process a command and stream the output as a device-to-cloud stream

    The ProcessCommand function executes the specified command using
    popen, and streams the commands output to the cloud as a device-to-cloud
    message via the IOTCLIENT_Stream function.

    @param[in]
        pState
            pointer to the IOTExecState

    @param[in]
        cmd
            pointer to the NUL terminated command to execute

    @retval EINVAL invalid arguments
    @retval EOK the command was executed and the results streamed successfully
    @retval ENOTSUP the command could not be executed
    @retval EBADF could not get the command output file descriptor
    @retval error as returned from IOTCLIENT_Stream

==============================================================================*/
static int ProcessCommand( IOTExecState *pState,
                           const char *cmd,
                           const char *msgId )
{
    int result = EINVAL;
    FILE *fp;
    char buf[BUFSIZ];
    const char *headers = "source:exec\nmessagetype:cmdresp";
    int fd;
    size_t n;

    if( ( pState != NULL ) &&
        ( cmd != NULL ) )
    {
        if( pState->verbose )
        {
            fprintf(stdout, "Processing Command: %s\n", cmd );
        }

        if( msgId != NULL )
        {
            if( pState->verbose )
            {
                fprintf(stdout, "MessageID: %s\n", msgId );
            }

            /* handle correlation idenfifier */
            /* messsageId -> correlationId */
            n = snprintf( buf,
                          BUFSIZ,
                          "%s\ncorrelationId:%s\n",
                          headers,
                          msgId);
            if( ( n > 0 ) && ( n < BUFSIZ ) )
            {
                /* update the headers to include the correlation Id */
                headers = buf;
            }
        }

        /* execute the command */
        fp = popen( cmd, "r" );
        if ( fp != NULL )
        {
            /* get the file descriptor for the commands stdout */
            fd = fileno( fp );
            if ( fd != -1 )
            {
                result = IOTCLIENT_Stream( pState->hIoTClient, headers, fd );
            }
            else
            {
                result = EBADF;
            }

            /* close the command output stream */
            pclose( fp );
        }
        else
        {
            result = ENOTSUP;
        }
    }

    return result;
}

/*============================================================================*/
/*  usage                                                                     */
/*!
    Display the application usage

    The usage function dumps the application usage message
    to stderr.

    @param[in]
       cmdname
            pointer to the invoked command name

    @return none

==============================================================================*/
static void usage( char *cmdname )
{
    if( cmdname != NULL )
    {
        fprintf(stderr,
                "usage: %s [-v] [-h]\n"
                " [-h] : display this help\n"
                " [-v] : verbose output\n",
                cmdname );
    }
}

/*============================================================================*/
/*  ProcessOptions                                                            */
/*!
    Process the command line options

    The ProcessOptions function processes the command line options and
    populates the iotsend state object

    @param[in]
        argC
            number of arguments
            (including the command itself)

    @param[in]
        argv
            array of pointers to the command line arguments

    @param[in]
        pState
            pointer to the iotsend state object

    @return none

==============================================================================*/
static int ProcessOptions( int argC, char *argV[], IOTExecState *pState )
{
    int c;
    int result = EINVAL;
    const char *options = "hv";

    if( ( pState != NULL ) &&
        ( argV != NULL ) )
    {
        while( ( c = getopt( argC, argV, options ) ) != -1 )
        {
            switch( c )
            {
                case 'v':
                    pState->verbose = true;
                    break;

                case 'h':
                    usage( argV[0] );
                    break;

                default:
                    break;

            }
        }
    }

    return 0;
}

/*============================================================================*/
/*  SetupTerminationHandler                                                   */
/*!
    Set up an abnormal termination handler

    The SetupTerminationHandler function registers a termination handler
    function with the kernel in case of an abnormal termination of this
    process.

==============================================================================*/
static void SetupTerminationHandler( void )
{
    static struct sigaction sigact;

    memset( &sigact, 0, sizeof(sigact) );

    sigact.sa_sigaction = TerminationHandler;
    sigact.sa_flags = SA_SIGINFO;

    sigaction( SIGTERM, &sigact, NULL );
    sigaction( SIGINT, &sigact, NULL );
}

/*============================================================================*/
/*  TerminationHandler                                                        */
/*!
    Abnormal termination handler

    The TerminationHandler function will be invoked in case of an abnormal
    termination of this process.  The termination handler closes
    the connection with the variable server and cleans up its VARFP shared
    memory.

@param[in]
    signum
        The signal which caused the abnormal termination (unused)

@param[in]
    info
        pointer to a siginfo_t object (unused)

@param[in]
    ptr
        signal context information (ucontext_t) (unused)

==============================================================================*/
static void TerminationHandler( int signum, siginfo_t *info, void *ptr )
{
    syslog( LOG_ERR, "Abnormal termination of iotexec\n" );
    IOTCLIENT_Close( state.hIoTClient );

    exit( 1 );
}

/*! @}
 * end of iotexec group */
