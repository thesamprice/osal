/*
 *  NASA Docket No. GSC-18,370-1, and identified as "Operating System Abstraction Layer"
 *
 *  Copyright (c) 2019 United States Government as represented by
 *  the Administrator of the National Aeronautics and Space Administration.
 *  All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/**
 * \file   os-impl-bsd-sockets.c
 * \author joseph.p.hickey@nasa.gov
 *
 * Purpose: This file contains the network functionality for for
 *      systems which implement the BSD-style socket API.
 */

/****************************************************************************************
                                    INCLUDE FILES
 ***************************************************************************************/


/*
 * Inclusions Defined by OSAL layer.
 *
 * This must include whatever is required to get the prototypes of these functions:
 *
 *  socket()
 *  getsockopt()
 *  setsockopt()
 *  fcntl()
 *  bind()
 *  listen()
 *  accept()
 *  connect()
 *  recvfrom()
 *  sendto()
 *  inet_pton()
 *  ntohl()/ntohs()
 *
 * As well as any headers for the struct sockaddr type and any address families in use
 */
#include <string.h>
#include <errno.h>

#include "os-impl-sockets.h"

extern "C" {

#include "os-shared-file.h"
#include "os-shared-select.h"
#include "os-shared-sockets.h"
#include "os-shared-idmap.h"
}


/****************************************************************************************
                                     DEFINES
****************************************************************************************/

typedef union
{
    char               data[OS_SOCKADDR_MAX_LEN];
    struct sockaddr    sa;
    struct sockaddr_in sa_in;
#ifdef OS_NETWORK_SUPPORTS_IPV6
    struct sockaddr_in6 sa_in6;
#endif
} OS_SockAddr_Accessor_t;



QServer::QServer(QTcpSocket *_server_socket,QObject *parent ) : QTcpServer(parent) {
    server_socket = _server_socket;
    connect( server_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(tcpError(QAbstractSocket::SocketError)) );
    connect( server_socket, SIGNAL(readyRead()),
             this, SLOT(tcpReady()) );
    server_socket->setSocketOption(QAbstractSocket::KeepAliveOption, true );
}

QServer::~QServer() {
    server_socket->disconnectFromHost();
    server_socket->waitForDisconnected();
}

void QServer::tcpReady() {
    QByteArray array = server_socket->read(server_socket->bytesAvailable());
}

void QServer::tcpError(QAbstractSocket::SocketError error) {
    // QMessageBox::warning( (QWidget *)this->parent(), tr("Error"),tr("TCP error: %1").arg( server_socket->errorString() ) );
}

bool QServer::start_listen(int port_no) {
    if( !this->listen( QHostAddress::Any, port_no ) ) {
        // QMessageBox::warning( (QWidget *)this->parent(), tr("Error!"), tr("Cannot listen to port %1").arg(port_no) );
        return false;
    }
    else{
        return true;
    }
}

void QServer::incomingConnection(qintptr descriptor) {
    if( !server_socket->setSocketDescriptor( descriptor ) ) {
        // QMessageBox::warning( (QWidget *)this->parent(), tr("Error!"), tr("Socket error!") );
        return;
    }
}

typedef struct OS_QT_Sock_t{
    QUdpSocket * udp;
    QTcpSocket *tcp;
    QAbstractSocket * generic;
    QServer * tcp_server;
    OS_SocketType_t socket_type;
    // OS_SocketDomain_t socket_domain;
    /* Socket file descriptor */
    int fd;
    int selectable;
    int port;
    QHostAddress host_addr;
    std::string name;
}OS_QT_Sock_t;


/* Globals */

OS_QT_Sock_t OS_impl_sockets[OS_MAX_NUM_OPEN_FILES] = {{0}};



QHostAddress OS_Address_To_QtAddress(const OS_SockAddr_t *Addr){
    const struct sockaddr *         sa;

    sa = (const struct sockaddr *)&Addr->AddrData;
    // struct sockaddr_in *sin = (struct sockaddr_in *)sa;
    // char ip[INET_ADDRSTRLEN];
    // inet_pton (AF_INET, sin->sin_addr, ip, sizeof (ip));

    QHostAddress qt_addr(sa);
    return qt_addr;
}

