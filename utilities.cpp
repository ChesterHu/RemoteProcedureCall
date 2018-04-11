#include "globals.h"
#include "utilities.h"
#include <string>
// HELPER FUNCTIONS FOR RPC
////////////////////////////////////////////////////////////////////////////////////////////////////



void parseMsg(char *msg, long long *length, int *type)
{
	memcpy(length, msg, sizeof(long long));
	memcpy(type, msg + sizeof(long long), sizeof(int));
}



void msgParse(char* msg, long long length, int type)
{
	memcpy(msg, &length, sizeof(long long));
	memcpy(msg + sizeof(long long), &type, sizeof(int));
}



std::string parseArgType(const int* argType, int* typeSize, int* argSize)
{
	*argSize = (*argType & 0xffff);  // last two bytes specify argument's size
	if (*argSize == 0)
		*argSize = 1;
	int type = (*argType >> 16) & 0xff;  // second byte specify argument type
	std::string argTypeName = "";
	  // find size of type
	switch (type)
	{
		case ARG_CHAR   : *typeSize = sizeof(char);
						  argTypeName = "char";
						  break;
		case ARG_SHORT  : *typeSize = sizeof(short);
						  argTypeName = "short";
						  break;
		case ARG_INT    : *typeSize = sizeof(int);
						  argTypeName = "int";
						  break;
		case ARG_LONG   : *typeSize = sizeof(long);
						  argTypeName = "long";
						  break;
		case ARG_DOUBLE : *typeSize = sizeof(double);
						  argTypeName = "double";
						  break;
		case ARG_FLOAT  : *typeSize = sizeof(float);
						  argTypeName = "float";
						  break;
	}
	
	  // if is an array	
	if (*argSize > 1)
		argTypeName += '*';

	return argTypeName;
}	



size_t argsToBytes(char *msg, const int *argTypes, void **args)
{
	int typeSize;
	int argSize;
	int argTypes_idx = 0;
	size_t numBytesWrite = 0;
	size_t toWrite = 0;	

	  //  argTypes
	while (argTypes[argTypes_idx] != 0)
		argTypes_idx++;
	toWrite = (argTypes_idx + 1) * sizeof(int);
	  // write argTypes
	memcpy(msg, argTypes, toWrite);
	numBytesWrite += toWrite;

	  // write arguments
	argTypes_idx = 0;
	while (argTypes[argTypes_idx] != 0)
	{
		  // get current arguement size and type
		parseArgType(&argTypes[argTypes_idx], &typeSize, &argSize);
		size_t toWrite = typeSize * argSize;

		  // copy
		memcpy(msg + numBytesWrite, args[argTypes_idx], toWrite);

		  // update
		argTypes_idx++;
		numBytesWrite += toWrite;
	}

	return numBytesWrite;
}



int bytesToArgs(char *msg, int **argTypes, void ***args) {
	int *arg   = reinterpret_cast<int*> (msg);
	int  count = 0;

	  // go to the start of args
	while( *arg != 0 ) {
		count++;
		arg++;
	}
	  // ATTENTION: DYNAMIC ALLOCATE
	*argTypes = new int[count + 1];
	*args     = new void *[count];
	
	char *arg_ptr  = reinterpret_cast<char*> (++arg);  // skip trailing zero
	int  *type_ptr = reinterpret_cast<int*> (msg);
	int   type_idx = 0;

	
	  // read args
	while (*type_ptr != 0)
	{
		int type;
		int typeSize;
		int argSize;

		  // copy argType
		memcpy( &type, type_ptr, sizeof(int) );
		(*argTypes)[type_idx] = type;
		parseArgType( &type, &typeSize, &argSize );

		  // copy arg
		int size = argSize * typeSize;
		(*args)[type_idx] = new char[size];
		memcpy( (*args)[type_idx], arg_ptr, size );

		  // update
		arg_ptr += size;
		type_ptr++;
		type_idx++;
	}
	
	  // trailing 0
	(*argTypes)[count] = 0;
	return count;
}




void binderToClient(int fd, int msgType, char *serverId, int *portId)
{
	//                     message length     message type   server id     '\0' port id
	long long msgLength = sizeof(long long) + sizeof(int) + HOST_NAME_MAX + 1 + sizeof(int);
	int offset  = 0;
	
	if (msgType == LOC_FAILURE)
	{
		long long failMsgLength = sizeof(long long) + sizeof(int) + sizeof(int);
		char failMsg[failMsgLength];
		int reasonCode = 10;  // TODO replace it by an input reasonCode

		memcpy(failMsg, &failMsgLength, sizeof(long long));  offset += sizeof(long long);
		memcpy(failMsg + offset, &msgType, sizeof(int));     offset += sizeof(int);
		memcpy(failMsg + offset, &reasonCode, sizeof(int));

		send(fd, failMsg, failMsgLength, 0);
		return;  // TODO send reason code to client
	}

	  // if success locate a server
	  // set up buffers
	char *msg         = new char[msgLength];
	char *serverIdBuf = new char[HOST_NAME_MAX + 1];          // 1 for the terminator
	memcpy(serverIdBuf, serverId, strlen(serverId) + 1);      // 1 for the terminator


	  // write message's length
	memcpy(msg + offset, &msgLength, sizeof(long long));   offset += sizeof(long long);
	memcpy(msg + offset, &msgType, sizeof(int));           offset += sizeof(int);
	memcpy(msg + offset, serverIdBuf, HOST_NAME_MAX + 1);  offset += HOST_NAME_MAX + 1;
	memcpy(msg + offset, portId, sizeof(int));


	  // send message
	send(fd, msg, msgLength, 0);

	delete [] msg;
	delete [] serverIdBuf;
}




