/* vi: ts=8 sts=4 sw=4
 *
 *
 * This file is part of the KDE project, module kdesu.
 * Copyright (C) 1999,2000 Geert Jansen <jansen@kde.org>
 * 
 * This file contains code from TEShell.C of the KDE konsole. 
 * Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de> 
 *
 * This is free software; you can use this library under the GNU Library 
 * General Public License, version 2. See the file "COPYING.LIB" for the 
 * exact licensing terms.
 *
 * process.cpp: Functionality to build a front end to password asking
 *  terminal programs.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>

#if defined(__SVR4) && defined(sun)
#include <stropts.h>
#include <sys/stream.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>		// Needed on some systems.
#endif

#include <qglobal.h>
#include <q3cstring.h>
#include <qfile.h>

#include <kdebug.h>
#include <kstandarddirs.h>

#include "process.h"
#include <kdesu/kdesu_pty.h>
#include <kdesu/kcookie.h>


MyPtyProcess::MyPtyProcess()
{
    m_bTerminal = false;
    m_bErase = false;
    m_pPTY = 0L;
    m_Pid = -1;
    m_Fd = -1;
}


int MyPtyProcess::init()
{
    delete m_pPTY;
    m_pPTY = new PTY();
    m_Fd = m_pPTY->getpt();
    if (m_Fd < 0)
	return -1;
    if ((m_pPTY->grantpt() < 0) || (m_pPTY->unlockpt() < 0)) 
    {
	kError(PTYPROC) << k_lineinfo << "Master setup failed.\n" << endl;
	m_Fd = -1;
	return -1;
    }
    m_TTY = m_pPTY->ptsname();
    m_stdoutBuf.resize(0);
    m_stderrBuf.resize(0);
    m_ptyBuf.resize(0);
    return 0;
}


MyPtyProcess::~MyPtyProcess()
{
    delete m_pPTY;
}
    

/*
 * Read one line of input. The terminal is in canonical mode, so you always
 * read a line at at time, but it's possible to receive multiple lines in
 * one time.
 */


Q3CString MyPtyProcess::readLineFrom(int fd, Q3CString& inbuf, bool block)
{
    int pos;
    Q3CString ret;

    if (!inbuf.isEmpty())
    {

        pos = inbuf.indexOf('\n');
        
        if (pos == -1) 
	{
	    ret = inbuf;
	    inbuf.resize(0);
	} else
	{
	    ret = inbuf.left(pos);
	    inbuf = inbuf.mid(pos+1);
	}
	return ret;

    }

    int flags = fcntl(fd, F_GETFL);
    if (flags < 0) 
    {
	kError(PTYPROC) << k_lineinfo << "fcntl(F_GETFL): " << perror << "\n";
	return ret;
    }
    if (block)
	flags &= ~O_NONBLOCK;
    else
	flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0)
    {
	kError(PTYPROC) << k_lineinfo << "fcntl(F_SETFL): " << perror << "\n";
	return ret;
    }

    int nbytes;
    char buf[256];
    while (1) 
    {
	nbytes = read(fd, buf, 255);
	if (nbytes == -1) 
	{
	    if (errno == EINTR)
		continue;
	    else break;
	}
	if (nbytes == 0)
	    break;	// eof

	buf[nbytes] = '\000';
	inbuf += buf;

	pos = inbuf.indexOf('\n');
        if (pos == -1) 
	{
	    ret = inbuf;
	    inbuf.resize(0);
	} else 
	{
	    ret = inbuf.left(pos);
	    inbuf = inbuf.mid(pos+1);
	}
	break;

    }

    return ret;
}

void MyPtyProcess::writeLine(Q3CString line, bool addnl)
{
    if (!line.isEmpty())
	write(m_Fd, line, line.length());
    if (addnl)
	write(m_Fd, "\n", 1);
}

void MyPtyProcess::unreadLineFrom(Q3CString inbuf, Q3CString line, bool addnl)
{
    if (addnl)
	line += '\n';
    if (!line.isEmpty())
	inbuf.prepend(line);
}


/*
 * Fork and execute the command. This returns in the parent.
 */

