#ifndef __http_h__
#define __http_h__

#ifdef HAVE_LIBZ
#define DO_GZIP
#endif

#ifdef HAVE_SSL_H
#define DO_SSL
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>

#include <errno.h>
#include <stdio.h>
#ifdef DO_SSL
#include <ssl.h>
#endif
#include <unistd.h>
#include <netdb.h>

#include <qstack.h>
#include <qstring.h>

#include <kio_interface.h>
#include <kio_base.h>
#include <kurl.h>


// Default ports.. you might want to change this if you're trying to dodge
// a proxy with some creative network address translation..  HTTP_PORT
// is used for HTTP and WebDAV/WebFolders, HTTPS_PORT is well used for
// HTTPS. Duh.
#define DEFAULT_HTTP_PORT	80
#define DEFAULT_HTTPS_PORT	443

class HTTPProtocol : public IOProtocol
{
public:
  HTTPProtocol( Connection *_conn );
  virtual ~HTTPProtocol() { }

  enum HTTP_REV    {HTTP_Unknown, HTTP_10, HTTP_11};
  enum HTTP_AUTH   {AUTH_None, AUTH_Basic, AUTH_Digest};
  enum HTTP_PROTO  {PROTO_HTTP, PROTO_HTTPS, PROTO_WEBDAV};
  enum HTTP_METHOD {HTTP_GET, HTTP_PUT, HTTP_PROPFIND, HTTP_HEAD, HTTP_DELETE};

  virtual void slotGet( const char *_url );
  virtual void slotGetSize( const char *_url );
  virtual void slotCopy( const char *_source, const char *_dest );
  
  virtual bool error( int _err, const char *_txt );

  void jobError( int _errid, const char *_txt );
  
  Connection* connection() { return ConnectionSignals::m_pConnection; }

protected:

  void decodeChunked();
  void decodeGzip();

  int openStream();
  ssize_t write (const void *buf, size_t nbytes);
  ssize_t read (void *b, size_t nbytes);
  char *gets (char *str, int size);
  bool eof ();
  bool m_bEOF;

  /**
    * Add an encoding on to the appropiate stack
    * this is nececesary because transfer encodings
    * and content encodings must be handled separately.
    */
  void addEncoding(QString, QStack<char> *);

  bool isValidProtocol (const char *);
  bool isValidProtocol (KURL *);

  void configAuth(const char *, bool);

  size_t sendData();

  bool initSockaddr( struct sockaddr_in *server_name, const char *hostname, int port);
  bool http_open( KURL &_url, const char* _post_data, int _post_data_len, bool _reload, unsigned long _offset = 0 );
  void http_close();

  void clearError() { m_iSavedError = 0; }
  void releaseError() {
    if ( m_iSavedError )
      IOProtocol::error( m_iSavedError, m_strSavedError.c_str() );
    m_iSavedError = 0;
  }


  const char *getUserAgentString();

protected: // Members
  int m_cmd, m_sock, m_iSize;
  FILE* m_fsocket;
  enum HTTP_REV m_HTTPrev;
  enum HTTP_PROTO m_proto;

  // Language/Encoding
  QStack<char> m_qTransferEncodings, m_qContentEncodings;
  QByteArray big_buffer;
  string m_sContentMD5, 
         m_strMimeType,
         m_strCharsets,
         m_strLanguages;
  
  // Proxy related members
  bool m_bUseProxy;
  int m_strProxyPort;
  string m_strNoProxyFor,
         m_strProxyHost,
         m_strProxyUser,
         m_strProxyPass;
  struct sockaddr_in m_proxySockaddr;

  // Authentication
  string m_strRealm, 
         m_strAuthString, 
         m_strProxyAuthString;
  enum HTTP_AUTH Authentication, ProxyAuthentication;

  // Stuff to hold various error state information
  int m_iSavedError;
  string m_strSavedError;
  bool m_bIgnoreJobErrors,
       m_bIgnoreErrors; 

  bool m_bCanResume;

#ifdef DO_SSL
  bool m_bUseSSL2, m_bUseSSL3, m_bUseTLS1, m_bUseSSL;
  SSL_METHOD *meth;
  SSL_CTX *ctx;
  SSL *hand;
#endif

};

class HTTPIOJob : public IOJob
{
public:
  HTTPIOJob( Connection *_conn, HTTPProtocol *_gzip );
  
  virtual void slotError( int _errid, const char *_txt );

protected:
  HTTPProtocol* m_pHTTP;
};

#endif
