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

	string s = "tmp";
	s += '\0';
	s += "ppp";

cout << s << endl;
cout << s.size() << endl;

	   fs_create("user1", "password1", session, seq++, s.c_str());


    //fs_append("user1", "password1", session, seq++, "tmp", "ab34c", 5);
    //fs_read("user1", "password1", session, seq++, "tmp", 1, buf, 2);
    //fs_delete("user1", "password1", session, seq++, "tmp");





}