int clientRecvBinder(int fd, char *serverId, int *portId)
{
	  // set up vars for receiving message from binder
	long long msgLength;
	int       bytesRead;
	int       offset;
	
	int       msgType;
	char     *buffer;

	  // allocate buffer to receive message length and message type
	msgLength = sizeof(long long) + sizeof(int);
	buffer    = new char[msgLength];

	  // start receiving msgLenght and msgType
	if ((bytesRead = readToBuffer(fd, buffer, msgLength)) < msgLength)  // fail to get full message lenght and message type
	{
		delete [] buffer;
		return -1;
	}

	  // read msgLength and msgTye from buffer
	offset = 0;
	memcpy(&msgLength, buffer + offset, sizeof(long long));  offset += sizeof(long long);
	memcpy(&msgType, buffer + offset, sizeof(int));          offset += sizeof(int);
	  // update msgLength left
	msgLength -= bytesRead;

	  // done with this buffer, reset bytesRead and offset
	delete [] buffer;

	  // if loc fail, read reason code
	if (msgType != LOC_SUCCESS)
	{
		  // allocate buffer to read reason code
		buffer = new char[msgLength];
		if (readToBuffer(fd, buffer, msgLength) < sizeof(int))  // fail to get reason code
		{
			delete [] buffer;
			return -1;
		}
		  // read reason code from buffer
		int reasonCode;
		memcpy(&reasonCode, buffer, sizeof(int));
		  // done with this buffer
		delete [] buffer;
		return reasonCode;
	}
	
	  // if loc success, read serverId, port Id
	buffer = new char[msgLength];
	
	if (readToBuffer(fd, buffer, msgLength) < msgLength)  // fail to get full message
	{
		delete [] buffer;
		return -1;
	}
	  // read server id and port id from buffer
	offset = 0;
	memcpy(serverId, buffer + offset, 65);                   offset += 65;
	memcpy(portId, buffer + offset, sizeof(int));
	
	  // done with this buffer
	delete [] buffer;
	
	return 0;	
}



void sendArgs(int fd, int msgType, const char *name, const int *argTypes, void **args)
{
	  // compute message's length
	long long msgLength = sizeof(long long) + sizeof(int) + 65 + sizeof(int);  // message length + message type + name + terminate 0;

	for (int i = 0; argTypes[i] != 0; ++i)
	{
		int typeSize; int argSize;
		parseArgType(&argTypes[i], &typeSize, &argSize);
		msgLength += typeSize * argSize + sizeof(int);
	}
	
	  // write message for server
	char *msgBuf = new char[msgLength];
	int offset = 0;
	memcpy(msgBuf, &msgLength, sizeof(long long));    offset += sizeof(long long);
	memcpy(msgBuf + offset, &msgType, sizeof(int));   offset += sizeof(int);
	memcpy(msgBuf + offset, name, strlen(name) + 1);  offset += 65;
	argsToBytes(msgBuf + offset, argTypes, args);
	
	  // send message to server
	send(fd, msgBuf, msgLength, 0);
	
	delete [] msgBuf;
}



int recvArgs(int fd, int *msgType, char *name, int **argTypes, void ***args)
{
	long long msgLength;
	char     *buffer;
	int       offset;
	  // read message length and message type
	msgLength = sizeof(long long) + sizeof(int);
	buffer = new char[msgLength];
	if(readToBuffer(fd, buffer, msgLength) < msgLength)
	{
		delete [] buffer;
		return -1;
	}
	
	memcpy(&msgLength, buffer, sizeof(long long));
	memcpy(msgType, buffer + sizeof(long long), sizeof(int));
	
	  // done with this buffer
	delete [] buffer;

	
	msgLength = msgLength - sizeof(long long) - sizeof(int);
	
	if (*msgType != EXECUTE || msgLength < 0) return -1;

	buffer = new char[msgLength];
	  // parse function name, argTypes, args
	if (readToBuffer(fd, buffer, msgLength) < msgLength)
	{
		delete [] buffer;
		return -1;
	}

	offset = 0;
	memcpy(name, buffer + offset, 65);  offset += 65;
	
	int res = bytesToArgs(buffer + offset, argTypes, args);

	delete [] buffer;
	return res;
}


  // connectTo binder or server, if server_addr is given, connect to given server, otherwise connect to binder
