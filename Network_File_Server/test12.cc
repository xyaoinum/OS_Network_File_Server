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
		char buf[124*512];
		if(fs_read("user1", "password1", session, seq++, s.c_str(), 0, buf, 0)==0){
			cout << "gppasd" << endl;
		}
		if(fs_read("user1", "password1", session, seq++, s.c_str(), 0, buf, 512)==0){
			cout << "gppasd" << endl;
		}
		if(fs_read("user1", "password1", session, seq++, s.c_str(), 0, buf, 0)==0){
			cout << "gppasd" << endl;
		}
		if(fs_read("user1", "password1", session, seq++, s.c_str(), 511, buf, 0)==0){
			cout << "gppasd" << endl;
		}
		if(fs_read("user1", "password1", session, seq++, s.c_str(), 511, buf, 1)==0){
			cout << "gppasd" << endl;
		}
		if(fs_read("user1", "password1", session, seq++, s.c_str(), 511, buf, 512*10)==0){
			cout << "gppasd" << endl;
		}
	}

}










