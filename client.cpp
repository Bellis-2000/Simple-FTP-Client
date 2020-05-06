/* -----------------------------------
 *	      A Simple FTP Client
 *          Author: Nicode
 *     Computer Network Experiment
 * ----------------------------------- */

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <cstring>
#include <string>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

#ifdef WINDOWS
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif

#define MAXLINE 4096

typedef struct
{
	char *servip;
	struct sockaddr_in *servaddr;
	char *port;
} ConnInfo;

ConnInfo *info;

// Judge port is legal or not.
inline bool isLegalPort(int);

// The dialog shows when local/server already has file with the same name.
bool confirmOverwrite();

// Process arguments pass to main().
void argument_process(int, char **);

// Create the initial socket and connect to server.
int create_initial_socket();

// Create FTP Passive Mode data port socket and connect.
int create_socket(int, char *);

// Mainframe to input FTP commands.
int command_input(int);

// Server login by username and password.
void user_login(int);

// Enter FTP Passive Mode, and get data socket.
int get_pasv_socket(int);

// Get response from server, and display the content.
string get_response(int, int);

int main(int argc, char **argv)
{
	int sockfd = -1;
	try
	{
		argument_process(argc, argv);
		sockfd = create_initial_socket();
		user_login(sockfd);
		command_input(sockfd);
	}
	catch (const char *err)
	{
		cerr << err << endl;
		exit(1);
	}

	if (sockfd != -1)
		close(sockfd);
	delete info;
	return 0;
}

inline bool isLegalPort(int port)
{
	return port >= 1 && port <= 65535;
}

bool confirmOverwrite()
{
	bool overwrite = false;
	for (;;)
	{
		cout << "Remote file already existed." << endl
			 << "Proceed to overwrite? [y/N]:";
		string confirm;
		getline(cin, confirm);
		if (confirm.length() > 1)
		{
			cerr << "Invalid sentence [y/N]" << endl;
			continue;
		}
		else if (confirm.length() == 1)
		{
			if (confirm == "y" || confirm == "Y")
			{
				overwrite = true;
				break;
			}
			else if (confirm == "n" || confirm == "N")
			{
				overwrite = false;
				break;
			}
			else
			{
				cerr << "Invalid sentence [y/N]" << endl;
				continue;
			}
		}
		else
		{
			overwrite = false;
			break;
		}
	}
	return overwrite;
}

void argument_process(int argc, char **argv)
{
	if (argc != 3)
		throw "Invalid argument count\nUsage: ./client [server IP] [port]";

	char *servip = argv[1];
	int servport_int = atoi(argv[2]);
	char *servport = argv[2];

	if (!isLegalPort(servport_int))
		throw "Invalid destination port";

	info = (ConnInfo *)calloc(1, sizeof(ConnInfo));
	if (info == NULL)
		throw "Can't allocate info";

	info->servaddr = (struct sockaddr_in *)calloc(1, sizeof(struct sockaddr_in));
	if (info->servaddr == NULL)
		throw "Can't allocate servaddr";

	info->servip = servip;
	info->port = servport;
	info->servaddr->sin_family = AF_INET;
	info->servaddr->sin_addr.s_addr = inet_addr(servip);
	info->servaddr->sin_port = htons(atoi(servport));

	return;
}

int create_initial_socket()
{
	int sockfd;

	// create a socket for the client
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		throw "Can't create initial client socket";

	// connect initial socket to server
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = info->servaddr->sin_family;
	servaddr.sin_addr.s_addr = info->servaddr->sin_addr.s_addr;
	servaddr.sin_port = info->servaddr->sin_port;
	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
		throw "Can't connect initial socket to server";

	return sockfd;
}