uint16_t OS_Address_To_Port(const OS_SockAddr_t *Addr){
    const struct sockaddr *         sa;

    sa = (const struct sockaddr *)&Addr->AddrData;
    struct sockaddr_in sin;
    memcpy(&sin, sa, sizeof(sin));
    
    uint16_t port;
    port = htons (sin.sin_port);
    return port;
}
void QtAddressPort_To_OS_Address(const QHostAddress &addr, int port, OS_SockAddr_t * os_addr){
    struct sockaddr *         sa;
    sa = (struct sockaddr *)&os_addr->AddrData;
    struct sockaddr_in sin;
    memcpy(&sin, sa, sizeof(sin));
    sin.sin_port = ntohs(port);
    memcpy( sa,&sin, sizeof(sin));


    if(sizeof(sin.sin_addr) ==4 ){
        int32_t ip4 = addr.toIPv4Address();
        ip4 = htonl(ip4);
        memcpy(&sin.sin_addr,&ip4, 4 );
        sa->sa_family = AF_INET;
    }else{
        Q_IPV6ADDR ip6 = addr.toIPv6Address();
        memcpy(&sin.sin_addr,&ip6, sizeof(sin.sin_addr));
        sa->sa_family = AF_INET6;
    }
    



}

/****************************************************************************************
                                    Sockets API
 ***************************************************************************************/
extern "C" {

/*----------------------------------------------------------------
 *
 * Function: OS_SocketOpen_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_SocketOpen_Impl(const OS_object_token_t *token)
{
    int                             os_domain;
    int                             os_type;
    int                             os_proto;
    // int                             os_flags;
    OS_QT_Sock_t *impl = OS_OBJECT_TABLE_GET(OS_impl_sockets, *token);
    OS_stream_internal_record_t* stream = OS_OBJECT_TABLE_GET(OS_stream_table, *token);


    os_proto = 0;

    switch (stream->socket_type)
    {
        case OS_SocketType_DATAGRAM:
            impl->socket_type = OS_SocketType_DATAGRAM;
            os_type  = SOCK_DGRAM;
            os_proto = IPPROTO_UDP;

            impl->udp = new QUdpSocket();
            impl->generic = impl->udp;
            break;

        case OS_SocketType_STREAM:
            impl->socket_type = OS_SocketType_STREAM;
            os_type  = SOCK_STREAM;
            os_proto = IPPROTO_TCP;
            impl->tcp        = new QTcpSocket();
            impl->tcp_server = new QServer(impl->tcp);
            impl->generic = impl->tcp;
            break;

        default:
            return OS_ERR_NOT_IMPLEMENTED;
    }

    switch (stream->socket_domain)
    {
        case OS_SocketDomain_INET:
            os_domain = AF_INET;
            break;
#ifdef OS_NETWORK_SUPPORTS_IPV6
        case OS_SocketDomain_INET6:
            os_domain = AF_INET6;
            break;
#endif
        default:
            return OS_ERR_NOT_IMPLEMENTED;
    }

    // impl->fd = socket(os_domain, os_type, os_proto);
    // if (impl->fd < 0)
    // {
    //     return OS_ERROR;
    // }

    // /*
    //  * Setting the REUSEADDR flag helps during debugging when there might be frequent
    //  * code restarts.  However if setting the option fails then it is not worth bailing out over.
    //  */
    // os_flags = 1;
    // setsockopt(impl->fd, SOL_SOCKET, SO_REUSEADDR, &os_flags, sizeof(os_flags));

    // /*
    //  * Set the standard options on the filehandle by default --
    //  * this may set it to non-blocking mode if the implementation supports it.
    //  * any blocking would be done explicitly via the select() wrappers
    //  *
    //  * NOTE: The implementation still generally works without this flag set, but
    //  * nonblock mode does improve robustness in the event that multiple tasks
    //  * attempt to accept new connections from the same server socket at the same time.
    //  */
    // os_flags = fcntl(impl->fd, F_GETFL);
    // if (os_flags == -1)
    // {
    //     /* No recourse if F_GETFL fails - just report the error and move on. */
    //     OS_DEBUG("fcntl(F_GETFL): %s\n", strerror(errno));
    // }
    // else
    // {
    //     os_flags |= OS_IMPL_SOCKET_FLAGS;
    //     if (fcntl(impl->fd, F_SETFL, os_flags) == -1)
    //     {
    //         /* No recourse if F_SETFL fails - just report the error and move on. */
    //         OS_DEBUG("fcntl(F_SETFL): %s\n", strerror(errno));
    //     }
    // }
    // impl->generic->setSocketDescriptor(impl->fd);
    impl->selectable = OS_IMPL_SOCKET_SELECTABLE;
    
    return OS_SUCCESS;
} /* end OS_SocketOpen_Impl */