int MyPtyProcess::exec(Q3CString command, QCStringList args)
{
  kDebug(PTYPROC) << "MyPtyProcess::exec(): " << command << endl;// << ", args = " << args << endl;

    if (init() < 0)
        return -1;

    // Open the pty slave before forking. See SetupTTY()
    int slave = open(m_TTY, O_RDWR);
    if (slave < 0) 
    {
        kError(PTYPROC) << k_lineinfo << "Could not open slave pty.\n";
        return -1;
    } 

    // Also create a socket pair to connect to standard in/out.
    // This will allow use to bypass the terminal.
    int inout[2];
    int err[2];
    int ok = 1;
    ok &= socketpair(AF_UNIX, SOCK_STREAM, 0, inout) >= 0;
    ok &= socketpair(AF_UNIX, SOCK_STREAM, 0, err  ) >= 0;
    if( !ok ) {
        kDebug(PTYPROC) << "Could not create socket" << endl;
        return -1;
    }
    m_stdinout = inout[0];
    m_err = err[0];

    if ((m_Pid = fork()) == -1) 
    {
        kError(PTYPROC) << k_lineinfo << "fork(): " << perror << "\n";
        return -1;
    } 

    // Parent
    if (m_Pid) 
    {
	    close(slave);
    	close(inout[1]);
	    close(err[1]);
    	return 0;
    }

    // Child
	
    ok = 1;
    ok &= dup2(inout[1], STDIN_FILENO)  >= 0;
    ok &= dup2(inout[1], STDOUT_FILENO) >= 0;
    ok &= dup2(err[1],   STDERR_FILENO) >= 0;

    if( !ok )
    {
        kError(PTYPROC) << "dup of socket descriptor failed" << endl;
        _exit(1);
    }

    close(inout[1]);
    close(inout[0]);
    close(err[1]);
    close(err[0]);

    if (SetupTTY(slave) < 0)
	    _exit(1);

    // From now on, terminal output goes through the tty.
    Q3CString path;
    if (command.contains('/'))
	    path = command;
    else 
    {
	    QString file = KStandardDirs::findExe(command);
    	if (file.isEmpty())
	    {
	        kError(PTYPROC) << k_lineinfo << command << " not found\n";
    	    _exit(1);
	    }
    	path = QFile::encodeName(file);
    }

    int i;
    const char * argp[32];
    argp[0] = path;
    QCStringList::Iterator it;
    for (i=1, it=args.begin(); it!=args.end() && i<31; it++) {
    	argp[i++] = *it;
    	kDebug(PTYPROC) << *it << endl;
    }
    argp[i] = 0L;
    execv(path, (char * const *)argp);
    kError(PTYPROC) << k_lineinfo << "execv(\"" << path << "\"): " << perror << "\n";
    _exit(1);
    return -1; // Shut up compiler. Never reached.
}

/*
 * Wait until the terminal is set into no echo mode. At least one su 
 * (RH6 w/ Linux-PAM patches) sets noecho mode AFTER writing the Password: 
 * prompt, using TCSAFLUSH. This flushes the terminal I/O queues, possibly 
 * taking the password  with it. So we wait until no echo mode is set 
 * before writing the password.
 * Note that this is done on the slave fd. While Linux allows tcgetattr() on
 * the master side, Solaris doesn't.
 */

int MyPtyProcess::WaitSlave()
{
    int slave = open(m_TTY, O_RDWR);
    if (slave < 0) 
    {
	kError(PTYPROC) << k_lineinfo << "Could not open slave tty.\n";
	return -1;
    }

    struct termios tio;
    struct timeval tv;
    while (1) 
    {
	if (tcgetattr(slave, &tio) < 0) 
	{
	    kError(PTYPROC) << k_lineinfo << "tcgetattr(): " << perror << "\n";
	    close(slave);
	    return -1;
	}
	if (tio.c_lflag & ECHO) 
	{
	    kDebug(PTYPROC) << k_lineinfo << "Echo mode still on." << endl;
	    // sleep 1/10 sec
	    tv.tv_sec = 0; tv.tv_usec = 100000;
	    select(slave, 0L, 0L, 0L, &tv);
	    continue;
	}
	break;
    }
    close(slave);
    return 0;
}