int command_input(int sockfd)
{
	int data_sock;
	char send_buf[MAXLINE], recvline[MAXLINE];
	string result;

	for (;;)
	{
		cout << "ftp> ";
		if (fgets(send_buf, MAXLINE, stdin) == NULL)
			break;

		string sendline = send_buf;
		sendline = sendline.find('\n') != string::npos ? sendline.substr(0, sendline.find('\n')) : sendline;
		string command = sendline.find(' ') != string::npos ? sendline.substr(0, sendline.find(' ')) : sendline;

		if (sendline.length() == 0)
			continue;

		if (command == "quit" || command == "QUIT")
		{
			write(sockfd, send_buf, strlen(send_buf));
			result = get_response(sockfd, MAXLINE);
			close(sockfd);
			return 0;
		}
		else if (command == "pwd" || command == "PWD")
		{
			write(sockfd, send_buf, strlen(send_buf));
			result = get_response(sockfd, MAXLINE);
		}
		else if (command == "!pwd" || command == "!PWD")
		{
			system("pwd");
		}
		else if (command == "cd" || command == "CWD")
		{
			string argument = sendline.find(' ') != string::npos ? sendline.substr(sendline.find(' ') + 1) : "";
			if (argument == "")
			{
				cout << "Argument error" << endl
					 << "Usage: cwd <dir>" << endl;
				continue;
			}
			sprintf(send_buf, "CWD %s\r\n", argument.c_str());
			write(sockfd, send_buf, strlen(send_buf));
			result = get_response(sockfd, MAXLINE);
		}
		else if (command == "!cd" || command == "!CWD")
		{
			string argument = sendline.find(' ') != string::npos ? sendline.substr(sendline.find(' ') + 1) : "";
			if (argument == "")
			{
				cout << "Argument error" << endl
					 << "Usage: !cwd <dir>";
				continue;
			}

			if (chdir(argument.c_str()) < 0)
			{
				cerr << "Path you entered doesn't exist" << endl
					 << "Please try again" << endl;
				continue;
			}

			system(("cd " + argument).c_str());
			system("pwd");
			cout << endl;
		}
		else if (command == "ls" || command == "LIST")
		{
			data_sock = get_pasv_socket(sockfd);

			sprintf(send_buf, "LIST\r\n");
			write(sockfd, send_buf, strlen(send_buf));
			result = get_response(sockfd, MAXLINE);

			for (;;)
			{
				result = get_response(data_sock, MAXLINE);
				if (result.length() == 0)
					break;
			}

			close(data_sock);
			result = get_response(sockfd, MAXLINE);
		}
		else if (command == "!ls" || command == "!LIST")
		{
			system("ls");
		}
		else if (command == "get" || command == "RETR")
		{
			string filename = sendline.find(' ') != string::npos ? sendline.substr(sendline.find(' ') + 1) : "";
			int filesize;
			FILE *IN;
			char buff[MAXLINE];

			if (filename == "")
			{
				cout << "Filename can't be blank" << endl
					 << "Usage: get <filename>" << endl;
				continue;
			}

			// Judge if there is already a local file
			ifstream f(filename);
			if (f.good() && !confirmOverwrite())
				continue;

			data_sock = get_pasv_socket(sockfd);

			// Get file size
			sprintf(send_buf, "SIZE %s\r\n", filename.c_str());
			write(sockfd, send_buf, strlen(send_buf));
			result = get_response(sockfd, MAXLINE);

			string code = result.substr(0, 3);
			if (code == "550")
			{
				close(data_sock);
				continue;
			}
			else if (code == "213")
			{
				if ((IN = fopen(filename.c_str(), "w")) == NULL)
				{
					close(data_sock);
					throw "Can't create local file!";
				}

				filesize = stoi(result.substr(4));

				sprintf(send_buf, "RETR %s\r\n", filename.c_str());
				write(sockfd, send_buf, strlen(send_buf));
				result = get_response(sockfd, MAXLINE);

				int completed = 0;
				for (;;)
				{
					int cur = read(data_sock, buff, MAXLINE);
					fwrite(buff, sizeof(char), cur, IN);
					completed += cur;
					cout << completed << endl;
					if (completed >= filesize)
						break;
				}

				fclose(IN);
				close(data_sock);
				result = get_response(sockfd, MAXLINE);
			}
		}
		else if (command == "put" || command == "STOR")
		{
			string filename = sendline.find(' ') != string::npos ? sendline.substr(sendline.find(' ') + 1) : "";
			int filesize;
			FILE *OUT;
			char buff[MAXLINE];

			if (filename == "")
			{
				cout << "Filename can't be blank" << endl
					 << "Usage: put <filename>" << endl;
				continue;
			}

			ifstream f(filename);
			if (!f.good())
			{
				cerr << "Can't find local file" << endl;
				continue;
			}
			if ((OUT = fopen(filename.c_str(), "r")) == NULL)
			{
				close(data_sock);
				throw "Can't read local file!";
			}

			data_sock = get_pasv_socket(sockfd);

			fseek(OUT, 0, SEEK_END);
			filesize = ftell(OUT);
			fseek(OUT, 0, SEEK_SET);

			sprintf(send_buf, "SIZE %s\r\n", filename.c_str());
			write(sockfd, send_buf, strlen(send_buf));
			result = get_response(sockfd, MAXLINE);

			string code = result.substr(0, 3);
			if (code == "213" && !confirmOverwrite())
			{
				close(data_sock);
				continue;
			}

			sprintf(send_buf, "STOR %s\r\n", filename.c_str());
			write(sockfd, send_buf, strlen(send_buf));
			result = get_response(sockfd, MAXLINE);

			int completed = 0;
			for (;;)
			{
				int read_success = fread(buff, sizeof(char), MAXLINE, OUT);
				int cur = write(data_sock, buff, read_success);
				completed += cur;
				cout << completed << endl;
				if (completed >= filesize)
					break;
			}

			fclose(OUT);
			close(data_sock);
			result = get_response(sockfd, MAXLINE);
		}
		else if (command == "help" || command == "HELP")
		{
			cout << "FTP Client commands:\n\n";
			cout << " put  <filename>  --- Upload a file from local to server\n";
			cout << " get  <filename>  --- Download a file from server to local\n";
			cout << "  ls              --- List all files under the present directory of the server\n";
			cout << " !ls              --- List all files under the present directory of the client\n";
			cout << " pwd              --- Display the present working directory of the server\n";
			cout << "!pwd              --- Display the present working directory of the client\n";
			cout << "  cd  <directory> --- Change the present working directory of the server\n";
			cout << " !cd  <directory> --- Change the present working directory of the client\n";
			cout << "quit              --- Quit\n";
		}
		else
		{
			cerr << "Illegal command" << endl
				 << "Please check command" << endl
				 << "Type \"help\" for all commands." << endl;

			//            Test Command
			// write(sockfd, send_buf, strlen(send_buf));
			// result = get_response(sockfd, MAXLINE);
		}
	}
	return 0;
}