/*----------------------------------------------------------------
 *
 * Function: OS_SocketBind_Impl
 *
 *  Purpose: Binds the indicated socket table entry to the passed-in address
 *
 *-----------------------------------------------------------------*/
int32 OS_SocketBind_Impl(const OS_object_token_t *token, const OS_SockAddr_t *Addr)
{
    // int                             os_result;
    socklen_t                       addrlen;
    const struct sockaddr *         sa;
    OS_QT_Sock_t *impl;
    impl   = OS_OBJECT_TABLE_GET(OS_impl_sockets, *token);
    OS_stream_internal_record_t* stream = OS_OBJECT_TABLE_GET(OS_stream_table, *token);

    sa = (const struct sockaddr *)&Addr->AddrData;

    switch (sa->sa_family)
    {
        case AF_INET:
            addrlen = sizeof(struct sockaddr_in);
            break;
#ifdef OS_NETWORK_SUPPORTS_IPV6
        case AF_INET6:
            addrlen = sizeof(struct sockaddr_in6);
            break;
#endif
        default:
            addrlen = 0;
            break;
    }

    if (addrlen == 0)
    {
        return OS_ERR_BAD_ADDRESS;
    }
    impl->host_addr  = OS_Address_To_QtAddress(Addr);
    uint16_t port = OS_Address_To_Port(Addr);
    QString name = QString("%1:%2").arg(impl->host_addr.toString(), port);
    impl->name = name.toStdString();

    if(impl->generic->bind(impl->host_addr,port) == false)
    {
        OS_DEBUG("bind: %s\n", strerror(errno));
        return OS_ERR_INCORRECT_OBJ_STATE;
    }

    /* Start listening on the socket (implied for stream sockets) */
    if (stream->socket_type == OS_SocketType_STREAM)
    {
        if ( impl->tcp_server->start_listen(impl->port) == false)
        {
            OS_DEBUG("listen: %s\n", strerror(errno));
            return OS_ERROR;
        }
    }
    return OS_SUCCESS;
} /* end OS_SocketBind_Impl */

/*----------------------------------------------------------------
 *
 * Function: OS_SocketConnect_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_SocketConnect_Impl(const OS_object_token_t *token, const OS_SockAddr_t *Addr, int32 timeout)
{
    int32                           return_code;
    // int                             os_status;
    // int                             sockopt;
    socklen_t                       slen;
    // uint32                          operation;
    const struct sockaddr *         sa;
    OS_QT_Sock_t *impl = OS_OBJECT_TABLE_GET(OS_impl_sockets, *token);

    // char ip[INET_ADDRSTRLEN];
    // uint16_t port;


    sa = (const struct sockaddr *)&Addr->AddrData;
    switch (sa->sa_family)
    {
        case AF_INET:
            slen = sizeof(struct sockaddr_in);
            break;
#ifdef OS_NETWORK_SUPPORTS_IPV6
        case AF_INET6:
            slen = sizeof(struct sockaddr_in6);
            break;
#endif
        default:
            slen = 0;
            break;
    }

    if (slen != Addr->ActualLength)
    {
        return_code = OS_ERR_BAD_ADDRESS;
    }
    else
    {
        return_code = OS_SUCCESS;
         QHostAddress host_addr = OS_Address_To_QtAddress(Addr);
         uint16_t port = OS_Address_To_Port(Addr);
         impl->port = port;
         /* See https://doc.qt.io/qt-6/qabstractsocket.html#connectToHost */
        // impl->generic->bind(host_addr, port);
        impl->generic->connectToHost(host_addr, port);

        if (impl->generic->waitForConnected(timeout) == false)
        {
            OS_DEBUG("connect: %s\n", impl->generic->errorString().toLocal8Bit().data() );
            return_code = OS_ERROR;

        }
    }
    return return_code;
} /* end OS_SocketConnect_Impl */