int MyPtyProcess::enableLocalEcho(bool enable)
{
    int slave = open(m_TTY, O_RDWR);
    if (slave < 0) 
    {
	kError(PTYPROC) << k_lineinfo << "Could not open slave tty.\n";
	return -1;
    }
    struct termios tio;
    if (tcgetattr(slave, &tio) < 0) 
    {
	kError(PTYPROC) << k_lineinfo << "tcgetattr(): " << perror << "\n";
	close(slave); return -1;
    }
    if (enable)
	tio.c_lflag |= ECHO;
    else
	tio.c_lflag &= ~ECHO;
    if (tcsetattr(slave, TCSANOW, &tio) < 0) 
    {
	kError(PTYPROC) << k_lineinfo << "tcsetattr(): " << perror << "\n";
	close(slave); return -1;
    }
    close(slave);
    return 0;
}


/*
 * Copy output to stdout until the child process exists, or a line of output
 * matches `m_Exit'.
 * We have to use waitpid() to test for exit. Merely waiting for EOF on the
 * pty does not work, because the target process may have children still
 * attached to the terminal.
 */

int MyPtyProcess::waitForChild()
{
    int ret, state, retval = 1;
    struct timeval tv;

    fd_set fds;
    FD_ZERO(&fds);

    while (1) 
    {
	tv.tv_sec = 1; tv.tv_usec = 0;
	FD_SET(m_Fd, &fds);
	ret = select(m_Fd+1, &fds, 0L, 0L, &tv);
	if (ret == -1) 
	{
	    if (errno == EINTR) continue;
	    else 
	    {
		kError(PTYPROC) << k_lineinfo << "select(): " << perror << "\n";
		return -1;
	    }
	}

	if (ret) 
	{
	    Q3CString line = readLine(false);
	    while (!line.isNull()) 
	    {
		if (!m_Exit.isEmpty() && !qstrnicmp(line, m_Exit, m_Exit.length()))
		    kill(m_Pid, SIGTERM);
		if (m_bTerminal) 
		{
		    fputs(line, stdout);
		    fputc('\n', stdout);
		}
		line = readLine(false);
	    }
	}

	// Check if the process is still alive
	ret = waitpid(m_Pid, &state, WNOHANG);
	if (ret < 0) 
	{
	    if (errno == ECHILD)
		retval = 0;
	    else
		kError(PTYPROC) << k_lineinfo << "waitpid(): " << perror << "\n";
	    break;
	}
	if (ret == m_Pid) 
	{
	    if (WIFEXITED(state))
		retval = WEXITSTATUS(state);
	    break;
	}
    }

    return -retval;
}
   
/*
 * SetupTTY: Creates a new session. The filedescriptor "fd" should be
 * connected to the tty. It is closed after the tty is reopened to make it
 * our controlling terminal. This way the tty is always opened at least once
 * so we'll never get EIO when reading from it.
 */

int MyPtyProcess::SetupTTY(int fd)
{    
    // Reset signal handlers
    for (int sig = 1; sig < NSIG; sig++)
	signal(sig, SIG_DFL);
    signal(SIGHUP, SIG_IGN);

    // Close all file handles
//    struct rlimit rlp;
//    getrlimit(RLIMIT_NOFILE, &rlp);
//    for (int i = 0; i < (int)rlp.rlim_cur; i++)
//    if (i != fd) close(i);

    // Create a new session.
    setsid();

    // Open slave. This will make it our controlling terminal
    int slave = open(m_TTY, O_RDWR);
    if (slave < 0) 
    {
	kError(PTYPROC) << k_lineinfo << "Could not open slave side: " << perror << "\n";
	return -1;
    }
    close(fd);

#if defined(__SVR4) && defined(sun)

    // Solaris STREAMS environment.
    // Push these modules to make the stream look like a terminal.
    ioctl(slave, I_PUSH, "ptem");
    ioctl(slave, I_PUSH, "ldterm");

#endif

    // Connect stdin, stdout and stderr
//    dup2(slave, 0); dup2(slave, 1); dup2(slave, 2);
//    if (slave > 2)
//	close(slave);

    // Disable OPOST processing. Otherwise, '\n' are (on Linux at least)
    // translated to '\r\n'.
    struct termios tio;
    if (tcgetattr(slave, &tio) < 0)
    {
	kError(PTYPROC) << k_lineinfo << "tcgetattr(): " << perror << "\n";
	return -1;
    }
    tio.c_oflag &= ~OPOST;
    if (tcsetattr(slave, TCSANOW, &tio) < 0)
    {
	kError(PTYPROC) << k_lineinfo << "tcsetattr(): " << perror << "\n";
	return -1;
    }

    return 0;
}
