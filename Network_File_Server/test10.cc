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


	string file512;
	for(int i = 0; i < 512; i++){
		file512.append(to_string(i%10));
	}

	
	string file124;
	for(int i = 0; i < 124; i++){
		file124+=file512;
	}


	for(int i = 0; i < 33; i++){
		string s = "tmp";
		s += to_string(i);
    		if(fs_create("user1", "password1", session, seq++, s.c_str())==0){
			cout << "success!" << endl;
		}
		if(fs_append("user1", "password1", session, seq++, s.c_str(), file124.c_str(), file124.size())==0){
			cout << "append success" << endl;
		}
	}
	for(int i = 0; i < 2; i++){
    		if(fs_create("user1", "password1", session, seq++, "wtf")==0){
			cout << "create success!" << endl;
		}
	}
	for(int i = 0; i < 34; i++){
		string s = "tmp";
		s += to_string(i);
		if(fs_delete("user1", "password1", session, seq++, s.c_str())==0){
			cout << "delete success" << endl;
		}
		if(fs_append("user1", "password1", session, seq++, s.c_str(), file124.c_str(), file124.size())==0){
			cout << "append success" << endl;
		}
	}


	fs_append("user1", "password1", session, seq++, "tmp", "abc", 3);
	fs_append("user1", "password1", session, seq, "tmp", "abc", 3);
	fs_append("user1", "password1", session, seq, "tmp", "abc", 3);
	fs_append("user2", "password2", session, seq++, "tmp", "abcde", 4);
	fs_append("user3", "password3", session, seq++, "tmp", "abcfg", 4);
	fs_read("user1", "password1", session, seq++, "tmp", 1, buf, 2);
	fs_read("user2", "password2", session, seq++, "tmp", 1, buf, 2);
	fs_read("user2", "password1", session, seq++, "tmp", 1, buf, 2);
	fs_read("user3", "password1", session, seq++, "tmp", 1, buf, 2);
	fs_delete("user1", "password1", session, seq++, "tmp");
}