/*----------------------------------------------------------------
   Function: OS_SocketShutdown_Impl

    Purpose: Connects the socket to a remote address.
             Socket must be of the STREAM variety.

    Returns: OS_SUCCESS on success, or relevant error code
 ------------------------------------------------------------------*/
int32 OS_SocketShutdown_Impl(const OS_object_token_t *token, OS_SocketShutdownMode_t Mode)
{
    int32                           return_code;
    // int                             how;
    return_code = OS_SUCCESS;
    OS_QT_Sock_t *conn_impl = OS_OBJECT_TABLE_GET(OS_impl_sockets, *token);

    conn_impl->generic->disconnectFromHost();
    if (conn_impl->generic->state() == QAbstractSocket::UnconnectedState){
        /* Already disconnected */
    }else if(conn_impl->generic->waitForDisconnected(-1) == false){

        qDebug() << conn_impl->generic->errorString();
        return_code = OS_ERROR;
    }


    /* Note that when called via the shared layer,
     * the "Mode" arg has already been checked/validated. */
    if (Mode == OS_SocketShutdownMode_SHUT_READ)
    {
        return_code = 1;
    }
    else if (Mode == OS_SocketShutdownMode_SHUT_WRITE)
    {
        return_code = 2;
    }



    return return_code;
} /* end OS_SocketShutdown_Impl */

/*----------------------------------------------------------------
 *
 * Function: OS_SocketAccept_Impl
 *
 *  Purpose: Accept an incoming connection on the indicated socket (must be a STREAM socket)
 *          Will wait up to "timeout" milliseconds for an incoming connection
 *          Will wait forever if timeout is negative
 *
 *-----------------------------------------------------------------*/
int32 OS_SocketAccept_Impl(const OS_object_token_t *sock_token, const OS_object_token_t *conn_token,
                           OS_SockAddr_t *Addr, int32 timeout)
{
    int32                           return_code;
    uint32                          operation;
    // socklen_t                       addrlen;
    // int                             os_flags;
    OS_QT_Sock_t *sock_impl = OS_OBJECT_TABLE_GET(OS_impl_sockets, *sock_token);
    OS_QT_Sock_t *conn_impl = OS_OBJECT_TABLE_GET(OS_impl_sockets, *conn_token);


    operation = OS_STREAM_STATE_READABLE;
    if (sock_impl->selectable)
    {
        return_code = OS_SelectSingle_Impl(sock_token, &operation, timeout);
    }
    else
    {
        return_code = OS_SUCCESS;
    }

    if (return_code == OS_SUCCESS)
    {
        if ((operation & OS_STREAM_STATE_READABLE) == 0)
        {
            return_code = OS_ERROR_TIMEOUT;
        }
        else
        {
            if(timeout < 0)
                timeout = -1; /* If msec is -1, this function will not time out. */
            if(sock_impl->tcp_server->waitForNewConnection(timeout) == false)
            {
                return_code = OS_ERROR;
            }
            else
            {
                conn_impl->tcp = sock_impl->tcp_server->nextPendingConnection();
                if(conn_impl->tcp == 0x0){
                    return_code = OS_ERROR;
                }else{
                    conn_impl->selectable = OS_IMPL_SOCKET_SELECTABLE;
                }
            }
        }
    }

    return return_code;
} /* end OS_SocketAccept_Impl */