int connectTo(addrinfo** serverinfo, const char *server_addr, const int *portId)
{
	*serverinfo = nullptr;
	  // set up hints to find binder
	addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	
	  // get binder address and port number, TODO: read from serverId & portId
	const char *to_addr;
	const char *to_port;
	if (server_addr != nullptr)
	{
		to_addr = server_addr;
		to_port = std::to_string(*portId).c_str();
	}
	else
	{
		to_addr = getenv("BINDER_ADDRESS");
		to_port = getenv("BINDER_PORT");
	}

	  // get binder/server information
	getaddrinfo(to_addr, to_port, &hints, serverinfo);

	addrinfo *p;
	int sockfd;
	for (p = *serverinfo; p != nullptr; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
			continue;
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			continue;
		}
		break;
	}

	freeaddrinfo(*serverinfo);	
	if (p == nullptr)
		return -1;
	return sockfd;
}


std::string signatureCreate(const int *argTypes ) {
	std::string ret = "(";
	int dummy1 = 0;
	int dummy2 = 0;
	for( int i = 0; argTypes[i] != 0; i++ ) {
		int in_out = ( argTypes[i] >> ARG_OUTPUT ) & 3;
		ret += std::to_string( in_out ) + parseArgType( &argTypes[i], &dummy1, &dummy2 );
		ret += ',';
	}
	ret.back() = ')';
	return ret;
}


void clearArgs(int *argTypes, void **args)
{
	if (argTypes == nullptr)
		return;

	int idx = 0;
	while (argTypes[idx] != 0)
	{
		int type = (argTypes[idx] >> 16) & 0xff;
		switch (type)
		{
			case ARG_CHAR   : delete [] (char*) args[idx];
							  break;
			case ARG_SHORT  : delete [] (short*) args[idx];
							  break;
			case ARG_INT    : delete [] (int*) args[idx];
							  break;
			case ARG_LONG   : delete [] (long long*) args[idx];
							  break;
			case ARG_DOUBLE : delete [] (double*) args[idx];
							  break;
			case ARG_FLOAT  : delete [] (float*) args[idx];
		}
		  // update
		idx++;
	}
	delete [] argTypes;
}



int recvServer(int fd, char *name, int *argTypes, void **args)
{
	long long  msgLength;
	long long  bytesRead;
	int        msgType;
	int        reasonCode;
	int        numArgs;
	int        offset;
	char      *buffer;

	  // read message length and message type
	msgLength = sizeof(long long) + sizeof(int);
	  // allocate buffer
	buffer = new char[msgLength];
	  // read from file descriptor
	if ((bytesRead = readToBuffer(fd, buffer, msgLength)) < msgLength)  // fail to get full message length and message type
	{
		delete [] buffer;
		return -1;
	}
	  // read from buffer
	offset = 0;
	memcpy(&msgLength, buffer + offset, sizeof(long long));  offset += sizeof(long long);
	memcpy(&msgType, buffer + offset, sizeof(int));

	  // done with this buffer
	delete [] buffer;

	  // update message length left
	msgLength -= bytesRead;
	if (msgLength < sizeof(int))  // minimum size of message left
	{
		return -1;
	}
	
	  // get number of arguments
	numArgs = 0;
	while (argTypes[numArgs] != 0) ++numArgs;
	
	  // read left message	
	if (msgType == EXECUTE_SUCCESS)
	{
		  // allocate buffer for rpc execute result
		buffer = new char[msgLength];
		  // read from file descriptor
		if (readToBuffer(fd, buffer, msgLength) < msgLength)  // fail to get full message
		{
			delete [] buffer;
			return -1;
		}
		  // read from buffer
		offset = 0;
		  // read name
		char dump[65];
		memcpy(dump, buffer, strlen(name) + 1);  offset += 65;
		  // read argTypes
		for (int i = 0; i < numArgs + 1; ++i)
		{
			memcpy(&argTypes[i], buffer + offset, sizeof(int));
			offset += sizeof(int);
		}
		  // read args
		for (int i = 0; i < numArgs; ++i)
		{
			  // get argument size
			int typeSize, argSize;
			parseArgType(&argTypes[i], &typeSize, &argSize);
			  // read one arguemnt from buffer
			memcpy(args[i], buffer + offset, typeSize * argSize);
			offset += typeSize * argSize;
		}
		  // set reasonCode as 0(success)
		reasonCode = 0;
	}  // argTypes and args read end
	else
	{
		  // receive failure reason code
		  // allocate buffer for reason code
		buffer = new char[sizeof(int)];
		if (readToBuffer(fd, buffer, sizeof(int)) < sizeof(int))
		{
			delete [] buffer;
			return -1;
		}
		  // read reason code from buffer
		memcpy(&reasonCode, buffer, sizeof(int));

	}  // read message end
	
	  // done with this buffer
	delete [] buffer;
	  // done with this server
	return reasonCode;
}



int readToBuffer(int fd, char *buffer, long long msgLength)
{
	long long bytesRead = 0;

	do
	{
		int recvResult = recv(fd, buffer + bytesRead, msgLength - bytesRead, 0);
		if (recvResult <= 0)
			return -1;
		bytesRead += recvResult;
	} while (bytesRead < msgLength);

	return bytesRead;
}
