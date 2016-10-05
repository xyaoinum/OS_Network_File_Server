#include <iostream>
#include <cstdlib>
#include "fs_client.h"

using namespace std;

main(int argc, char *argv[])

{
	char *server;
	int server_port;
	unsigned int session, seq=0;          
	char buf[10];

	if (argc != 3) {
		cout << "error: usage: " << argv[0] << " <server> <serverPort>\n";
		exit(1);
	}
	server = argv[1];
	server_port = atoi(argv[2]);

	fs_clientinit(server, server_port);
	fs_session("user1", "password1", &session, seq++);
	fs_create("user1", "password1", session, seq++, "tmp");
	fs_append("user1", "password1", session, seq++, "tmp", "abc", 3);
	fs_append("user1", "password1", session, seq, "tmp", "abc", 3);
	fs_append("user1", "password1", session, seq, "tmp", "abc", 3);
	fs_append("user2", "password2", session, seq++, "tmp", "abcde", 4);
	fs_append("user3", "password3", session, seq++, "tmp", "abcfg", 4);
	fs_read("user1", "password1", session, seq++, "tmp", 1, buf, 2);
	fs_read("user2", "password2", session, seq++, "tmp", 1, buf, 2);
	fs_read("user2", "password1", session, seq++, "tmp", 1, buf, 2);
	fs_read("user3", "password1", session, seq++, "tmp", 1, buf, 2);
	//fs_delete("user1", "password1", session, seq++, "tmp");
	fs_session("user1", "password1", &session, seq);
	fs_session("user2", "password2", &session, seq);
	fs_create("user2", "password2", session, seq++, "tmp");
	fs_append("user2", "password2", session, seq++, "tmp", "abcde", 4);
	if(fs_create("user1", "password1", 0, seq++, "tmp2")== 0){
		cout << "good" << endl;
	}

}