/*----------------------------------------------------------------
 *
 * Function: OS_SocketRecvFrom_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_SocketRecvFrom_Impl(const OS_object_token_t *token, void *buffer, size_t buflen, OS_SockAddr_t *RemoteAddr,
                             int32 timeout)
{
    int result = -1;
    OS_QT_Sock_t *impl = OS_OBJECT_TABLE_GET(OS_impl_sockets, *token);
    quint16 port = OS_Address_To_Port(RemoteAddr);
    QHostAddress addr = OS_Address_To_QtAddress(RemoteAddr);

    if(impl->socket_type == OS_SocketType_DATAGRAM)
    {
        result = impl->udp->readDatagram((char*)buffer, buflen,&addr, &port);
        QtAddressPort_To_OS_Address(addr, port, RemoteAddr);

    }else{
        impl->generic->waitForReadyRead(-1);
        if(impl->generic->waitForReadyRead(-1) == false){
            return OS_ERROR;
        }
        int bytes_read = impl->generic->read((char*)buffer,buflen);
        if(bytes_read == -1){
            return OS_ERROR;
        }

    }

    // memcpy(buffer, data.constData(), data.size());
    // if(RemoteAddr != 0x0){
    //     /* TODO */
    //     // RemoteAddr->AddrDat
    // }


    // switch(impl->socket_type){
    //     case OS_SocketType_DATAGRAM:
    //         if(impl->udp->waitForReadyRead(-1) == false){
    //             return OS_ERROR;
    //         }
    //         int bytes_read = impl->udp->read((char*)buffer,buflen);
    //         if(bytes_read == -1){
    //             return OS_ERROR;
    //         }
    //         break;
    //     case OS_SocketType_STREAM:

    //         if(impl->tcp->)
    // }

    return result;
    // int32                           return_code;
    // int                             os_result;
    // int                             waitflags;
    // uint32                          operation;
    // struct sockaddr *               sa;
    // socklen_t                       addrlen;
    // OS_impl_file_internal_record_t *impl;

    // impl = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);

    // if (RemoteAddr == NULL)
    // {
    //     sa      = NULL;
    //     addrlen = 0;
    // }
    // else
    // {
    //     addrlen = OS_SOCKADDR_MAX_LEN;
    //     sa      = (struct sockaddr *)&RemoteAddr->AddrData;
    // }

    // operation = OS_STREAM_STATE_READABLE;
    // /*
    //  * If "O_NONBLOCK" flag is set then use select()
    //  * Note this is the only way to get a correct timeout
    //  */
    // if (impl->selectable)
    // {
    //     waitflags   = MSG_DONTWAIT;
    //     return_code = OS_SelectSingle_Impl(token, &operation, timeout);
    // }
    // else
    // {
    //     if (timeout == 0)
    //     {
    //         waitflags = MSG_DONTWAIT;
    //     }
    //     else
    //     {
    //         /* note timeout will not be honored if >0 */
    //         waitflags = 0;
    //     }
    //     return_code = OS_SUCCESS;
    // }

    // if (return_code == OS_SUCCESS)
    // {
    //     if ((operation & OS_STREAM_STATE_READABLE) == 0)
    //     {
    //         return_code = OS_ERROR_TIMEOUT;
    //     }
    //     else
    //     {
    //         os_result = recvfrom(impl->fd, buffer, buflen, waitflags, sa, &addrlen);
    //         if (os_result < 0)
    //         {
    //             if (errno == EAGAIN || errno == EWOULDBLOCK)
    //             {
    //                 return_code = OS_QUEUE_EMPTY;
    //             }
    //             else
    //             {
    //                 OS_DEBUG("recvfrom: %s\n", strerror(errno));
    //                 return_code = OS_ERROR;
    //             }
    //         }
    //         else
    //         {
    //             return_code = os_result;

    //             if (RemoteAddr != NULL)
    //             {
    //                 RemoteAddr->ActualLength = addrlen;
    //             }
    //         }
    //     }
    // }

    // return return_code;
} /* end OS_SocketRecvFrom_Impl */