int create_socket(int port, char *addr)
{
	int sockfd;
	struct sockaddr_in servaddr;

	// create a socket for the client
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		throw "Can't create socket";

	// creation of the socket
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(addr);
	servaddr.sin_port = htons(port);

	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
		throw "Can't create data channel";

	return sockfd;
}

void user_login(int sockfd)
{
	string result, code = "000";
	char sendline[MAXLINE];

	// Welcome message
	result = get_response(sockfd, MAXLINE);

	// Input username
	while (code != "331")
	{
		string username;
		cout << "Name: ";
		getline(cin, username);
		if (username.length() > MAXLINE / 3)
			throw "Error: Username too long.";
		sprintf(sendline, "USER %s\r\n", username.c_str());
		write(sockfd, sendline, strlen(sendline));
		result = get_response(sockfd, MAXLINE);
		code = result.substr(0, 3);
	}

	// Input Password
	while (code != "230")
	{
		string password;
		cout << "Password: ";
		getline(cin, password);
		if (password.length() > MAXLINE / 3)
			throw "Error: password too long.";
		sprintf(sendline, "PASS %s\r\n", password.c_str());
		write(sockfd, sendline, strlen(sendline));
		result = get_response(sockfd, MAXLINE);
		code = result.substr(0, 3);
	}

	return;
}

string get_response(int sockfd, int len = MAXLINE)
{
	char recvline[MAXLINE];
	int status;

	if ((status = read(sockfd, recvline, len)) < 0)
		throw "Can't get response from server";

	string result = recvline;
	result = result.substr(0, status);
	cout << result;

	return result;
}

int get_pasv_socket(int sockfd)
{
	char send_buf[MAXLINE];
	string result;

	sprintf(send_buf, "PASV\r\n");
	write(sockfd, send_buf, strlen(send_buf));
	//receive data transfer port from server
	result = get_response(sockfd, MAXLINE);

	string temp = result.substr(0, result.rfind(','));
	int port_p1 = stoi(temp.substr(temp.rfind(',') + 1));
	int port_p2 = stoi(result.substr(result.rfind(',') + 1, result.rfind(')') - result.rfind(',') - 1));
	int data_port = port_p1 * 256 + port_p2;

	cout << "Passive data port: " << data_port << endl;
	if (!isLegalPort(data_port))
		throw "Invalid data transfer port";

	int data_sock = create_socket(data_port, info->servip);

	return data_sock;
}