/*----------------------------------------------------------------
 *
 * Function: OS_SocketSendTo_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_SocketSendTo_Impl(const OS_object_token_t *token, const void *buffer, size_t buflen,
                           const OS_SockAddr_t *RemoteAddr)
{
    OS_QT_Sock_t *impl = OS_OBJECT_TABLE_GET(OS_impl_sockets, *token);
    int bytes_sent=0;
    // int bytes_just_sent = 0;
    // if(impl->generic->waitForBytesWritten(-1) == false){
    //     return OS_ERROR;
    // }

    if(impl->socket_type == OS_SocketType_DATAGRAM){
        bytes_sent = impl->udp->writeDatagram((const char *)buffer, buflen, OS_Address_To_QtAddress(RemoteAddr), OS_Address_To_Port(RemoteAddr) );
    }else{
        /* TODO Check if this matches expected stream implementation */
        bytes_sent = impl->generic->write((const char *)buffer, buflen);
    }
    if(bytes_sent == -1){
        return OS_ERROR;
    }

    /* TODO Probably bugs in here from not checking bytes sent vs buflen, and not using remoteaddr */

    return bytes_sent;

    // return OS_ERR_NOT_IMPLEMENTED; /* TODO */
//     int                             os_result;
//     socklen_t                       addrlen;
//     const struct sockaddr *         sa;
//     OS_impl_file_internal_record_t *impl;

//     impl = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);

//     sa = (const struct sockaddr *)&RemoteAddr->AddrData;
//     switch (sa->sa_family)
//     {
//         case AF_INET:
//             addrlen = sizeof(struct sockaddr_in);
//             break;
// #ifdef OS_NETWORK_SUPPORTS_IPV6
//         case AF_INET6:
//             addrlen = sizeof(struct sockaddr_in6);
//             break;
// #endif
//         default:
//             addrlen = 0;
//             break;
//     }

//     if (addrlen != RemoteAddr->ActualLength)
//     {
//         return OS_ERR_BAD_ADDRESS;
//     }

//     os_result = sendto(impl->fd, buffer, buflen, MSG_DONTWAIT, sa, addrlen);
//     if (os_result < 0)
//     {
//         OS_DEBUG("sendto: %s\n", strerror(errno));
//         return OS_ERROR;
//     }

//     return os_result;
} /* end OS_SocketSendTo_Impl */

/*----------------------------------------------------------------
 *
 * Function: OS_SocketGetInfo_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_SocketGetInfo_Impl(const OS_object_token_t *token, OS_socket_prop_t *sock_prop)
{
    OS_QT_Sock_t *impl = OS_OBJECT_TABLE_GET(OS_impl_sockets, *token);
    snprintf(sock_prop->name,sizeof(sock_prop->name), "%s", impl->name.c_str());

    sock_prop->creator = -1;
    return OS_ERR_NOT_IMPLEMENTED;
    // return OS_SUCCESS;
} /* end OS_SocketGetInfo_Impl */

/*----------------------------------------------------------------
 *
 * Function: OS_SocketAddrInit_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_SocketAddrInit_Impl(OS_SockAddr_t *Addr, OS_SocketDomain_t Domain)
{
    sa_family_t             sa_family;
    socklen_t               addrlen;
    OS_SockAddr_Accessor_t *Accessor;

    memset(Addr, 0, sizeof(OS_SockAddr_t));
    Accessor = (OS_SockAddr_Accessor_t *)&Addr->AddrData;

    switch (Domain)
    {
        case OS_SocketDomain_INET:
            sa_family = AF_INET;
            addrlen   = sizeof(struct sockaddr_in);
            break;
#ifdef OS_NETWORK_SUPPORTS_IPV6
        case OS_SocketDomain_INET6:
            sa_family = AF_INET6;
            addrlen   = sizeof(struct sockaddr_in6);
            break;
#endif
        default:
            sa_family = 0;
            addrlen   = 0;
            break;
    }

    if (addrlen == 0)
    {
        return OS_ERR_NOT_IMPLEMENTED;
    }

    Addr->ActualLength     = OSAL_SIZE_C(addrlen);
    Accessor->sa.sa_family = sa_family;

    return OS_SUCCESS;
} /* end OS_SocketAddrInit_Impl */

/*----------------------------------------------------------------
 *
 * Function: OS_SocketAddrToString_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_SocketAddrToString_Impl(char *buffer, size_t buflen, const OS_SockAddr_t *Addr)
{
    // const void *                  addrbuffer;
    const OS_SockAddr_Accessor_t *Accessor;

    Accessor = (const OS_SockAddr_Accessor_t *)&Addr->AddrData;
    QHostAddress qaddr = OS_Address_To_QtAddress(Addr);
    int port = OS_Address_To_Port(Addr);
    QString str_addr = QString("%1:%2").arg( qaddr.toString() ).arg(port);
    int nu_bytes = snprintf(buffer, buflen, "%s", str_addr.toStdString().c_str());
    if(nu_bytes > buflen)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
} /* end OS_SocketAddrToString_Impl */

/*----------------------------------------------------------------
 *
 * Function: OS_SocketAddrFromString_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_SocketAddrFromString_Impl(OS_SockAddr_t *Addr, const char *string)
{
    void *                  addrbuffer;
    OS_SockAddr_Accessor_t *Accessor;

    Accessor = (OS_SockAddr_Accessor_t *)&Addr->AddrData;

    switch (Accessor->sa.sa_family)
    {
        case AF_INET:
            addrbuffer = &Accessor->sa_in.sin_addr;
            break;
#ifdef OS_NETWORK_SUPPORTS_IPV6
        case AF_INET6:
            addrbuffer = &Accessor->sa_in6.sin6_addr;
            break;
#endif
        default:
            return OS_ERR_BAD_ADDRESS;
            break;
    }

    /* This function is defined as returning 1 on success, not 0 */
    if (inet_pton(Accessor->sa.sa_family, string, addrbuffer) != 1)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
} /* end OS_SocketAddrFromString_Impl */

/*----------------------------------------------------------------
 *
 * Function: OS_SocketAddrGetPort_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_SocketAddrGetPort_Impl(uint16 *PortNum, const OS_SockAddr_t *Addr)
{
    in_port_t                     sa_port;
    const OS_SockAddr_Accessor_t *Accessor;

    Accessor = (const OS_SockAddr_Accessor_t *)&Addr->AddrData;

    switch (Accessor->sa.sa_family)
    {
        case AF_INET:
            sa_port = Accessor->sa_in.sin_port;
            break;
#ifdef OS_NETWORK_SUPPORTS_IPV6
        case AF_INET6:
            sa_port = Accessor->sa_in6.sin6_port;
            break;
#endif
        default:
            return OS_ERR_BAD_ADDRESS;
            break;
    }

    *PortNum = ntohs(sa_port);

    return OS_SUCCESS;
} /* end OS_SocketAddrGetPort_Impl */

/*----------------------------------------------------------------
 *
 * Function: OS_SocketAddrSetPort_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_SocketAddrSetPort_Impl(OS_SockAddr_t *Addr, uint16 PortNum)
{
    in_port_t               sa_port;
    OS_SockAddr_Accessor_t *Accessor;

    sa_port  = htons(PortNum);
    Accessor = (OS_SockAddr_Accessor_t *)&Addr->AddrData;

    switch (Accessor->sa.sa_family)
    {
        case AF_INET:
            Accessor->sa_in.sin_port = sa_port;
            break;
#ifdef OS_NETWORK_SUPPORTS_IPV6
        case AF_INET6:
            Accessor->sa_in6.sin6_port = sa_port;
            break;
#endif
        default:
            return OS_ERR_BAD_ADDRESS;
    }

    return OS_SUCCESS;
} /* end OS_SocketAddrSetPort_Impl */

}

// #include "inc/moc_os-impl-timebase.cpp"
#include "inc/moc_os-impl-sockets.cpp"